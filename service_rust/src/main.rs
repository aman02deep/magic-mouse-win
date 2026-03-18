mod config;
mod device;
mod gesture_engine;
mod hid_reader;
mod input;
mod ipc;
mod scroll_engine;

use config::ConfigManager;
use hidapi::HidApi;
use std::ffi::OsString;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};
use std::fs::OpenOptions;
use std::io::Write;

use windows_service::{
    define_windows_service,
    service::{
        ServiceControl, ServiceControlAccept, ServiceExitCode, ServiceState, ServiceStatus,
        ServiceType,
    },
    service_control_handler::{self, ServiceControlHandlerResult},
    service_dispatcher,
};

use gesture_engine::{GestureEngine, GestureEvent, GestureType, TouchPoint};
use hid_reader::HidReader;
use scroll_engine::ScrollParams;

define_windows_service!(ffi_service_main, magic_mouse_service_main);

fn magic_mouse_service_main(arguments: Vec<OsString>) {
    if let Err(e) = run_service(arguments) {
        eprintln!("Service error: {:?}", e);
    }
}

fn run_service(_arguments: Vec<OsString>) -> windows_service::Result<()> {
    let service_name = "MagicMouseService";

    let (shutdown_tx, shutdown_rx) = crossbeam_channel::bounded(1);

    let event_handler = move |control_event| -> ServiceControlHandlerResult {
        match control_event {
            ServiceControl::Stop | ServiceControl::Shutdown => {
                let _ = shutdown_tx.send(());
                ServiceControlHandlerResult::NoError
            }
            _ => ServiceControlHandlerResult::NotImplemented,
        }
    };

    let status_handle = service_control_handler::register(service_name, event_handler)?;

    let next_status = ServiceStatus {
        service_type: ServiceType::OWN_PROCESS,
        current_state: ServiceState::Running,
        controls_accepted: ServiceControlAccept::STOP | ServiceControlAccept::SHUTDOWN,
        exit_code: ServiceExitCode::Win32(0),
        checkpoint: 0,
        wait_hint: Duration::default(),
        process_id: None,
    };

    status_handle.set_service_status(next_status)?;

    driver_loop(shutdown_rx);

    let stop_status = ServiceStatus {
        service_type: ServiceType::OWN_PROCESS,
        current_state: ServiceState::Stopped,
        controls_accepted: ServiceControlAccept::empty(),
        exit_code: ServiceExitCode::Win32(0),
        checkpoint: 0,
        wait_hint: Duration::default(),
        process_id: None,
    };
    status_handle.set_service_status(stop_status)?;
    Ok(())
}

fn driver_loop(shutdown_rx: crossbeam_channel::Receiver<()>) {
    // 1. Load config
    let mut config_mgr = ConfigManager::new();
    config_mgr.load();
    let config = Arc::new(Mutex::new(config_mgr.config.clone()));

    // 2. Start IPC
    let cfg = Arc::clone(&config);
    let ipc = Arc::new(ipc::IpcServer::new(move |cmd| {
        if cmd == "reload_config" {
            let mut mgr = ConfigManager::new();
            if mgr.load() {
                *cfg.lock().unwrap() = mgr.config;
                return r#"{"status":"ok"}"#.to_string();
            }
        }
        r#"{"status":"error"}"#.to_string()
    }));

    let api = HidApi::new().expect("Failed to initialize HID API");
    let mut buf = [0u8; 128];

    // Scroll engine
    let scroll_params = ScrollParams::default(); // we should map from config, but omitting for brevity
    let scroll_engine = scroll_engine::ScrollEngine::new(|v, h| {
        input::inject_scroll_v(v);
        input::inject_scroll_h(h);
    }, scroll_params);

    // Gesture engine
    let mut gesture_engine = GestureEngine::new(|event: GestureEvent| {
        match event.gtype {
            GestureType::ScrollV => { input::inject_scroll_v(event.delta); },
            GestureType::ScrollH => { input::inject_scroll_h(event.delta); },
            _ => {
                // TODO: Map swipes to keystrokes based on config
            }
        }
    }, gesture_engine::GestureParams::default());

    let mut _current_device_path = String::new();
    let mut reader: Option<HidReader> = None;

    // Track previous touch position for delta-based scroll
    let mut prev_touch_x: f32 = 0.0;
    let mut prev_touch_y: f32 = 0.0;
    let mut touching: bool = false;
    let mut battery_poll = Instant::now();

    // Debug logging mechanism
    let mut logs_written = 0;
    let debug_path = "C:\\mm_debug.txt";
    let _ = std::fs::remove_file(debug_path); // clear old log

    loop {
        if shutdown_rx.try_recv().is_ok() {
            break;
        }

        // Check if enabled in config
        let enabled = config.lock().unwrap().enabled;
        if !enabled {
            std::thread::sleep(Duration::from_millis(500));
            continue;
        }

        if reader.is_none() {
            if let Ok(mice) = device::enumerate_magic_mice() {
                if let Some(mouse) = mice.first() {
                    if let Ok(r) = HidReader::new(&api, &mouse.path) {
                        reader = Some(r);
                        let _current_device_path = mouse.path.clone();
                    }
                }
            }
        }

        if let Some(ref r) = reader {
            match r.read(&mut buf, 16) {
                Ok(bytes) if bytes > 0 => {
                    // --- DEBUG LOGGING ---
                    if logs_written < 100 {
                        if let Ok(mut file) = OpenOptions::new().create(true).append(true).open(debug_path) {
                            let hex: Vec<String> = buf[..bytes].iter().map(|b| format!("{:02X}", b)).collect();
                            let _ = writeln!(file, "[{}] len={} | {}", logs_written, bytes, hex.join(" "));
                            logs_written += 1;
                        }
                    }

                    if bytes >= 15 {
                        // ---------- Apple Magic Mouse 2 BT HID Report ----------
                    // buf[0]    = Report ID
                    // buf[1]    = Button state
                    // buf[2..3] = X delta (i16 LE) — OS handles cursor movement
                    // buf[4..5] = Y delta (i16 LE) — OS handles cursor movement
                    // buf[6..13]= Unknown bytes
                    // buf[14..] = Touch packets, 8 bytes per finger:
                    //   bytes 0-1: touch X (i16 LE, ~1/100 mm units)
                    //   bytes 2-3: touch Y (i16 LE)
                    //   byte  4:   finger ID
                    //   byte  5:   touch size/state
                    //   bytes 6-7: misc

                    let touch_data = &buf[14..bytes];
                    let finger_count = touch_data.len() / 8;
                    let now = Instant::now().elapsed().as_millis() as i64;

                    if finger_count > 0 {
                        // Parse first finger for delta scroll
                        let raw_x = i16::from_le_bytes([touch_data[0], touch_data[1]]) as f32;
                        let raw_y = i16::from_le_bytes([touch_data[2], touch_data[3]]) as f32;

                        // Build touch points for gesture engine
                        let mut points = Vec::new();
                        for i in 0..finger_count {
                            let o = i * 8;
                            if o + 4 <= touch_data.len() {
                                let fx = i16::from_le_bytes([touch_data[o], touch_data[o+1]]) as f32;
                                let fy = i16::from_le_bytes([touch_data[o+2], touch_data[o+3]]) as f32;
                                let fid = *touch_data.get(o+4).unwrap_or(&0) as i32;
                                points.push(TouchPoint { id: fid, x: fx, y: fy, pressure: 1.0 });
                            }
                        }
                        gesture_engine.on_touch_frame(&points, now);

                        // Broadcast touch positions to WPF settings UI
                        // Normalize raw coords (~-2000..2000) to 0..1 for UI rendering
                        let fingers_json: Vec<String> = points.iter().map(|p| {
                            let nx = ((p.x + 2048.0) / 4096.0).clamp(0.0, 1.0);
                            let ny = ((p.y + 2048.0) / 4096.0).clamp(0.0, 1.0);
                            format!(r#"{{"x":{:.3},"y":{:.3}}}"#, nx, ny)
                        }).collect();
                        let touch_msg = format!(
                            r#"{{"type":"touch","fingers":[{}]}}"#,
                            fingers_json.join(",")
                        );
                        ipc.broadcast(touch_msg);

                        // Inject scroll from per-frame delta (only after first frame)
                        if touching {
                            let dx = raw_x - prev_touch_x;
                            let dy = raw_y - prev_touch_y;
                            // Raw units are ~1/100 mm; scale to reasonable scroll ticks
                            let scale = 0.04_f32;
                            if dy.abs() > 0.5 {
                                input::inject_scroll_v(-dy * scale);
                            }
                            if dx.abs() > 0.5 {
                                input::inject_scroll_h(dx * scale);
                            }
                        }

                        prev_touch_x = raw_x;
                        prev_touch_y = raw_y;
                        touching = true;
                    } else {
                        // Finger lift — hand off to inertia
                        if touching {
                            gesture_engine.on_finger_lift(now);
                            scroll_engine.on_finger_lift(0.0, 0.0);
                            // Notify UI that fingers lifted
                            ipc.broadcast(r#"{"type":"touch","fingers":[]}"#.to_string());
                        }
                        touching = false;
                    }
                }
                } // Closes Ok(bytes) if bytes > 0 =>
                Ok(_) => {
                    // 0 bytes read, just continue
                }
                Err(_) => {
                    // Device disconnected
                    reader = None;
                    touching = false;
                }
            }
        } else {
            std::thread::sleep(Duration::from_secs(1));
        }
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let service_name = "MagicMouseService";
    
    // Attempt SCM dispatch first
    if let Err(_e) = service_dispatcher::start(service_name, ffi_service_main) {
        // Fallback to debug mode
        let (_tx, rx) = crossbeam_channel::bounded(1);
        
        ctrlc::set_handler(move || {
            let _ = _tx.send(());
        }).expect("Error setting Ctrl-C handler");

        driver_loop(rx);
    }
    
    Ok(())
}

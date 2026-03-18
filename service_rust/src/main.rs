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
    let _ipc = ipc::IpcServer::new(move |cmd| {
        if cmd == "reload_config" {
            let mut mgr = ConfigManager::new();
            if mgr.load() {
                *cfg.lock().unwrap() = mgr.config;
                return r#"{"status":"ok"}"#.to_string();
            }
        }
        r#"{"status":"error"}"#.to_string()
    });

    let api = HidApi::new().expect("Failed to initialize HID API");
    let mut buf = [0u8; 64];

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
            match r.read(&mut buf, 100) {
                Ok(bytes) if bytes > 0 => {
                    // Very simple parsing of Apple BT Report (for Magic Mouse)
                    // Normally report[1] is states, report[4..] is touch packets
                    // This is vastly simplified for demonstration
                    
                    let mut points = Vec::new();
                    // Example touch mapping (pseudo format depending on MM1 / MM2):
                    // If touch count > 0, we populate points
                    
                    // The time is needed for the engines:
                    let now = Instant::now().elapsed().as_millis() as i64;

                    if buf[1] & 0x01 != 0 {
                        // Pretend we read 1 finger
                        points.push(TouchPoint { id: 1, x: buf[4] as f32, y: buf[5] as f32, pressure: 1.0 });
                        gesture_engine.on_touch_frame(&points, now);
                    } else {
                        gesture_engine.on_finger_lift(now);
                        scroll_engine.on_finger_lift(0.0, 0.0);
                    }
                }
                Ok(_) => {
                    // Timeout, keep looping
                }
                Err(_) => {
                    // Device disconnected
                    reader = None;
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

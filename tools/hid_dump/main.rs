/// HID dump utility - prints raw bytes from all Apple HID devices
/// Run this to figure out the real byte layout from your Magic Mouse.
/// Usage: cargo run --manifest-path tools/hid_dump/Cargo.toml

use hidapi::HidApi;
use std::fs::File;
use std::io::Write;

const APPLE_VID: u16 = 0x05AC;

fn main() {
    let api = HidApi::new().expect("Failed to init hidapi");

    println!("Scanning for Apple devices...\n");

    // Print all Apple devices so we can identify the right PID
    for dev in api.device_list() {
        if dev.vendor_id() == APPLE_VID {
            println!(
                "  Found: VID={:04X} PID={:04X} Usage={}/{} Path={} Name={:?}",
                dev.vendor_id(),
                dev.product_id(),
                dev.usage_page(),
                dev.usage(),
                dev.path().to_string_lossy(),
                dev.product_string()
            );
        }
    }

    // Try to open the first Apple device with usage page 0xFF00 (vendor-specific touch)
    // For Magic Mouse: usage_page=0xFF00, usage=0x0001  OR  usage_page=0x0001, usage=0x0002
    let target = api.device_list().find(|d| {
        d.vendor_id() == APPLE_VID
            && (d.usage_page() == 0xFF00 || d.usage_page() == 0x0001)
    });

    let dev_info = match target {
        Some(d) => d,
        None => {
            eprintln!("\nNo Apple touch device found! Is the Magic Mouse connected?");
            return;
        }
    };

    println!(
        "\nOpening: VID={:04X} PID={:04X} Usage={:04X}/{:04X}",
        dev_info.vendor_id(),
        dev_info.product_id(),
        dev_info.usage_page(),
        dev_info.usage()
    );

    let device = match api.open_path(dev_info.path()) {
        Ok(d) => d,
        Err(e) => {
            eprintln!("Failed to open device: {e}");
            return;
        }
    };

    device.set_blocking_mode(false).ok();

    let log_path = "C:\\magic_mouse_hid_dump.txt";
    let mut log = File::create(log_path).expect("Failed to create log file");
    println!("Logging to {log_path} - Move your finger on the mouse now...");
    println!("Press Ctrl+C to stop.\n");

    let mut buf = [0u8; 128];
    let mut count = 0;

    loop {
        match device.read_timeout(&mut buf, 100) {
            Ok(n) if n > 0 => {
                let hex: Vec<String> = buf[..n].iter().map(|b| format!("{b:02X}")).collect();
                let line = format!("[{count:04}] n={n:3} | {}\n", hex.join(" "));
                print!("{}", line);
                write!(log, "{}", line).ok();
                count += 1;
                if count >= 500 {
                    println!("Captured 500 reports, stopping.");
                    break;
                }
            }
            Ok(_) => {}
            Err(e) => {
                eprintln!("Read error: {e}");
                break;
            }
        }
    }

    println!("\nDone! Share {log_path} so we can decode the byte layout.");
}

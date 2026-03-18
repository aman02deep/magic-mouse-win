use hidapi::HidApi;

pub const APPLE_VENDOR_ID: u16 = 0x05AC;
pub const MAGIC_MOUSE_1_PID: u16 = 0x030D; // Bluetooth Classic
pub const MAGIC_MOUSE_2_PID: u16 = 0x0269; // BLE

#[derive(Debug, Clone)]
pub struct DeviceInfo {
    pub vendor_id: u16,
    pub product_id: u16,
    pub serial: String,
    pub path: String,
    pub model_name: String,
}

pub fn enumerate_magic_mice() -> Result<Vec<DeviceInfo>, hidapi::HidError> {
    // HidApi::new() initializes the underlying hid_init() C call safely
    let api = HidApi::new()?;
    let mut results = Vec::new();

    for device in api.device_list() {
        if device.vendor_id() == APPLE_VENDOR_ID {
            let pid = device.product_id();
            if pid == MAGIC_MOUSE_1_PID || pid == MAGIC_MOUSE_2_PID {
                let model_name = match pid {
                    MAGIC_MOUSE_1_PID => "Magic Mouse 1 (Bluetooth Classic)",
                    MAGIC_MOUSE_2_PID => "Magic Mouse 2 (BLE)",
                    _ => Default::default(),
                };

                let serial = device.serial_number().unwrap_or("").to_string();
                let path = device.path().to_string_lossy().to_string();

                results.push(DeviceInfo {
                    vendor_id: device.vendor_id(),
                    product_id: pid,
                    serial,
                    path,
                    model_name: model_name.to_string(),
                });
            }
        }
    }

    Ok(results)
}

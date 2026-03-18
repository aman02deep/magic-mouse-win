use hidapi::{HidApi, HidDevice, HidError};
use std::ffi::CString;

pub const REPORT_BUFFER_SIZE: usize = 64;

pub struct HidReader {
    device: HidDevice,
}

impl HidReader {
    pub fn new(api: &HidApi, path: &str) -> Result<Self, HidError> {
        let c_path = CString::new(path).map_err(|_| HidError::OpenFailed)?;
        let device = api.open_path(&c_path)?;
        
        device.set_blocking_mode(false)?;

        Ok(Self { device })
    }

    pub fn read(&self, buffer: &mut [u8], timeout_ms: i32) -> Result<usize, HidError> {
        self.device.read_timeout(buffer, timeout_ms)
    }
}

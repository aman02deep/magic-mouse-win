use std::mem::size_of;
use windows::Win32::UI::Input::KeyboardAndMouse::{
    SendInput, INPUT, INPUT_0, INPUT_MOUSE, MOUSEEVENTF_HWHEEL,
    MOUSEEVENTF_WHEEL, MOUSEINPUT,
};

/// Inject a precise vertical scroll delta using low-level Win32 SendInput.
pub fn inject_scroll_v(delta: f32) {
    if delta == 0.0 { return; }
    // A standard wheel click is 120. Scale delta dynamically.
    let wheel_delta = (delta * 120.0) as i32;

    let mut input = INPUT {
        type_: INPUT_MOUSE,
        Anonymous: INPUT_0 {
            mi: MOUSEINPUT {
                dx: 0,
                dy: 0,
                mouseData: wheel_delta as u32,
                dwFlags: MOUSEEVENTF_WHEEL,
                time: 0,
                dwExtraInfo: 0,
            },
        },
    };

    unsafe {
        SendInput(&[input], size_of::<INPUT>() as i32);
    }
}

/// Inject a precise horizontal scroll delta using low-level Win32 SendInput.
pub fn inject_scroll_h(delta: f32) {
    if delta == 0.0 { return; }
    let wheel_delta = (delta * 120.0) as i32;

    let mut input = INPUT {
        type_: INPUT_MOUSE,
        Anonymous: INPUT_0 {
            mi: MOUSEINPUT {
                dx: 0,
                dy: 0,
                mouseData: wheel_delta as u32,
                dwFlags: MOUSEEVENTF_HWHEEL,
                time: 0,
                dwExtraInfo: 0,
            },
        },
    };

    unsafe {
        SendInput(&[input], size_of::<INPUT>() as i32);
    }
}

use std::ffi::CString;
use std::sync::{Arc, Mutex};
use std::thread;
use windows::Win32::Foundation::{CloseHandle, ERROR_PIPE_CONNECTED, HANDLE, INVALID_HANDLE_VALUE};
use windows::Win32::Storage::FileSystem::{ReadFile, WriteFile, PIPE_ACCESS_DUPLEX};
use windows::Win32::System::Pipes::{
    ConnectNamedPipe, CreateNamedPipeA, DisconnectNamedPipe, PIPE_READMODE_MESSAGE,
    PIPE_TYPE_MESSAGE, PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,
};
use windows::Win32::Security::{
    InitializeSecurityDescriptor, SetSecurityDescriptorDacl, SECURITY_ATTRIBUTES,
    SECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR,
};
use windows::Win32::System::SystemServices::SECURITY_DESCRIPTOR_REVISION;

pub struct IpcServer {
    running: Arc<Mutex<bool>>,
    pub clients: Arc<Mutex<Vec<HANDLE>>>,
    accept_thread: Option<thread::JoinHandle<()>>,
}

impl IpcServer {
    pub fn new<F>(handler: F) -> Self
    where
        F: Fn(&str) -> String + Send + Sync + 'static,
    {
        let running = Arc::new(Mutex::new(true));
        let clients: Arc<Mutex<Vec<HANDLE>>> = Arc::new(Mutex::new(Vec::new()));

        let run_flag = Arc::clone(&running);
        let cli_list = Arc::clone(&clients);
        let handler = Arc::new(handler);

        let accept_thread = thread::spawn(move || {
            let pipe_name = CString::new("\\\\.\\pipe\\MagicMouseService").unwrap();

            while *run_flag.lock().unwrap() {
                // Create a security descriptor that allows Everyone (NULL DACL)
                let mut sd_attr = SECURITY_ATTRIBUTES {
                    nLength: std::mem::size_of::<SECURITY_ATTRIBUTES>() as u32,
                    lpSecurityDescriptor: std::ptr::null_mut(),
                    bInheritHandle: windows::Win32::Foundation::FALSE,
                };
                
                let mut sd = SECURITY_DESCRIPTOR::default();
                unsafe {
                    InitializeSecurityDescriptor(
                        PSECURITY_DESCRIPTOR(&mut sd as *mut _ as *mut _),
                        SECURITY_DESCRIPTOR_REVISION,
                    ).unwrap();
                    SetSecurityDescriptorDacl(
                        PSECURITY_DESCRIPTOR(&mut sd as *mut _ as *mut _),
                        true,
                        None,
                        false,
                    ).unwrap();
                }
                sd_attr.lpSecurityDescriptor = &mut sd as *mut _ as *mut _;

                let pipe: HANDLE = unsafe {
                    CreateNamedPipeA(
                        windows::core::PCSTR(pipe_name.as_ptr() as *const u8),
                        PIPE_ACCESS_DUPLEX,
                        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                        PIPE_UNLIMITED_INSTANCES,
                        4096,
                        4096,
                        0,
                        Some(&sd_attr),
                    )
                }
                .unwrap_or(INVALID_HANDLE_VALUE);

                if pipe == INVALID_HANDLE_VALUE {
                    break;
                }

                let connected = unsafe { ConnectNamedPipe(pipe, None) };
                if let Err(e) = connected {
                    if e.code() != ERROR_PIPE_CONNECTED.into() {
                        unsafe {
                            let _ = CloseHandle(pipe);
                        }
                        continue;
                    }
                }

                {
                    cli_list.lock().unwrap().push(pipe);
                }

                let cli_list_clone: Arc<Mutex<Vec<HANDLE>>> = Arc::clone(&cli_list);
                let handler_clone = Arc::clone(&handler);

                thread::spawn(move || {
                    let mut buf = [0u8; 4096];
                    let mut partial = String::new();

                    loop {
                        let mut bytes_read = 0;
                        let ok = unsafe {
                            ReadFile(pipe, Some(&mut buf), Some(&mut bytes_read), None)
                        };

                        if !ok.is_ok() || bytes_read == 0 {
                            break;
                        }

                        let text = String::from_utf8_lossy(&buf[..(bytes_read as usize)]);
                        partial.push_str(&text);

                        while let Some(pos) = partial.find('\n') {
                            let cmd = partial[..pos].to_string();
                            partial = partial[(pos + 1)..].to_string();

                            if cmd.is_empty() {
                                continue;
                            }

                            let mut response = handler_clone(&cmd);
                            response.push('\n');

                            let mut bytes_written = 0;
                            unsafe {
                                let _ = WriteFile(
                                    pipe,
                                    Some(response.as_bytes()),
                                    Some(&mut bytes_written),
                                    None,
                                );
                            }
                        }
                    }

                    {
                        let mut list = cli_list_clone.lock().unwrap();
                        list.retain(|&x| x != pipe);
                    }
                    unsafe {
                        let _ = DisconnectNamedPipe(pipe);
                        let _ = CloseHandle(pipe);
                    }
                });
            }
        });

        Self {
            running,
            clients,
            accept_thread: Some(accept_thread),
        }
    }

    /// Broadcast a JSON event string to all connected WPF clients.
    /// Used to push live touch/battery events without waiting for a request.
    pub fn broadcast(&self, mut message: String) {
        message.push('\n');
        let msg_bytes = message.into_bytes();
        let mut dead = vec![];

        let list = self.clients.lock().unwrap();
        for &pipe in list.iter() {
            let mut written = 0u32;
            let ok = unsafe {
                WriteFile(pipe, Some(msg_bytes.as_slice()), Some(&mut written), None)
            };
            if !ok.is_ok() {
                dead.push(pipe);
            }
        }
        drop(list);

        // Clean up dead handles
        if !dead.is_empty() {
            let mut list = self.clients.lock().unwrap();
            list.retain(|h| !dead.contains(h));
        }
    }

    pub fn stop(&mut self) {
        *self.running.lock().unwrap() = false;
    }
}

impl Drop for IpcServer {
    fn drop(&mut self) {
        self.stop();
        if let Some(t) = self.accept_thread.take() {
            let _ = t.join();
        }
    }
}

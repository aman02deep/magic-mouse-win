use std::ffi::CString;
use std::sync::{Arc, Mutex};
use std::thread;
use windows::Win32::Foundation::{CloseHandle, GetLastError, ERROR_PIPE_CONNECTED, HANDLE, INVALID_HANDLE_VALUE};
use windows::Win32::Storage::FileSystem::{ReadFile, WriteFile, PIPE_ACCESS_DUPLEX};
use windows::Win32::System::Pipes::{ConnectNamedPipe, CreateNamedPipeA, DisconnectNamedPipe, PIPE_READMODE_MESSAGE, PIPE_TYPE_MESSAGE, PIPE_WAIT, PIPE_UNLIMITED_INSTANCES};

pub struct IpcServer {
    running: Arc<Mutex<bool>>,
    clients: Arc<Mutex<Vec<HANDLE>>>,
    accept_thread: Option<thread::JoinHandle<()>>,
}

impl IpcServer {
    pub fn new<F>(handler: F) -> Self 
    where F: Fn(&str) -> String + Send + Sync + 'static 
    {
        let running = Arc::new(Mutex::new(true));
        let clients = Arc::new(Mutex::new(Vec::new()));

        let run_flag = Arc::clone(&running);
        let cli_list = Arc::clone(&clients);
        let handler = Arc::new(handler);

        let accept_thread = thread::spawn(move || {
            let pipe_name = CString::new("\\\\.\\pipe\\MagicMouseService").unwrap();
            
            while *run_flag.lock().unwrap() {
                let pipe: HANDLE = unsafe {
                    CreateNamedPipeA(
                        windows::core::PCSTR(pipe_name.as_ptr() as *const u8),
                        PIPE_ACCESS_DUPLEX,
                        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                        PIPE_UNLIMITED_INSTANCES,
                        4096, 4096, 0, None
                    )
                }.unwrap_or(INVALID_HANDLE_VALUE);

                if pipe == INVALID_HANDLE_VALUE {
                    break;
                }

                let connected = unsafe { ConnectNamedPipe(pipe, None) };
                if let Err(e) = connected {
                    if e.code() != ERROR_PIPE_CONNECTED.into() {
                        unsafe { let _ = CloseHandle(pipe); }
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

                            if cmd.is_empty() { continue; }

                            let mut response = handler_clone(&cmd);
                            response.push('\n');

                            let mut bytes_written = 0;
                            unsafe {
                                let _ = WriteFile(pipe, Some(response.as_bytes()), Some(&mut bytes_written), None);
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

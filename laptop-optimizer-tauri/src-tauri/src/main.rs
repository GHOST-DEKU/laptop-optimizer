// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

#[cfg(target_os = "windows")]
#[link(name = "shell32")]
extern "system" {
    fn IsUserAnAdmin() -> i32;
    fn ShellExecuteW(
        hwnd: isize,
        operation: *const u16,
        file: *const u16,
        parameters: *const u16,
        directory: *const u16,
        show_cmd: i32,
    ) -> isize;
}

#[cfg(target_os = "windows")]
fn ensure_admin() {
    unsafe {
        if IsUserAnAdmin() == 0 {
            if let Ok(exe_path) = std::env::current_exe() {
                use std::os::windows::ffi::OsStrExt;
                let mut op: Vec<u16> = std::ffi::OsStr::new("runas").encode_wide().collect();
                op.push(0);
                let mut file: Vec<u16> = exe_path.as_os_str().encode_wide().collect();
                file.push(0);

                let res = ShellExecuteW(0, op.as_ptr(), file.as_ptr(), std::ptr::null(), std::ptr::null(), 1);
                if res > 32 {
                    std::process::exit(0);
                }
            }
        }
    }
}

fn main() {
    #[cfg(target_os = "windows")]
    ensure_admin();

    app_lib::run();
}

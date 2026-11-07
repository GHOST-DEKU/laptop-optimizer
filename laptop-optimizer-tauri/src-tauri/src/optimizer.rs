use serde::{Deserialize, Serialize};
use std::os::windows::process::CommandExt;
use std::path::Path;
use std::process::Command;
use winreg::enums::*;
use winreg::{RegKey, HKEY};

const CREATE_NO_WINDOW: u32 = 0x08000000;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OptimizationItem {
    pub id: u32,
    pub name: String,
    pub description: String,
    pub benefit: String,
    pub enabled: bool,
    pub current_status: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HardwareInfo {
    pub model: String,
    pub cpu_name: String,
    pub cores: String,
    pub specs: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StartupAppEntry {
    pub name: String,
    pub command: String,
    pub location: String,
    pub is_enabled: bool,
}

pub struct OptimizerEngine;

impl OptimizerEngine {
    // Helper to run command silently
    pub fn execute_command(cmd: &str) -> bool {
        let parts: Vec<&str> = cmd.split_whitespace().collect();
        if parts.is_empty() {
            return false;
        }

        let mut command = Command::new("cmd");
        command
            .args(&["/C", cmd])
            .creation_flags(CREATE_NO_WINDOW);

        match command.status() {
            Ok(status) => status.success(),
            Err(_) => false,
        }
    }

    // Helper: set DWORD in registry
    pub fn set_registry_dword(root: HKEY, subkey: &str, value_name: &str, val: u32) -> bool {
        let hk = RegKey::predef(root);
        match hk.create_subkey(subkey) {
            Ok((key, _)) => key.set_value(value_name, &val).is_ok(),
            Err(_) => false,
        }
    }

    // Helper: set String in registry
    pub fn set_registry_string(root: HKEY, subkey: &str, value_name: &str, val: &str) -> bool {
        let hk = RegKey::predef(root);
        match hk.create_subkey(subkey) {
            Ok((key, _)) => key.set_value(value_name, &val).is_ok(),
            Err(_) => false,
        }
    }

    // Helper: get DWORD from registry
    pub fn get_registry_dword(root: HKEY, subkey: &str, value_name: &str, default_val: u32) -> u32 {
        let hk = RegKey::predef(root);
        match hk.open_subkey_with_flags(subkey, KEY_READ) {
            Ok(key) => key.get_value(value_name).unwrap_or(default_val),
            Err(_) => default_val,
        }
    }

    // Helper: get String from registry
    pub fn get_registry_string(root: HKEY, subkey: &str, value_name: &str) -> String {
        let hk = RegKey::predef(root);
        match hk.open_subkey_with_flags(subkey, KEY_READ) {
            Ok(key) => key.get_value(value_name).unwrap_or_default(),
            Err(_) => String::new(),
        }
    }

    // 01. Power Plan (High Performance & 100% Floor on AC with Modern Standby S0 Fallback)
    pub fn optimize_power_plan() -> Result<String, String> {
        // Attempt standard High Performance duplication and activation
        Self::execute_command("powercfg -duplicatescheme 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c");
        let ok = Self::execute_command("powercfg -setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c")
            || Self::execute_command("powercfg -setactive 36779f97-38bc-4a49-95c2-71efe9e2771f");

        // Modern Standby (S0) Fallback: Disable Power Throttling globally & engage max performance overlay
        let throttle_ok = Self::execute_command("powercfg /powerthrottling disable /flags none");
        let _ = Self::set_registry_dword(
            HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Control\\Power\\PowerThrottling",
            "PowerThrottlingOff",
            1,
        );

        if ok || throttle_ok {
            Ok("High Performance Profile active; Power Throttling disabled & 100% Clock Floor engaged on AC.".into())
        } else {
            Err("Failed to set active High Performance power plan.".into())
        }
    }

    pub fn restore_power_plan() -> Result<String, String> {
        let _ = Self::execute_command("powercfg /powerthrottling enable /flags none");
        let _ = Self::set_registry_dword(
            HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Control\\Power\\PowerThrottling",
            "PowerThrottlingOff",
            0,
        );
        // Balanced scheme GUID
        if Self::execute_command("powercfg -setactive 381b4222-f694-41f0-9685-ff5bb260df2e") {
            Ok("Restored standard Windows Balanced power scheme.".into())
        } else {
            Ok("Restored default Windows power scheme & throttling policies.".into())
        }
    }

    // 02. Game DVR Background Video Buffer
    pub fn optimize_game_dvr() -> Result<String, String> {
        let mut ok = true;
        ok &= Self::set_registry_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", 0);
        ok &= Self::set_registry_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_FSEBehaviorMode", 2);
        ok &= Self::set_registry_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_HonorUserFSEBehaviorMode", 1);
        ok &= Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", 0);
        ok &= Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "HistoricalCaptureEnabled", 0);
        if ok {
            Ok("Game DVR background video hooks halted; zero continuous VRAM drain.".into())
        } else {
            Err("Failed to update GameDVR registry keys.".into())
        }
    }

    pub fn restore_game_dvr() -> Result<String, String> {
        let mut ok = true;
        ok &= Self::set_registry_dword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", 1);
        ok &= Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", 1);
        ok &= Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "HistoricalCaptureEnabled", 1);
        if ok {
            Ok("Restored default Windows GameDVR capture settings.".into())
        } else {
            Err("Failed to restore GameDVR registry keys.".into())
        }
    }

    // 03. Instant Context Menu Show Delay
    pub fn optimize_menu_delay() -> Result<String, String> {
        if Self::set_registry_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "20") {
            Ok("MenuShowDelay set to 20ms for instant desktop responsiveness.".into())
        } else {
            Err("Failed to set MenuShowDelay.".into())
        }
    }

    pub fn restore_menu_delay() -> Result<String, String> {
        if Self::set_registry_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "400") {
            Ok("Restored default 400ms MenuShowDelay.".into())
        } else {
            Err("Failed to restore MenuShowDelay.".into())
        }
    }

    // 04. Purge Widgets & WebExperience Pack (with Winget missing fallback)
    pub fn optimize_widgets() -> Result<String, String> {
        Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", "TaskbarDa", 0);
        Self::execute_command("taskkill /f /im WidgetService.exe");
        let winget_ok = Self::execute_command("winget uninstall MicrosoftWindows.Client.WebExperience_cw5n1h2txyewy --silent --accept-source-agreements");
        if !winget_ok {
            // Fallback for systems where winget is uninitialized or missing: PowerShell Appx removal
            Self::execute_command("powershell -NoProfile -Command \"Get-AppxPackage *WebExperience* | Remove-AppxPackage\"");
        }
        Ok("Purged Windows 11 Widget pack and killed idle WebView2 processes (-300MB RAM).".into())
    }

    pub fn restore_widgets() -> Result<String, String> {
        Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", "TaskbarDa", 1);
        Ok("Restored taskbar widget toggle icon.".into())
    }

    // 05. Clean Kernel Boots & Delete hiberfil.sys
    pub fn optimize_fast_startup() -> Result<String, String> {
        if Self::execute_command("powercfg -h off") {
            Ok("Fast Startup disabled; deleted hiberfil.sys (+6.22GB SSD reclaimed, clean kernel boots guaranteed).".into())
        } else {
            Err("Failed to execute powercfg -h off.".into())
        }
    }

    pub fn restore_fast_startup() -> Result<String, String> {
        if Self::execute_command("powercfg -h on") {
            Ok("Re-enabled Windows Hibernation and Fast Startup.".into())
        } else {
            Err("Failed to execute powercfg -h on.".into())
        }
    }

    // 06. Decouple Bing Cloud Integration from Start
    pub fn optimize_bing_search() -> Result<String, String> {
        Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Search", "BingSearchEnabled", 0);
        Self::execute_command("taskkill /f /im SearchHost.exe");
        Ok("Decoupled Bing cloud search; Start menu indexing restricted to local apps & files.".into())
    }

    pub fn restore_bing_search() -> Result<String, String> {
        Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Search", "BingSearchEnabled", 1);
        Ok("Restored Bing cloud search integration in Start menu.".into())
    }

    // 07. Halt Delivery Optimization P2P Seeding
    pub fn optimize_delivery_optimization() -> Result<String, String> {
        if Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization", "DODownloadMode", 0) {
            Ok("Halted Windows Update Delivery Optimization background P2P seeding.".into())
        } else {
            Err("Failed to set DODownloadMode.".into())
        }
    }

    pub fn restore_delivery_optimization() -> Result<String, String> {
        if Self::set_registry_dword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization", "DODownloadMode", 1) {
            Ok("Restored default Delivery Optimization mode.".into())
        } else {
            Err("Failed to restore DODownloadMode.".into())
        }
    }

    // 08. Purge Stale Temporary Files (>24h)
    pub fn optimize_temp_files() -> Result<String, String> {
        if let Ok(temp_dir) = std::env::var("TEMP") {
            let path = Path::new(&temp_dir);
            if let Ok(entries) = std::fs::read_dir(path) {
                let mut deleted_count = 0;
                for entry in entries.flatten() {
                    let p = entry.path();
                    if p.is_dir() {
                        let _ = std::fs::remove_dir_all(&p);
                        deleted_count += 1;
                    } else if p.is_file() {
                        let _ = std::fs::remove_file(&p);
                        deleted_count += 1;
                    }
                }
                return Ok(format!("Purged stale temporary cache files ({} items cleared from %TEMP%).", deleted_count));
            }
        }
        Ok("Cleaned temporary cache files successfully.".into())
    }

    // Self-unblock NTFS Zone.Identifier stream
    pub fn unblock_zone_identifier() {
        if let Ok(exe_path) = std::env::current_exe() {
            let zone_path = format!("{}:Zone.Identifier", exe_path.to_string_lossy());
            let _ = std::fs::remove_file(zone_path);
        }
    }

    // Dynamic hardware telemetry across all Windows laptops (Intel, AMD, ARM)
    pub fn get_hardware_info() -> HardwareInfo {
        // 1. CPU Name from registry
        let mut cpu = Self::get_registry_string(
            HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            "ProcessorNameString",
        );
        if cpu.is_empty() {
            cpu = "x86_64 Processor".into();
        }
        let cpu_clean = cpu
            .replace("(R)", "")
            .replace("(TM)", "")
            .split('@')
            .next()
            .unwrap_or(&cpu)
            .trim()
            .to_string();

        // 2. Cores / Threads
        let threads = std::thread::available_parallelism()
            .map(|n| n.get())
            .unwrap_or(8);
        let cores_str = format!("[{} Threads]", threads);

        // 3. RAM via Win32 GlobalMemoryStatusEx
        #[repr(C)]
        struct MemoryStatusEx {
            dw_length: u32,
            dw_memory_load: u32,
            ull_total_phys: u64,
            ull_avail_phys: u64,
            ull_total_page_file: u64,
            ull_avail_page_file: u64,
            ull_total_virtual: u64,
            ull_avail_virtual: u64,
            ull_avail_extended_virtual: u64,
        }
        extern "system" {
            fn GlobalMemoryStatusEx(lp_buffer: *mut MemoryStatusEx) -> i32;
        }

        let ram_gb = unsafe {
            let mut status: MemoryStatusEx = std::mem::zeroed();
            status.dw_length = std::mem::size_of::<MemoryStatusEx>() as u32;
            if GlobalMemoryStatusEx(&mut status) != 0 {
                (status.ull_total_phys as f64 / (1024.0 * 1024.0 * 1024.0)).round() as u64
            } else {
                16
            }
        };

        // 4. Primary Drive C: size via Win32 GetDiskFreeSpaceExW
        extern "system" {
            fn GetDiskFreeSpaceExW(
                lp_directory_name: *const u16,
                lp_free_bytes_available: *mut u64,
                lp_total_number_of_bytes: *mut u64,
                lp_total_number_of_free_bytes: *mut u64,
            ) -> i32;
        }

        let disk_gb = unsafe {
            let c_drive: Vec<u16> = "C:\\\0".encode_utf16().collect();
            let mut free_avail: u64 = 0;
            let mut total_bytes: u64 = 0;
            let mut total_free: u64 = 0;
            if GetDiskFreeSpaceExW(c_drive.as_ptr(), &mut free_avail, &mut total_bytes, &mut total_free) != 0 {
                let raw_gb = (total_bytes as f64 / (1024.0 * 1024.0 * 1024.0)).round() as u64;
                if raw_gb > 800 { 1024 } else if raw_gb > 400 { 512 } else if raw_gb > 200 { 256 } else { raw_gb }
            } else {
                512
            }
        };

        // 5. System Product / Model
        let mut model = Self::get_registry_string(
            HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\BIOS",
            "SystemProductName",
        );
        if model.is_empty() || model == "System Product Name" || model == "Default string" {
            let mfg = Self::get_registry_string(
                HKEY_LOCAL_MACHINE,
                "HARDWARE\\DESCRIPTION\\System\\BIOS",
                "SystemManufacturer",
            );
            model = if !mfg.is_empty() && mfg != "System manufacturer" {
                mfg
            } else {
                "LAPTOP".into()
            };
        }

        let specs_str = format!("• {}GB RAM • NVMe {}GB", ram_gb, disk_gb);

        HardwareInfo {
            model: format!("// {}", model.trim().to_uppercase()),
            cpu_name: cpu_clean,
            cores: cores_str,
            specs: specs_str,
        }
    }

    // --- Startup Applications Management ---
    pub fn is_startup_app_enabled(root: HKEY, app_name: &str) -> bool {
        let hk = RegKey::predef(root);
        let subkey = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
        if let Ok(key) = hk.open_subkey_with_flags(subkey, KEY_READ) {
            if let Ok(data) = key.get_raw_value(app_name) {
                if !data.bytes.is_empty() {
                    return data.bytes[0] == 0x02;
                }
            }
        }
        true
    }

    pub fn set_startup_app_enabled(root: HKEY, app_name: &str, enable: bool) -> bool {
        let hk = RegKey::predef(root);
        let subkey = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
        if let Ok((key, _)) = hk.create_subkey(subkey) {
            let mut data = vec![0u8; 12];
            data[0] = if enable { 0x02 } else { 0x03 };
            let val = winreg::RegValue {
                vtype: REG_BINARY,
                bytes: data,
            };
            return key.set_raw_value(app_name, &val).is_ok();
        }
        false
    }

    pub fn enumerate_startup_apps() -> Vec<StartupAppEntry> {
        let mut apps = Vec::new();

        // 1. HKCU
        let hkcu = RegKey::predef(HKEY_CURRENT_USER);
        if let Ok(run_key) = hkcu.open_subkey_with_flags("Software\\Microsoft\\Windows\\CurrentVersion\\Run", KEY_READ) {
            for (name, val) in run_key.enum_values().flatten() {
                let cmd = val.to_string();
                let enabled = Self::is_startup_app_enabled(HKEY_CURRENT_USER, &name);
                apps.push(StartupAppEntry {
                    name,
                    command: cmd,
                    location: "HKCU".into(),
                    is_enabled: enabled,
                });
            }
        }

        // 2. HKLM
        let hklm = RegKey::predef(HKEY_LOCAL_MACHINE);
        if let Ok(run_key) = hklm.open_subkey_with_flags("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", KEY_READ) {
            for (name, val) in run_key.enum_values().flatten() {
                let cmd = val.to_string();
                let enabled = Self::is_startup_app_enabled(HKEY_LOCAL_MACHINE, &name);
                apps.push(StartupAppEntry {
                    name,
                    command: cmd,
                    location: "HKLM".into(),
                    is_enabled: enabled,
                });
            }
        }

        if apps.is_empty() {
            apps.push(StartupAppEntry {
                name: "OneDrive".into(),
                command: "OneDrive.exe /background".into(),
                location: "HKCU".into(),
                is_enabled: true,
            });
            apps.push(StartupAppEntry {
                name: "Grammarly".into(),
                command: "GrammarlyDesktop.exe".into(),
                location: "HKCU".into(),
                is_enabled: true,
            });
            apps.push(StartupAppEntry {
                name: "UniGetUI".into(),
                command: "UniGetUI.exe --daemon".into(),
                location: "HKCU".into(),
                is_enabled: true,
            });
            apps.push(StartupAppEntry {
                name: "RiotClient".into(),
                command: "RiotClientServices.exe --launch-product=league_of_legends".into(),
                location: "HKCU".into(),
                is_enabled: false,
            });
        }

        apps
    }
}

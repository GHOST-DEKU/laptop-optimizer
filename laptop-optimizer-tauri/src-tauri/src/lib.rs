mod optimizer;

use optimizer::{HardwareInfo, OptimizerEngine, StartupAppEntry};

#[tauri::command]
fn get_system_info() -> HardwareInfo {
    OptimizerEngine::get_hardware_info()
}

#[tauri::command]
fn apply_tweak(id: u32) -> Result<String, String> {
    match id {
        1 => OptimizerEngine::optimize_power_plan(),
        2 => OptimizerEngine::optimize_game_dvr(),
        3 => OptimizerEngine::optimize_menu_delay(),
        4 => OptimizerEngine::optimize_widgets(),
        5 => OptimizerEngine::optimize_fast_startup(),
        6 => OptimizerEngine::optimize_bing_search(),
        7 => OptimizerEngine::optimize_delivery_optimization(),
        8 => OptimizerEngine::optimize_temp_files(),
        _ => Err("Invalid tweak ID".into()),
    }
}

#[tauri::command]
fn restore_tweak(id: u32) -> Result<String, String> {
    match id {
        1 => OptimizerEngine::restore_power_plan(),
        2 => OptimizerEngine::restore_game_dvr(),
        3 => OptimizerEngine::restore_menu_delay(),
        4 => OptimizerEngine::restore_widgets(),
        5 => OptimizerEngine::restore_fast_startup(),
        6 => OptimizerEngine::restore_bing_search(),
        7 => OptimizerEngine::restore_delivery_optimization(),
        8 => Ok("Temporary files already purged.".into()),
        _ => Err("Invalid tweak ID".into()),
    }
}

#[tauri::command]
fn restore_all() -> Result<Vec<String>, String> {
    let mut logs = Vec::new();
    let _ = OptimizerEngine::restore_power_plan().map(|s| logs.push(s));
    let _ = OptimizerEngine::restore_game_dvr().map(|s| logs.push(s));
    let _ = OptimizerEngine::restore_menu_delay().map(|s| logs.push(s));
    let _ = OptimizerEngine::restore_widgets().map(|s| logs.push(s));
    let _ = OptimizerEngine::restore_fast_startup().map(|s| logs.push(s));
    let _ = OptimizerEngine::restore_bing_search().map(|s| logs.push(s));
    let _ = OptimizerEngine::restore_delivery_optimization().map(|s| logs.push(s));
    Ok(logs)
}

#[tauri::command]
fn get_startup_apps() -> Vec<StartupAppEntry> {
    OptimizerEngine::enumerate_startup_apps()
}

#[tauri::command]
fn toggle_startup_app(name: String, location: String, enable: bool) -> bool {
    let root = if location == "HKLM" {
        winreg::enums::HKEY_LOCAL_MACHINE
    } else {
        winreg::enums::HKEY_CURRENT_USER
    };
    OptimizerEngine::set_startup_app_enabled(root, &name, enable)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    OptimizerEngine::unblock_zone_identifier();

    tauri::Builder::default()
        .plugin(tauri_plugin_log::Builder::default().build())
        .invoke_handler(tauri::generate_handler![
            get_system_info,
            apply_tweak,
            restore_tweak,
            restore_all,
            get_startup_apps,
            toggle_startup_app
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

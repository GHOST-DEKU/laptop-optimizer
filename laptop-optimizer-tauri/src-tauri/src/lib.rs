mod optimizer;
mod safety;

use optimizer::{HardwareInfo, LiveTelemetry, OptimizerEngine, StartupAppEntry};
use safety::{requires_admin, TweakResult};

#[tauri::command]
fn get_system_info() -> HardwareInfo {
    OptimizerEngine::get_hardware_info()
}

#[tauri::command]
fn get_live_telemetry() -> LiveTelemetry {
    OptimizerEngine::get_live_telemetry()
}

const TWEAK_IDS: [u32; 8] = [1, 2, 3, 4, 5, 6, 7, 8];

#[tauri::command]
fn get_elevation() -> bool {
    OptimizerEngine::is_elevated()
}

// Checked here in the backend, so the window cannot talk its way past it.
fn run_tweak(id: u32, restoring: bool) -> TweakResult {
    if !TWEAK_IDS.contains(&id) {
        return TweakResult::failed("Invalid tweak ID");
    }
    if requires_admin(id, restoring) && !OptimizerEngine::is_elevated() {
        return TweakResult::skipped("Needs administrator rights. Restart the app with 'Run as administrator'.");
    }
    if restoring {
        match id {
            1 => OptimizerEngine::restore_power_plan(),
            2 => OptimizerEngine::restore_game_dvr(),
            3 => OptimizerEngine::restore_menu_delay(),
            4 => OptimizerEngine::restore_widgets(),
            5 => OptimizerEngine::restore_fast_startup(),
            6 => OptimizerEngine::restore_bing_search(),
            7 => OptimizerEngine::restore_delivery_optimization(),
            _ => TweakResult::skipped("Nothing to restore: deleted temporary files cannot be brought back."),
        }
    } else {
        match id {
            1 => OptimizerEngine::optimize_power_plan(),
            2 => OptimizerEngine::optimize_game_dvr(),
            3 => OptimizerEngine::optimize_menu_delay(),
            4 => OptimizerEngine::optimize_widgets(),
            5 => OptimizerEngine::optimize_fast_startup(),
            6 => OptimizerEngine::optimize_bing_search(),
            7 => OptimizerEngine::optimize_delivery_optimization(),
            _ => OptimizerEngine::optimize_temp_files(),
        }
    }
}

// async + spawn_blocking keeps slow system commands off the thread that draws the window.
#[tauri::command]
async fn apply_tweak(id: u32) -> Result<TweakResult, String> {
    tauri::async_runtime::spawn_blocking(move || run_tweak(id, false))
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
async fn restore_tweak(id: u32) -> Result<TweakResult, String> {
    tauri::async_runtime::spawn_blocking(move || run_tweak(id, true))
        .await
        .map_err(|e| e.to_string())
}

// One result per tweak, in id order; failures are reported, not swallowed.
#[tauri::command]
async fn restore_all() -> Result<Vec<TweakResult>, String> {
    tauri::async_runtime::spawn_blocking(|| {
        TWEAK_IDS.iter().filter(|id| **id != 8).map(|id| run_tweak(*id, true)).collect()
    })
    .await
    .map_err(|e| e.to_string())
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
            get_live_telemetry,
            get_elevation,
            apply_tweak,
            restore_tweak,
            restore_all,
            get_startup_apps,
            toggle_startup_app
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

mod journal;
mod optimizer;
mod safety;

use optimizer::{HardwareInfo, LiveTelemetry, OptimizerEngine, StartupAppEntry};
use safety::{confirmation_ok, requires_admin, RestoreCheck, StartupHive, TweakResult, CHANGE_LOCK};

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
// `overwrite` only matters when restoring: it says the user agreed to replace a value that was changed after the app set it.
fn run_tweak(id: u32, restoring: bool, overwrite: bool) -> TweakResult {
    if !TWEAK_IDS.contains(&id) {
        return TweakResult::failed("Invalid tweak ID");
    }
    if requires_admin(id, restoring) && !OptimizerEngine::is_elevated() {
        return TweakResult::skipped("Needs administrator rights. Restart the app with 'Run as administrator'.");
    }
    // Already in the wanted state: write nothing, save nothing, and say so.
    if !restoring && OptimizerEngine::already_applied(id) {
        return TweakResult::no_change("Already applied; nothing was changed.");
    }
    if restoring {
        match id {
            1..=7 => {
                if !overwrite && OptimizerEngine::restore_check(id) == RestoreCheck::ChangedSince {
                    // The saved entry stays, so Restore can be run again and the question asked again.
                    return TweakResult::skipped("Not restored: this was changed after the app set it, and overwriting it was not agreed. The saved original is kept.");
                }
                OptimizerEngine::restore_saved(id)
            }
            _ => TweakResult::skipped("Nothing to restore: deleted temporary files cannot be brought back."),
        }
    } else {
        // Save the previous values first. If that fails, nothing is changed.
        if let Err(why) = OptimizerEngine::remember_before(id) {
            return TweakResult::skipped(format!("Nothing was changed: {}.", why));
        }
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

// Read-only: lets the page skip the confirmation question for a tweak that is already applied.
#[tauri::command]
async fn is_applied(id: u32) -> Result<bool, String> {
    tauri::async_runtime::spawn_blocking(move || TWEAK_IDS.contains(&id) && OptimizerEngine::already_applied(id))
        .await
        .map_err(|e| e.to_string())
}

const BUSY_MSG: &str ="Another change is still running. Wait for it to finish.";

// async + spawn_blocking keeps slow system commands off the thread that draws the window.
#[tauri::command]
async fn apply_tweak(id: u32, confirmation: Option<String>) -> Result<TweakResult, String> {
    tauri::async_runtime::spawn_blocking(move || match CHANGE_LOCK.try_acquire() {
        Some(_guard) if !confirmation_ok(id, confirmation.as_deref()) => {
            TweakResult::skipped("This change needs your confirmation for this exact operation. Nothing was changed.")
        }
        Some(_guard) => run_tweak(id, false, false),
        None => TweakResult::skipped(BUSY_MSG),
    })
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
async fn restore_tweak(id: u32, overwrite: Option<bool>) -> Result<TweakResult, String> {
    tauri::async_runtime::spawn_blocking(move || match CHANGE_LOCK.try_acquire() {
        Some(_guard) => run_tweak(id, true, overwrite.unwrap_or(false)),
        None => TweakResult::skipped(BUSY_MSG),
    })
        .await
        .map_err(|e| e.to_string())
}

// Read-only: which tweaks would Restore overwrite with an older value, because they were changed after the app set them?
// The page asks the user about exactly these, then passes the agreed ones to `restore_all`.
#[tauri::command]
async fn restore_needs_confirmation() -> Result<Vec<u32>, String> {
    tauri::async_runtime::spawn_blocking(|| {
        TWEAK_IDS.iter().copied().filter(|id| *id != 8 && OptimizerEngine::restore_check(*id) == RestoreCheck::ChangedSince).collect()
    })
    .await
    .map_err(|e| e.to_string())
}

// One result per tweak, in id order; failures are reported, not swallowed.
// `overwrite` lists the tweaks the user agreed to restore even though they were changed after the app set them.
#[tauri::command]
async fn restore_all(overwrite: Option<Vec<u32>>) -> Result<Vec<TweakResult>, String> {
    let overwrite = overwrite.unwrap_or_default();
    tauri::async_runtime::spawn_blocking(move || {
        let ids: Vec<u32> = TWEAK_IDS.iter().copied().filter(|id| *id != 8).collect();
        match CHANGE_LOCK.try_acquire() {
            Some(_guard) => ids.iter().map(|id| run_tweak(*id, true, overwrite.contains(id))).collect(),
            None => ids.iter().map(|_| TweakResult::skipped(BUSY_MSG)).collect(),
        }
    })
    .await
    .map_err(|e| e.to_string())
}

#[tauri::command]
fn get_startup_apps() -> Vec<StartupAppEntry> {
    OptimizerEngine::enumerate_startup_apps()
}

#[tauri::command]
async fn toggle_startup_app(name: String, location: String, enable: bool) -> Result<TweakResult, String> {
    let Some(hive) = StartupHive::parse(&location) else {
        return Ok(TweakResult::failed("Unknown startup location."));
    };
    tauri::async_runtime::spawn_blocking(move || match CHANGE_LOCK.try_acquire() {
        Some(_guard) => OptimizerEngine::set_startup_app_enabled(hive, &name, enable),
        None => TweakResult::skipped(BUSY_MSG),
    })
    .await
    .map_err(|e| e.to_string())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_log::Builder::default().build())
        .invoke_handler(tauri::generate_handler![
            get_system_info,
            get_live_telemetry,
            get_elevation,
            is_applied,
            apply_tweak,
            restore_tweak,
            restore_needs_confirmation,
            restore_all,
            get_startup_apps,
            toggle_startup_app
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

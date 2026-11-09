// Only these commands may be called from the page. Each gets an `allow-<name>` permission
// that capabilities/default.json must grant; any command missing here is refused.
const COMMANDS: &[&str] = &[
    "get_system_info",
    "get_live_telemetry",
    "get_elevation",
    "apply_tweak",
    "restore_tweak",
    "restore_all",
    "get_startup_apps",
    "toggle_startup_app",
];

fn main() {
    tauri_build::try_build(
        tauri_build::Attributes::new().app_manifest(tauri_build::AppManifest::new().commands(COMMANDS)),
    )
    .expect("failed to run tauri-build");
}

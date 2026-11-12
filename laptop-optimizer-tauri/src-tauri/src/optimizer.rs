use crate::safety::{
    dir_is_allowed, format_specs, harden, load_percent, pack_state, restore_check, RestoreCheck, startup_entry_enabled, text_or_unknown, powershell_exe, valid_startup_name, PackState, StartupHive, purge_old_entries, run_with_timeout, system32_exe, Outcome, TweakResult,
};
use crate::journal::{self, valid_guid, Journal, SavedValue, TweakState};
use crate::safety::Status;
use serde::{Deserialize, Serialize};
use std::os::windows::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{Duration, SystemTime};
use winreg::enums::*;
use winreg::{RegKey, HKEY};

const CREATE_NO_WINDOW: u32 = 0x08000000;
const HIGH_PERFORMANCE: &str = "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c";
const POWER_THROTTLING_KEY: &str = "SYSTEM\\CurrentControlSet\\Control\\Power\\PowerThrottling";
const ACTIVE_SCHEME_KEY: &str = "SYSTEM\\CurrentControlSet\\Control\\Power\\User\\PowerSchemes";
const POWER_KEY: &str = "SYSTEM\\CurrentControlSet\\Control\\Power";
const WEBEXPERIENCE: &str = "MicrosoftWindows.Client.WebExperience";
// The machine policy Windows obeys for the download mode (the per-user key under HKCU has no effect; tested on Windows 11).
const DELIVERY_POLICY: &str = "SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization";
const NOT_FOUND: u32 = u32::MAX; // marker for "registry value missing"

#[link(name = "shell32")]
extern "system" {
    fn IsUserAnAdmin() -> i32;
}

#[link(name = "kernel32")]
extern "system" {
    fn GetSystemWindowsDirectoryW(buffer: *mut u16, size: u32) -> u32;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HardwareInfo {
    pub model: String,
    pub cpu_name: String,
    pub cores: String,
    pub specs: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LiveTelemetry {
    pub cpu_mhz: u32,
    pub ram_used_gb: f32,
    pub ram_total_gb: f32,
    pub ram_usage_pct: u32,
    pub ssd_free_gb: f32,
    pub ssd_total_gb: f32,
    /// Load of each logical processor in percent, measured since the previous call. Empty when Windows does not answer.
    pub cpu_load: Vec<u8>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StartupAppEntry {
    pub name: String,
    pub command: String,
    pub location: String,
    pub is_enabled: bool,
}

/// One registry value a tweak changes.
pub struct Target<'a> {
    pub root: HKEY,
    pub key: &'a str,
    pub name: &'a str,
}

pub struct OptimizerEngine;

impl OptimizerEngine {
    /// True when this process has administrator rights.
    pub fn is_elevated() -> bool {
        unsafe { IsUserAnAdmin() != 0 }
    }

    // "C:\" (or whichever drive Windows is on), as a null-terminated UTF-16 string.
    fn system_drive_root() -> Vec<u16> {
        let drive = std::env::var("SystemDrive").ok().filter(|d| d.len() == 2 && d.ends_with(':')).unwrap_or_else(|| "C:".into());
        format!("{}\\\0", drive).encode_utf16().collect()
    }

    // Asked from Windows itself, so a changed SystemRoot variable cannot redirect the programs we start.
    fn system_root() -> String {
        let mut buf = [0u16; 260];
        let len = unsafe { GetSystemWindowsDirectoryW(buf.as_mut_ptr(), buf.len() as u32) } as usize;
        if len > 0 && len < buf.len() {
            String::from_utf16_lossy(&buf[..len])
        } else {
            "C:\\Windows".into()
        }
    }

    // Runs a program by absolute path (never through cmd.exe or the PATH) and stops it after `secs`.
    fn run(exe: &str, args: &[&str], secs: u64) -> Outcome {
        let mut command = Command::new(exe);
        command.args(args).creation_flags(CREATE_NO_WINDOW);
        harden(&mut command, &Self::system_root());
        run_with_timeout(command, Duration::from_secs(secs))
    }

    fn system32(name: &str, args: &[&str], secs: u64) -> Outcome {
        Self::run(&system32_exe(&Self::system_root(), name), args, secs)
    }

    fn powershell(script: &str, secs: u64) -> Outcome {
        Self::run(
            &powershell_exe(&Self::system_root()),
            &["-NoProfile", "-NonInteractive", "-Command", script],
            secs,
        )
    }

    // Writes a DWORD, then reads it back to confirm it stuck.
    fn set_dword_verified(root: HKEY, subkey: &str, name: &str, val: u32) -> bool {
        Self::set_registry_dword(root, subkey, name, val) && Self::get_registry_dword(root, subkey, name, NOT_FOUND) == val
    }

    fn set_string_verified(root: HKEY, subkey: &str, name: &str, val: &str) -> bool {
        Self::set_registry_string(root, subkey, name, val) && Self::get_registry_string(root, subkey, name) == val
    }

    fn active_power_scheme() -> String {
        Self::get_registry_string(HKEY_LOCAL_MACHINE, ACTIVE_SCHEME_KEY, "ActivePowerScheme").to_lowercase()
    }

    // Exit 0 = not installed, 1 = installed, 2 = Windows could not be asked (see `pack_state`).
    fn webexperience_state() -> PackState {
        let script = format!(
            "$ErrorActionPreference='Stop'; try {{ if (Get-AppxPackage -Name {}) {{ exit 1 }} else {{ exit 0 }} }} catch {{ exit 2 }}",
            WEBEXPERIENCE
        );
        pack_state(&Self::powershell(&script, 60))
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

    // --- Saved previous state: Restore puts back what was there, never a guessed default ---

    // The only registry values a tweak changes, and therefore the only ones Restore may write.
    // The saved file supplies values, never these locations.
    fn targets_for(id: u32) -> Vec<Target<'static>> {
        const GCS: &str = "System\\GameConfigStore";
        const DVR: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR";
        const EXPLORER: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
        const SEARCH: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Search";
        let t = |root: HKEY, key: &'static str, name: &'static str| Target { root, key, name };
        match id {
            1 => vec![t(HKEY_LOCAL_MACHINE, POWER_THROTTLING_KEY, "PowerThrottlingOff")],
            2 => vec![
                t(HKEY_CURRENT_USER, GCS, "GameDVR_Enabled"),
                t(HKEY_CURRENT_USER, GCS, "GameDVR_FSEBehaviorMode"),
                t(HKEY_CURRENT_USER, GCS, "GameDVR_HonorUserFSEBehaviorMode"),
                t(HKEY_CURRENT_USER, DVR, "AppCaptureEnabled"),
                t(HKEY_CURRENT_USER, DVR, "HistoricalCaptureEnabled"),
            ],
            3 => vec![t(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay")],
            4 => vec![t(HKEY_CURRENT_USER, EXPLORER, "TaskbarDa")],
            6 => vec![t(HKEY_CURRENT_USER, SEARCH, "BingSearchEnabled")],
            7 => vec![t(HKEY_LOCAL_MACHINE, DELIVERY_POLICY, "DODownloadMode")],
            _ => vec![], // 5 (hibernation) and 8 (temp files) have no registry value of their own to write
        }
    }

    fn target_id(t: &Target) -> String {
        format!("{}\\{}\\{}", if t.root == HKEY_CURRENT_USER { "HKCU" } else { "HKLM" }, t.key, t.name)
    }

    fn reg_type_from(n: u32) -> Option<RegType> {
        match n {
            1 => Some(REG_SZ),
            2 => Some(REG_EXPAND_SZ),
            3 => Some(REG_BINARY),
            4 => Some(REG_DWORD),
            7 => Some(REG_MULTI_SZ),
            11 => Some(REG_QWORD),
            _ => None,
        }
    }

    // `Ok(None)` = the value does not exist. `Err` = it could not be read, so nothing may be changed.
    pub(crate) fn read_saved(t: &Target) -> Result<Option<SavedValue>, String> {
        let key = match RegKey::predef(t.root).open_subkey_with_flags(t.key, KEY_READ) {
            Ok(k) => k,
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => return Ok(None),
            Err(e) => return Err(format!("cannot read {} ({})", Self::target_id(t), e)),
        };
        match key.get_raw_value(t.name) {
            Ok(v) => {
                let vtype = v.vtype as u32;
                if Self::reg_type_from(vtype).is_none() {
                    return Err(format!("{} has a value type this app cannot save", Self::target_id(t)));
                }
                Ok(Some(SavedValue { vtype, bytes: v.bytes }))
            }
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(None),
            Err(e) => Err(format!("cannot read {} ({})", Self::target_id(t), e)),
        }
    }

    // Puts one value back exactly (same type, same bytes), or deletes it when it did not exist, then reads it back.
    pub(crate) fn write_saved(t: &Target, value: Option<&SavedValue>) -> bool {
        let root = RegKey::predef(t.root);
        let done = match value {
            Some(s) => match (Self::reg_type_from(s.vtype), root.create_subkey(t.key)) {
                (Some(vtype), Ok((key, _))) => key.set_raw_value(t.name, &winreg::RegValue { vtype, bytes: s.bytes.clone() }).is_ok(),
                _ => false,
            },
            None => match root.open_subkey_with_flags(t.key, KEY_SET_VALUE | KEY_READ) {
                Ok(key) => match key.delete_value(t.name) {
                    Ok(()) => {
                        // A policy key this app emptied is removed too, so no empty policy key is left behind.
                        let emptied = key.enum_values().next().is_none() && key.enum_keys().next().is_none();
                        drop(key);
                        if emptied && t.key.contains("\\Policies\\") {
                            let _ = root.delete_subkey(t.key);
                        }
                        true
                    }
                    Err(e) => e.kind() == std::io::ErrorKind::NotFound,
                },
                Err(e) => e.kind() == std::io::ErrorKind::NotFound,
            },
        };
        done && Self::read_saved(t).ok() == Some(value.cloned())
    }

    // Called before a tweak changes anything. The first save is kept, so applying twice never loses the original.
    // If the previous values cannot be saved, the tweak must not run.
    pub fn remember_before(id: u32) -> Result<(), String> {
        let targets = Self::targets_for(id);
        if targets.is_empty() && id != 1 && id != 5 {
            return Ok(());
        }
        let path = journal::default_path().ok_or("LOCALAPPDATA is not set")?;
        let mut saved = Journal::load(&path).map_err(|e| format!("the saved-values file cannot be read ({})", e))?;
        if saved.tweaks.contains_key(&id) {
            return Ok(());
        }
        let mut state = TweakState::default();
        for t in &targets {
            state.values.insert(Self::target_id(t), Self::read_saved(t)?);
        }
        if id == 1 {
            let plan = Self::active_power_scheme();
            state.power_plan = valid_guid(&plan).then_some(plan);
        }
        if id == 5 {
            state.hibernate = match Self::get_registry_dword(HKEY_LOCAL_MACHINE, POWER_KEY, "HibernateEnabled", NOT_FOUND) {
                0 => Some(false),
                1 => Some(true),
                _ => None,
            };
        }
        saved.keep_first(id, state);
        saved.save(&path).map_err(|e| format!("the previous values could not be saved ({})", e))
    }

    // True when every saved value already equals what the PC has now, so restoring would change nothing.
    fn matches_saved(id: u32, state: &TweakState) -> bool {
        for t in Self::targets_for(id) {
            match (Self::read_saved(&t), state.values.get(&Self::target_id(&t))) {
                (Ok(now), Some(saved)) if now == *saved => {}
                _ => return false, // different, not saved, or not readable: do not claim it matches
            }
        }
        if id == 1 {
            if let Some(plan) = &state.power_plan {
                if Self::active_power_scheme() != plan.to_lowercase() {
                    return false;
                }
            }
        }
        if id == 5 {
            if let Some(on) = state.hibernate {
                if Self::get_registry_dword(HKEY_LOCAL_MACHINE, POWER_KEY, "HibernateEnabled", NOT_FOUND) != u32::from(on) {
                    return false;
                }
            }
        }
        true
    }

    // Read-only. Would putting back the saved values overwrite something changed after the app set it?
    // An unreadable saved-values file counts as "nothing saved" here; `restore_saved` reports the real problem.
    pub fn restore_check(id: u32) -> RestoreCheck {
        let saved = journal::default_path().and_then(|p| Journal::load(&p).ok()).and_then(|j| j.tweaks.get(&id).cloned());
        match saved {
            None => restore_check(false, false, false),
            Some(state) => restore_check(true, Self::matches_saved(id, &state), Self::values_set_by_app(id)),
        }
    }

    // Puts back what `remember_before` saved. With nothing saved it changes nothing and says so.
    pub fn restore_saved(id: u32) -> TweakResult {
        let Some(path) = journal::default_path() else {
            return TweakResult::skipped("LOCALAPPDATA is not set; nothing was restored.");
        };
        let mut saved = match Journal::load(&path) {
            Ok(j) => j,
            Err(e) => return TweakResult::skipped(format!("The saved-values file cannot be read ({}); nothing was restored.", e)),
        };
        let Some(state) = saved.tweaks.get(&id).cloned() else {
            return TweakResult::skipped("No saved previous value for this tweak, so nothing was restored. The app restores only what it saved before changing it.");
        };

        let (mut good, mut total) = (0, 0);
        for t in Self::targets_for(id) {
            total += 1;
            match state.values.get(&Self::target_id(&t)) {
                Some(v) if Self::write_saved(&t, v.as_ref()) => good += 1,
                _ => {} // not saved, or not confirmed after writing
            }
        }
        if id == 1 {
            if let Some(plan) = &state.power_plan {
                total += 1;
                let ran = Self::system32("powercfg.exe", &["-setactive", plan], 20).succeeded();
                if ran && Self::active_power_scheme() == plan.to_lowercase() {
                    good += 1;
                }
            }
        }
        if id == 5 {
            if let Some(on) = state.hibernate {
                total += 1;
                if Self::set_hibernation(on).status == Status::Ok {
                    good += 1;
                }
            }
        }

        let mut result = TweakResult::from_counts(good, total, format!("Put back {} of {} saved values.", good, total));
        if result.status == Status::Ok {
            saved.tweaks.remove(&id);
            let _ = saved.save(&path); // if this fails, the next restore writes the same values again
            if id == 4 {
                result = TweakResult::partial("Widgets button put back. The removed Widgets pack is not reinstalled; get it from the Microsoft Store if you want it back.");
            }
        }
        result
    }

    // 01. Power plan: High Performance + Power Throttling off (needs admin; throttling flag needs a restart)
    pub fn optimize_power_plan() -> TweakResult {
        let plan_ran = Self::system32("powercfg.exe", &["-setactive", HIGH_PERFORMANCE], 20).succeeded();
        let plan_confirmed = plan_ran && Self::active_power_scheme() == HIGH_PERFORMANCE;
        let throttle_set = Self::set_dword_verified(HKEY_LOCAL_MACHINE, POWER_THROTTLING_KEY, "PowerThrottlingOff", 1);

        if plan_confirmed && throttle_set {
            TweakResult::ok("High Performance plan is active. Power Throttling flag set; it applies after a restart.")
        } else if !plan_ran && !throttle_set {
            TweakResult::failed("Nothing changed. This PC may not offer the High Performance plan (common with Modern Standby laptops).")
        } else {
            let plan = if plan_confirmed {
                "High Performance plan is active"
            } else if plan_ran {
                "Plan switch was accepted but could not be confirmed"
            } else {
                "High Performance plan not available on this PC, plan unchanged"
            };
            let throttle = if throttle_set { "Power Throttling flag set (applies after restart)" } else { "Power Throttling flag not saved" };
            TweakResult::partial(format!("{}; {}.", plan, throttle))
        }
    }

    // 02. Game DVR background capture
    pub fn optimize_game_dvr() -> TweakResult {
        const GCS: &str = "System\\GameConfigStore";
        const DVR: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR";
        let checks = [
            Self::set_dword_verified(HKEY_CURRENT_USER, GCS, "GameDVR_Enabled", 0),
            Self::set_dword_verified(HKEY_CURRENT_USER, GCS, "GameDVR_FSEBehaviorMode", 2),
            Self::set_dword_verified(HKEY_CURRENT_USER, GCS, "GameDVR_HonorUserFSEBehaviorMode", 1),
            Self::set_dword_verified(HKEY_CURRENT_USER, DVR, "AppCaptureEnabled", 0),
            Self::set_dword_verified(HKEY_CURRENT_USER, DVR, "HistoricalCaptureEnabled", 0),
        ];
        let passed = checks.iter().filter(|c| **c).count();
        TweakResult::from_counts(passed, checks.len(), format!("Game DVR settings saved: {} of {}.", passed, checks.len()))
    }

    // 03. Context menu delay
    pub fn optimize_menu_delay() -> TweakResult {
        if Self::set_string_verified(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay", "20") {
            TweakResult::ok("MenuShowDelay saved as 20 ms. You may need to sign out and back in to see it.")
        } else {
            TweakResult::failed("Could not save MenuShowDelay.")
        }
    }

    // 04. Widgets: hide the taskbar button, stop the service, remove the Web Experience pack (needs admin).
    // The pack is removed only through the System32 PowerShell; no program is started from a user-writable folder.
    pub fn optimize_widgets() -> TweakResult {
        let button_hidden = Self::set_dword_verified(
            HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
            "TaskbarDa",
            0,
        );
        // Not running is fine, so the result of taskkill is ignored.
        let _ = Self::system32("taskkill.exe", &["/f", "/im", "WidgetService.exe"], 10);

        // Removal runs only when the pack is known to be installed, and "removed" is claimed only after Windows said so.
        let mut pack = Self::webexperience_state();
        if pack == PackState::Installed {
            // current user only; the pack may stay installed for other accounts
            let _ = Self::powershell(&format!("Get-AppxPackage -Name {} | Remove-AppxPackage", WEBEXPERIENCE), 120);
            pack = Self::webexperience_state();
        }

        match (button_hidden, pack) {
            (true, PackState::Gone) => TweakResult::ok("Widgets button hidden and the Widgets pack is not installed for this account."),
            (true, PackState::Installed) => TweakResult::partial("Widgets button hidden, but the Widgets pack is still installed."),
            (true, PackState::Unknown) => TweakResult::partial("Widgets button hidden, but Windows could not tell whether the Widgets pack is installed."),
            (false, PackState::Gone) => TweakResult::partial("Widgets pack is not installed for this account. Windows 11 did not allow the Widgets button to be changed by an app (it protects that setting); hide it in Settings > Personalization > Taskbar."),
            (false, _) => TweakResult::failed("Windows did not allow the Widgets button to be changed by an app (Windows 11 protects it), and the Widgets pack was not confirmed removed."),
        }
    }

    // 05. Turn hibernation (and Fast Startup) off (needs admin)
    pub fn optimize_fast_startup() -> TweakResult {
        Self::set_hibernation(false)
    }

    fn set_hibernation(on: bool) -> TweakResult {
        let outcome = Self::system32("powercfg.exe", &["-h", if on { "on" } else { "off" }], 30);
        let state = Self::get_registry_dword(HKEY_LOCAL_MACHINE, POWER_KEY, "HibernateEnabled", NOT_FOUND);
        let word = if on { "on" } else { "off" };
        if outcome.succeeded() && state == on as u32 {
            TweakResult::ok(format!("Hibernation turned {}.", word))
        } else if outcome.succeeded() {
            TweakResult::partial(format!("powercfg reported success but hibernation could not be confirmed {}.", word))
        } else {
            TweakResult::failed(format!("Hibernation not changed: powercfg {}.", outcome.describe()))
        }
    }

    // 06. Bing results in Start search
    pub fn optimize_bing_search() -> TweakResult {
        const KEY: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Search";
        let saved = Self::set_dword_verified(HKEY_CURRENT_USER, KEY, "BingSearchEnabled", 0);
        let _ = Self::system32("taskkill.exe", &["/f", "/im", "SearchHost.exe"], 10); // restarts on its own
        if saved {
            TweakResult::ok("Bing search setting saved. Start may need a moment or a sign-out to reflect it.")
        } else {
            TweakResult::failed("Could not save the Bing search setting.")
        }
    }

    // 07. Delivery Optimization: HTTP-only download mode, written as the machine policy (needs admin).
    // Tested on Windows 11: the old per-user key left the mode at "Lan"; this policy makes Windows report "CdnOnly".
    pub fn optimize_delivery_optimization() -> TweakResult {
        if !Self::set_dword_verified(HKEY_LOCAL_MACHINE, DELIVERY_POLICY, "DODownloadMode", 0) {
            return TweakResult::failed("Could not save the Delivery Optimization policy.");
        }
        if Self::delivery_mode_is_http_only() {
            TweakResult::ok("Delivery Optimization is HTTP only (checked with Windows). Settings may now say some settings are managed by your organization.")
        } else {
            TweakResult::partial("Policy saved, but Windows did not report HTTP-only mode yet. It may need a moment or a restart.")
        }
    }

    // Asks Windows itself which mode is in effect, and waits up to about 10 s for the service to pick up a change.
    fn delivery_mode_is_http_only() -> bool {
        let script = "for ($i=0; $i -lt 5; $i++) { try { if ((Get-DOConfig).DownloadMode -in 'CdnOnly','Http') { exit 0 } } catch {}; Start-Sleep -Seconds 2 }; exit 1";
        Self::powershell(script, 30).succeeded()
    }

    // The values the app writes for a tweak, read from the registry and power settings only (fast, no Windows service calls).
    // One list of the wanted numbers: `already_applied` and Restore's "changed since?" check both use it.
    // Reads only. Anything that cannot be read counts as "not set".
    fn values_set_by_app(id: u32) -> bool {
        const GCS: &str = "System\\GameConfigStore";
        const DVR: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR";
        const EXPLORER: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
        const SEARCH: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Search";
        let is = |root: HKEY, key: &str, name: &str, want: u32| Self::get_registry_dword(root, key, name, NOT_FOUND) == want;
        match id {
            1 => Self::active_power_scheme() == HIGH_PERFORMANCE && is(HKEY_LOCAL_MACHINE, POWER_THROTTLING_KEY, "PowerThrottlingOff", 1),
            2 => {
                is(HKEY_CURRENT_USER, GCS, "GameDVR_Enabled", 0)
                    && is(HKEY_CURRENT_USER, GCS, "GameDVR_FSEBehaviorMode", 2)
                    && is(HKEY_CURRENT_USER, GCS, "GameDVR_HonorUserFSEBehaviorMode", 1)
                    && is(HKEY_CURRENT_USER, DVR, "AppCaptureEnabled", 0)
                    && is(HKEY_CURRENT_USER, DVR, "HistoricalCaptureEnabled", 0)
            }
            3 => Self::get_registry_string(HKEY_CURRENT_USER, "Control Panel\\Desktop", "MenuShowDelay") == "20",
            4 => is(HKEY_CURRENT_USER, EXPLORER, "TaskbarDa", 0),
            5 => is(HKEY_LOCAL_MACHINE, POWER_KEY, "HibernateEnabled", 0),
            6 => is(HKEY_CURRENT_USER, SEARCH, "BingSearchEnabled", 0),
            7 => is(HKEY_LOCAL_MACHINE, DELIVERY_POLICY, "DODownloadMode", 0),
            _ => false, // 8 (temp files) can always find something new to remove
        }
    }

    // True when the tweak's wanted state is already in place, so applying it would change nothing.
    // Tweaks 4 and 7 also ask Windows (slower), because the registry value alone does not prove the result.
    pub fn already_applied(id: u32) -> bool {
        match id {
            4 => Self::values_set_by_app(4) && Self::webexperience_state() == PackState::Gone,
            7 => Self::values_set_by_app(7) && Self::delivery_mode_is_http_only(),
            _ => Self::values_set_by_app(id),
        }
    }

    // 08. Delete temp files untouched for 24 hours, only inside the current user's Temp folder
    pub fn optimize_temp_files() -> TweakResult {
        let (Ok(temp), Ok(local)) = (std::env::var("TEMP"), std::env::var("LOCALAPPDATA")) else {
            return TweakResult::skipped("TEMP or LOCALAPPDATA is not set; nothing was deleted.");
        };
        let allowed = [PathBuf::from(local).join("Temp")];
        if !dir_is_allowed(Path::new(&temp), &allowed) {
            return TweakResult::skipped("TEMP does not point to your own Temp folder; nothing was deleted.");
        }
        match purge_old_entries(Path::new(&temp), Duration::from_secs(24 * 3600), SystemTime::now()) {
            Ok(r) => TweakResult::ok(format!(
                "Removed {} items older than 24 h; kept {} recent; {} in use or protected.",
                r.deleted, r.kept_recent, r.failed + r.skipped_links
            )),
            Err(e) => TweakResult::failed(format!("Could not read the Temp folder: {}", e)),
        }
    }

    // (total bytes, available bytes, load percent) from Windows, or None when it does not answer.
    fn memory_status() -> Option<(u64, u64, u32)> {
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
        unsafe {
            let mut s: MemoryStatusEx = std::mem::zeroed();
            s.dw_length = std::mem::size_of::<MemoryStatusEx>() as u32;
            (GlobalMemoryStatusEx(&mut s) != 0).then_some((s.ull_total_phys, s.ull_avail_phys, s.dw_memory_load))
        }
    }

    // (free bytes available to the user, total bytes) of the system drive, or None when Windows does not answer.
    fn system_drive_space() -> Option<(u64, u64)> {
        extern "system" {
            fn GetDiskFreeSpaceExW(root: *const u16, free_to_caller: *mut u64, total: *mut u64, total_free: *mut u64) -> i32;
        }
        let root = Self::system_drive_root();
        let (mut free, mut total, mut total_free) = (0u64, 0u64, 0u64);
        unsafe { (GetDiskFreeSpaceExW(root.as_ptr(), &mut free, &mut total, &mut total_free) != 0).then_some((free, total)) }
    }

    // Dynamic hardware telemetry across all Windows laptops (Intel, AMD, ARM)
    pub fn get_hardware_info() -> HardwareInfo {
        // 1. CPU Name from registry
        let mut cpu = Self::get_registry_string(
            HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            "ProcessorNameString",
        );
        if cpu.trim().is_empty() {
            cpu = "Unknown processor".into();
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
            .map(|n| n.get());
        let cores_str = match threads {
            Ok(n) => format!("[{} Threads]", n),
            Err(_) => "[threads unknown]".to_string(),
        };

        // 3. RAM and 4. system drive size, rounded to whole GB; 0 means it could not be read
        const GB: f64 = 1024.0 * 1024.0 * 1024.0;
        let ram_gb = Self::memory_status().map_or(0, |(total, _, _)| (total as f64 / GB).round() as u64);
        let disk_gb = Self::system_drive_space().map_or(0, |(_, total)| (total as f64 / GB).round() as u64);

        // 5. System Product / Model
        let product = Self::get_registry_string(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName");
        let maker = Self::get_registry_string(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer");
        let product = text_or_unknown(&product, "");
        let maker = text_or_unknown(&maker, "");
        let model = match (maker.is_empty(), product.is_empty()) {
            (false, false) => format!("{} {}", maker, product),
            (true, false) => product,
            (false, true) => maker,
            (true, true) => "Unknown model".to_string(),
        };

        let specs_str = format_specs(ram_gb, disk_gb);

        HardwareInfo {
            model: format!("// {}", model.to_uppercase()),
            cpu_name: cpu_clean,
            cores: cores_str,
            specs: specs_str,
        }
    }

    pub fn get_live_telemetry() -> LiveTelemetry {
        // 1. Nominal / Base CPU Clock in MHz from Windows Registry
        let cpu_mhz = Self::get_registry_dword(
            HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            "~MHz",
            0, // 0 = could not be read
        );

        // 2. RAM and 3. system-drive space. 0 means "could not be read"; the window then shows "--", never a made-up number.
        const GB: f32 = 1024.0 * 1024.0 * 1024.0;
        let round1 = |v: f32| (v * 10.0).round() / 10.0;
        let (ram_total_gb, ram_used_gb, ram_usage_pct) = match Self::memory_status() {
            Some((total, avail, load)) => {
                let total = total as f32 / GB;
                (round1(total), round1((total - avail as f32 / GB).max(0.0)), load)
            }
            None => (0.0, 0.0, 0),
        };
        let (ssd_free_gb, ssd_total_gb) = match Self::system_drive_space() {
            Some((free, total)) => (round1(free as f32 / GB), round1(total as f32 / GB)),
            None => (0.0, 0.0),
        };

        LiveTelemetry {
            cpu_mhz,
            ram_used_gb,
            ram_total_gb,
            ram_usage_pct,
            ssd_free_gb,
            ssd_total_gb,
            cpu_load: Self::cpu_load(),
        }
    }

    // (idle time, total time) of every logical processor, from Windows (the source of Task Manager's per-core view).
    // None when Windows does not answer.
    fn processor_times() -> Option<Vec<(u64, u64)>> {
        #[repr(C)]
        struct ProcessorPerf {
            idle: i64,
            kernel: i64, // includes idle time
            user: i64,
            dpc: i64,
            interrupt: i64,
            interrupt_count: u32,
        }
        #[link(name = "ntdll")]
        extern "system" {
            fn NtQuerySystemInformation(class: u32, buffer: *mut std::ffi::c_void, len: u32, returned: *mut u32) -> i32;
        }
        const SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION: u32 = 8;
        const LENGTH_MISMATCH: i32 = 0xC000_0004u32 as i32;
        let size = std::mem::size_of::<ProcessorPerf>();
        let mut slots = std::thread::available_parallelism().map_or(8, |n| n.get()).max(1);
        for _ in 0..4 {
            let mut buf: Vec<ProcessorPerf> = (0..slots).map(|_| unsafe { std::mem::zeroed() }).collect();
            let mut returned = 0u32;
            let status = unsafe {
                NtQuerySystemInformation(
                    SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION,
                    buf.as_mut_ptr().cast(),
                    (slots * size) as u32,
                    &mut returned,
                )
            };
            if status == 0 {
                let n = (returned as usize / size).min(slots);
                return Some(buf[..n].iter().map(|p| (p.idle as u64, (p.kernel + p.user) as u64)).collect());
            }
            if status != LENGTH_MISMATCH {
                return None;
            }
            slots *= 2;
        }
        None
    }

    // Load of each logical processor since the previous call. The first call takes a short second sample,
    // so even the first number is a real recent load and not an average since boot.
    fn cpu_load() -> Vec<u8> {
        static PREVIOUS: std::sync::Mutex<Vec<(u64, u64)>> = std::sync::Mutex::new(Vec::new());
        let Some(now) = Self::processor_times() else { return Vec::new() };
        let mut previous = PREVIOUS.lock().unwrap_or_else(|e| e.into_inner());
        if previous.len() != now.len() {
            *previous = now;
            std::thread::sleep(Duration::from_millis(120));
            let Some(later) = Self::processor_times().filter(|l| l.len() == previous.len()) else {
                previous.clear();
                return Vec::new();
            };
            let load = previous.iter().zip(&later).map(|(a, b)| load_percent(*a, *b)).collect();
            *previous = later;
            return load;
        }
        let load = previous.iter().zip(&now).map(|(a, b)| load_percent(*a, *b)).collect();
        *previous = now;
        load
    }

    // --- Startup Applications Management ---
    pub fn is_startup_app_enabled(root: HKEY, app_name: &str) -> bool {
        let hk = RegKey::predef(root);
        let subkey = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
        if let Ok(key) = hk.open_subkey_with_flags(subkey, KEY_READ) {
            if let Ok(data) = key.get_raw_value(app_name) {
                return startup_entry_enabled(data.bytes.first().copied());
            }
        }
        true
    }

    // Turns a startup entry on or off. The entry must already exist in that hive's Run key,
    // and the new state is read back before success is reported.
    pub fn set_startup_app_enabled(hive: StartupHive, app_name: &str, enable: bool) -> TweakResult {
        if !valid_startup_name(app_name) {
            return TweakResult::failed("Not a valid startup entry name.");
        }
        let (root, run_key) = match hive {
            StartupHive::CurrentUser => (HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
            StartupHive::LocalMachine => (HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
        };
        if hive == StartupHive::LocalMachine && !Self::is_elevated() {
            return TweakResult::skipped("Changing all-users startup entries needs administrator rights.");
        }
        let exists = RegKey::predef(root)
            .open_subkey_with_flags(run_key, KEY_READ)
            .map(|k| k.enum_values().flatten().any(|(n, _)| n == app_name))
            .unwrap_or(false);
        if !exists {
            return TweakResult::failed("That startup entry no longer exists.");
        }
        let subkey = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
        let Ok((key, _)) = RegKey::predef(root).create_subkey(subkey) else {
            return TweakResult::failed("Could not open the startup settings key.");
        };
        let mut bytes = vec![0u8; 12];
        bytes[0] = if enable { 0x02 } else { 0x03 };
        let wrote = key.set_raw_value(app_name, &winreg::RegValue { vtype: REG_BINARY, bytes }).is_ok();
        if wrote && Self::is_startup_app_enabled(root, app_name) == enable {
            TweakResult::ok(if enable { "Startup entry enabled." } else { "Startup entry disabled." })
        } else {
            TweakResult::failed("Windows did not accept the change to the startup entry.")
        }
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

        apps
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // A scratch key under HKCU, never one of the app's real settings.
    fn scratch_key() -> String {
        format!("Software\\LaptopOptimizerTest\\{}", std::process::id())
    }

    #[test]
    fn restore_puts_back_exact_values_and_deletes_values_that_did_not_exist() {
        let key = scratch_key();
        let (a, b, c) = (
            Target { root: HKEY_CURRENT_USER, key: &key, name: "A_dword" },
            Target { root: HKEY_CURRENT_USER, key: &key, name: "B_text" },
            Target { root: HKEY_CURRENT_USER, key: &key, name: "C_missing" },
        );
        assert!(OptimizerEngine::set_registry_dword(HKEY_CURRENT_USER, &key, "A_dword", 5));
        assert!(OptimizerEngine::set_registry_string(HKEY_CURRENT_USER, &key, "B_text", "three hundred"));

        // save the "before" state
        let before: Vec<_> = [&a, &b, &c].iter().map(|t| OptimizerEngine::read_saved(t).unwrap()).collect();
        assert!(before[0].is_some() && before[1].is_some() && before[2].is_none());

        // the tweak changes all three, and creates the one that did not exist
        assert!(OptimizerEngine::set_registry_dword(HKEY_CURRENT_USER, &key, "A_dword", 9));
        assert!(OptimizerEngine::set_registry_string(HKEY_CURRENT_USER, &key, "B_text", "20"));
        assert!(OptimizerEngine::set_registry_dword(HKEY_CURRENT_USER, &key, "C_missing", 1));

        // restore
        for (t, v) in [&a, &b, &c].iter().zip(&before) {
            assert!(OptimizerEngine::write_saved(t, v.as_ref()), "{}", t.name);
        }
        let after: Vec<_> = [&a, &b, &c].iter().map(|t| OptimizerEngine::read_saved(t).unwrap()).collect();
        assert_eq!(after, before, "same types and same bytes");
        assert_eq!(after[2], None, "a value that did not exist is deleted again, not written");

        let _ = RegKey::predef(HKEY_CURRENT_USER).delete_subkey_all("Software\\LaptopOptimizerTest");
    }

    #[test]
    fn every_registry_target_is_in_the_known_list() {
        // Restore may only write these; the saved file cannot add to them.
        let all: Vec<String> = (1..=8).flat_map(|id| OptimizerEngine::targets_for(id)).map(|t| OptimizerEngine::target_id(&t)).collect();
        assert_eq!(all.len(), 1 + 5 + 1 + 1 + 1 + 1, "tweaks 1,2,3,4,6,7");
        let hklm_allowed = [
            "HKLM\\SYSTEM\\CurrentControlSet\\Control\\Power\\PowerThrottling\\PowerThrottlingOff",
            "HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization\\DODownloadMode",
        ];
        assert!(all.iter().all(|t| t.starts_with("HKCU\\") || hklm_allowed.contains(&t.as_str())), "{all:?}");
    }

    // Opt-in system test for tweak 7 (same command and rules as above). It sets the real policy for a few seconds,
    // asks Windows what it reports, restores, and removes the policy again if it did not exist before.
    #[test]
    #[ignore]
    fn system_roundtrip_delivery_optimization() {
        assert!(OptimizerEngine::is_elevated(), "run this test as administrator");
        let scratch = std::env::temp_dir().join(format!("lo-system-test-do-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&scratch);
        std::env::set_var("LOCALAPPDATA", &scratch);
        let target = Target { root: HKEY_LOCAL_MACHINE, key: DELIVERY_POLICY, name: "DODownloadMode" };
        struct Undo(Option<SavedValue>);
        impl Drop for Undo {
            fn drop(&mut self) {
                let t = Target { root: HKEY_LOCAL_MACHINE, key: DELIVERY_POLICY, name: "DODownloadMode" };
                let _ = OptimizerEngine::write_saved(&t, self.0.as_ref());
            }
        }
        let _undo = Undo(OptimizerEngine::read_saved(&target).unwrap());

        assert!(!OptimizerEngine::already_applied(7), "test setup: start from a PC where it is not applied");
        OptimizerEngine::remember_before(7).unwrap();
        let applied = OptimizerEngine::optimize_delivery_optimization();
        assert_eq!(applied.status, Status::Ok, "{}", applied.message);
        assert!(OptimizerEngine::already_applied(7), "Windows reports HTTP-only, so it reads as applied");
        let again = crate::run_tweak(7, false, false);
        assert_eq!(again.status, Status::NoChange, "applying twice changes nothing: {}", again.message);
        let restored = OptimizerEngine::restore_saved(7);
        assert_eq!(restored.status, Status::Ok, "{}", restored.message);
        assert_eq!(OptimizerEngine::read_saved(&target).unwrap(), None, "the policy value did not exist before, so it is gone again");
        assert!(RegKey::predef(HKEY_LOCAL_MACHINE).open_subkey(DELIVERY_POLICY).is_err(), "no empty policy key is left behind");
        assert!(!OptimizerEngine::delivery_mode_is_http_only(), "Windows is back to its default mode");
        let _ = std::fs::remove_dir_all(&scratch);
    }

    // The per-processor load must follow real work: idle first, then every logical processor kept busy for a moment.
    #[test]
    fn cpu_load_follows_real_work() {
        use std::sync::atomic::{AtomicBool, Ordering};
        let n = std::thread::available_parallelism().unwrap().get();
        let first = OptimizerEngine::get_live_telemetry().cpu_load;
        assert_eq!(first.len(), n, "one value per logical processor");
        assert!(first.iter().all(|v| *v <= 100));

        let stop = AtomicBool::new(false);
        let busy: Vec<u8> = std::thread::scope(|s| {
            for _ in 0..n {
                s.spawn(|| {
                    let mut x = 1u64;
                    while !stop.load(Ordering::Relaxed) {
                        x = x.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
                        std::hint::black_box(x);
                    }
                });
            }
            std::thread::sleep(Duration::from_millis(400));
            OptimizerEngine::get_live_telemetry(); // start a fresh window while everything is busy
            std::thread::sleep(Duration::from_millis(1200));
            let reading = OptimizerEngine::get_live_telemetry().cpu_load;
            stop.store(true, Ordering::Relaxed);
            reading
        });
        let avg = busy.iter().map(|v| *v as u32).sum::<u32>() / busy.len().max(1) as u32;
        println!("CPU LOAD idle-ish {:?} | all busy {:?} | average while busy {}%", first, busy, avg);
        assert_eq!(busy.len(), n);
        assert!(avg >= 60, "all processors were busy but the average load read {}%", avg);
    }

    // Opt-in helper for comparing with Windows' own counter: prints the app's load for 6 s, once a second.
    //   cargo test --lib print_cpu_load -- --ignored --nocapture
    #[test]
    #[ignore]
    fn print_cpu_load() {
        OptimizerEngine::get_live_telemetry();
        let mut sums = vec![0u32; std::thread::available_parallelism().unwrap().get()];
        for _ in 0..6 {
            std::thread::sleep(Duration::from_secs(1));
            for (i, v) in OptimizerEngine::get_live_telemetry().cpu_load.iter().enumerate() {
                sums[i] += *v as u32;
            }
        }
        let avg: Vec<u32> = sums.iter().map(|s| s / 6).collect();
        println!("APP_LOAD_AVG_6S {:?} total {}%", avg, avg.iter().sum::<u32>() / avg.len() as u32);
    }

    #[test]
    fn telemetry_reads_real_and_consistent_values() {
        let t = OptimizerEngine::get_live_telemetry();
        let h = OptimizerEngine::get_hardware_info();
        println!("TELEMETRY ram {}/{} GB ({}%) | disk free {}/{} GB | cpu {} MHz | specs {:?}", t.ram_used_gb, t.ram_total_gb, t.ram_usage_pct, t.ssd_free_gb, t.ssd_total_gb, t.cpu_mhz, h.specs);
        assert!(t.ram_total_gb > 0.0 && t.ram_used_gb >= 0.0 && t.ram_used_gb <= t.ram_total_gb + 0.1);
        assert!(t.ram_usage_pct <= 100);
        assert!(t.ssd_total_gb > 0.0 && t.ssd_free_gb >= 0.0 && t.ssd_free_gb <= t.ssd_total_gb);
        assert!(h.specs.contains("GB RAM") && h.specs.contains("GB disk"), "{}", h.specs);
    }

    // Puts the PC back to how it started, even when an assert below fails.
    struct PutBack {
        plan: String,
        hibernate_on: bool,
        throttle: Option<SavedValue>,
    }

    impl Drop for PutBack {
        fn drop(&mut self) {
            let _ = OptimizerEngine::system32("powercfg.exe", &["-setactive", &self.plan], 20);
            let _ = OptimizerEngine::system32("powercfg.exe", &["-h", if self.hibernate_on { "on" } else { "off" }], 60);
            let t = Target { root: HKEY_LOCAL_MACHINE, key: POWER_THROTTLING_KEY, name: "PowerThrottlingOff" };
            let _ = OptimizerEngine::write_saved(&t, self.throttle.as_ref());
        }
    }

    // Opt-in system test (changes the real power plan and hibernation for a few seconds, then puts them back):
    //   cargo test --lib system_roundtrip -- --ignored --test-threads=1      (run as administrator)
    // The saved-values file goes to a scratch folder, never the real one.
    #[test]
    #[ignore]
    fn system_roundtrip_power_plan_and_hibernation() {
        const BALANCED: &str = "381b4222-f694-41f0-9685-ff5bb260df2e";
        assert!(OptimizerEngine::is_elevated(), "run this test as administrator");
        let scratch = std::env::temp_dir().join(format!("lo-system-test-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&scratch);
        std::env::set_var("LOCALAPPDATA", &scratch);
        let journal_path = journal::default_path().unwrap();

        let throttle_target = Target { root: HKEY_LOCAL_MACHINE, key: POWER_THROTTLING_KEY, name: "PowerThrottlingOff" };
        let _put_back = PutBack {
            plan: OptimizerEngine::active_power_scheme(),
            hibernate_on: OptimizerEngine::get_registry_dword(HKEY_LOCAL_MACHINE, POWER_KEY, "HibernateEnabled", NOT_FOUND) == 1,
            throttle: OptimizerEngine::read_saved(&throttle_target).unwrap(),
        };
        let start_throttle = _put_back.throttle.clone();
        assert!(valid_guid(&_put_back.plan), "could not read the starting power plan");

        // --- tweak 1: start on Balanced, apply, apply again, restore
        assert!(OptimizerEngine::system32("powercfg.exe", &["-setactive", BALANCED], 20).succeeded());
        assert_eq!(OptimizerEngine::active_power_scheme(), BALANCED, "test setup: Balanced is active");
        OptimizerEngine::remember_before(1).unwrap();
        let applied = OptimizerEngine::optimize_power_plan();
        assert_ne!(applied.status, Status::Failed, "{}", applied.message);
        assert_eq!(OptimizerEngine::active_power_scheme(), HIGH_PERFORMANCE, "apply switched to High performance");
        assert!(OptimizerEngine::already_applied(1), "after apply, the tweak reads as already applied");
        OptimizerEngine::remember_before(1).unwrap(); // a second deploy must not replace the original
        let restored = OptimizerEngine::restore_saved(1);
        assert_eq!(restored.status, Status::Ok, "{}", restored.message);
        assert_eq!(OptimizerEngine::active_power_scheme(), BALANCED, "restore went back to what was active before");
        assert!(!OptimizerEngine::already_applied(1), "after restore, the tweak is no longer applied");
        assert_eq!(OptimizerEngine::read_saved(&throttle_target).unwrap(), start_throttle, "throttling value exactly as before");
        assert!(!Journal::load(&journal_path).unwrap().tweaks.contains_key(&1), "entry removed after a full restore");
        assert_eq!(OptimizerEngine::restore_saved(1).status, Status::Skipped, "nothing saved, so nothing is changed");

        // --- tweak 5: make sure hibernation is ON first, apply (off), restore (on)
        let on = OptimizerEngine::set_hibernation(true);
        assert_eq!(on.status, Status::Ok, "test setup: {}", on.message);
        OptimizerEngine::remember_before(5).unwrap();
        let applied = OptimizerEngine::optimize_fast_startup();
        assert_eq!(applied.status, Status::Ok, "{}", applied.message);
        assert_eq!(OptimizerEngine::get_registry_dword(HKEY_LOCAL_MACHINE, POWER_KEY, "HibernateEnabled", NOT_FOUND), 0);
        assert!(OptimizerEngine::already_applied(5));
        let restored = OptimizerEngine::restore_saved(5);
        assert_eq!(restored.status, Status::Ok, "{}", restored.message);
        assert_eq!(OptimizerEngine::get_registry_dword(HKEY_LOCAL_MACHINE, POWER_KEY, "HibernateEnabled", NOT_FOUND), 1);

        let _ = std::fs::remove_dir_all(&scratch);
    }

    // Puts MenuShowDelay back to how it was, even when an assert below fails.
    struct PutBackDelay(Option<SavedValue>);
    impl Drop for PutBackDelay {
        fn drop(&mut self) {
            let t = Target { root: HKEY_CURRENT_USER, key: "Control Panel\\Desktop", name: "MenuShowDelay" };
            let _ = OptimizerEngine::write_saved(&t, self.0.as_ref());
        }
    }

    fn sz(text: &str) -> SavedValue {
        let bytes: Vec<u8> = text.encode_utf16().chain(std::iter::once(0)).flat_map(|u| u.to_le_bytes()).collect();
        SavedValue { vtype: 1, bytes }
    }

    // Opt-in system test (changes only your own MenuShowDelay for a moment, then puts it back):
    //   cargo test --lib system_restore_check -- --ignored --test-threads=1
    // The saved-values file goes to a scratch folder, never the real one.
    #[test]
    #[ignore]
    fn system_restore_check_notices_a_value_changed_after_the_app_set_it() {
        let scratch = std::env::temp_dir().join(format!("lo-restorecheck-test-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&scratch);
        std::env::set_var("LOCALAPPDATA", &scratch);
        let t = Target { root: HKEY_CURRENT_USER, key: "Control Panel\\Desktop", name: "MenuShowDelay" };
        let _put_back = PutBackDelay(OptimizerEngine::read_saved(&t).unwrap());

        assert!(OptimizerEngine::write_saved(&t, Some(&sz("400"))), "test setup: start from 400");
        assert_eq!(OptimizerEngine::restore_check(3), RestoreCheck::NothingSaved, "nothing saved yet");
        OptimizerEngine::remember_before(3).unwrap();
        assert_ne!(OptimizerEngine::optimize_menu_delay().status, Status::Failed);
        assert_eq!(OptimizerEngine::restore_check(3), RestoreCheck::SafeToRestore, "still what the app set: no question");

        assert!(OptimizerEngine::write_saved(&t, Some(&sz("100"))), "the user picks their own value");
        assert_eq!(OptimizerEngine::restore_check(3), RestoreCheck::ChangedSince, "changed by hand: ask first");

        assert!(OptimizerEngine::write_saved(&t, Some(&sz("400"))), "the user sets it back to the original");
        assert_eq!(OptimizerEngine::restore_check(3), RestoreCheck::SafeToRestore, "already the original: nothing is overwritten");

        assert!(OptimizerEngine::write_saved(&t, Some(&sz("100"))));
        let declined = crate::run_tweak(3, true, false);
        assert_eq!(declined.status, Status::Skipped, "overwriting was not agreed: {}", declined.message);
        assert_eq!(OptimizerEngine::read_saved(&t).unwrap(), Some(sz("100")), "the user's own value is untouched");
        assert_eq!(OptimizerEngine::restore_check(3), RestoreCheck::ChangedSince, "the saved entry is kept, so Restore can ask again");
        let restored = crate::run_tweak(3, true, true);
        assert_eq!(restored.status, Status::Ok, "agreed to overwrite: {}", restored.message);
        assert_eq!(OptimizerEngine::read_saved(&t).unwrap(), Some(sz("400")));
        assert_eq!(OptimizerEngine::restore_check(3), RestoreCheck::NothingSaved, "the saved entry is used up");

        let _ = std::fs::remove_dir_all(&scratch);
    }
}

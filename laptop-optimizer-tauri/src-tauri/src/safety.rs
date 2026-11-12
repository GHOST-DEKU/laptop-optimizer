//! Safety helpers that use only `std` and `serde`, so their tests run anywhere.
//! The app itself is Windows-only; nothing here adds support for other systems.

use serde::Serialize;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::{Duration, Instant, SystemTime};

/// How a tweak ended. `Ok` is used only when the change was read back and matches.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Status {
    Ok,
    Partial,
    Skipped,
    Failed,
}

#[derive(Debug, Clone, Serialize)]
pub struct TweakResult {
    pub status: Status,
    pub message: String,
}

impl TweakResult {
    pub fn ok(msg: impl Into<String>) -> Self {
        Self { status: Status::Ok, message: msg.into() }
    }
    pub fn partial(msg: impl Into<String>) -> Self {
        Self { status: Status::Partial, message: msg.into() }
    }
    pub fn skipped(msg: impl Into<String>) -> Self {
        Self { status: Status::Skipped, message: msg.into() }
    }
    pub fn failed(msg: impl Into<String>) -> Self {
        Self { status: Status::Failed, message: msg.into() }
    }
    /// `Ok` when every check passed, `Partial` when some did, `Failed` when none did.
    pub fn from_counts(passed: usize, total: usize, msg: impl Into<String>) -> Self {
        let status = if total > 0 && passed == total {
            Status::Ok
        } else if passed > 0 {
            Status::Partial
        } else {
            Status::Failed
        };
        Self { status, message: msg.into() }
    }
}

/// What happened when a program was started.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Outcome {
    Success,
    ExitCode(i32),
    TimedOut,
    CouldNotStart(String),
}

impl Outcome {
    pub fn succeeded(&self) -> bool {
        matches!(self, Outcome::Success)
    }
    pub fn describe(&self) -> String {
        match self {
            Outcome::Success => "succeeded".into(),
            Outcome::ExitCode(c) => format!("exit code {}", c),
            Outcome::TimedOut => "timed out and was stopped".into(),
            Outcome::CouldNotStart(e) => format!("could not start ({})", e),
        }
    }
}

/// Runs `cmd` and stops it if it takes longer than `timeout`.
/// Output is discarded; only the exit status is kept.
/// shortcut: a timeout kills the program itself, not any helper it started; upgrade with a Windows job object if that matters.
pub fn run_with_timeout(mut cmd: Command, timeout: Duration) -> Outcome {
    cmd.stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null());
    let mut child = match cmd.spawn() {
        Ok(c) => c,
        Err(e) => return Outcome::CouldNotStart(e.to_string()),
    };
    let start = Instant::now();
    loop {
        match child.try_wait() {
            Ok(Some(status)) => {
                return if status.success() {
                    Outcome::Success
                } else {
                    Outcome::ExitCode(status.code().unwrap_or(-1))
                };
            }
            Ok(None) => {
                if start.elapsed() >= timeout {
                    let _ = child.kill();
                    let _ = child.wait();
                    return Outcome::TimedOut;
                }
                std::thread::sleep(Duration::from_millis(50));
            }
            Err(e) => {
                let _ = child.kill();
                return Outcome::CouldNotStart(e.to_string());
            }
        }
    }
}


/// One "a change is running" flag. Changing commands take it, so two never run at once.
pub struct Lock(AtomicBool);

pub struct LockGuard<'a>(&'a Lock);

impl Lock {
    pub const fn new() -> Self {
        Lock(AtomicBool::new(false))
    }
    /// `None` when another change already holds it. Released when the guard drops, even after a panic.
    pub fn try_acquire(&self) -> Option<LockGuard<'_>> {
        self.0
            .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
            .ok()
            .map(|_| LockGuard(self))
    }
}

impl Drop for LockGuard<'_> {
    fn drop(&mut self) {
        self.0 .0.store(false, Ordering::Release);
    }
}

pub static CHANGE_LOCK: Lock = Lock::new();

/// Makes a child program ignore a poisoned environment: it starts in System32,
/// and SystemRoot, windir and PATH are set to the real Windows folder.
/// shortcut: the rest of the environment is inherited (some tools need profile variables); tighten after a Windows test.
pub fn harden(cmd: &mut Command, system_root: &str) {
    let root = system_root.trim_end_matches(['\\', '/']);
    let sys32 = format!("{}\\System32", root);
    cmd.current_dir(&sys32)
        .env("SystemRoot", root)
        .env("windir", root)
        .env("PATH", format!("{};{}", sys32, root))
        .env_remove("PSModulePath");
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StartupHive {
    CurrentUser,
    LocalMachine,
}

impl StartupHive {
    /// Only the two exact names the page sends are accepted; anything else is refused.
    pub fn parse(s: &str) -> Option<Self> {
        match s {
            "HKCU" => Some(Self::CurrentUser),
            "HKLM" => Some(Self::LocalMachine),
            _ => None,
        }
    }
}

/// A registry value name for a startup entry: 1-260 characters, no control characters.
pub fn valid_startup_name(name: &str) -> bool {
    !name.is_empty() && name.chars().count() <= 260 && !name.chars().any(|c| c.is_control())
}

/// "• 16 GB RAM • 477 GB disk"; a size of 0 means it could not be read, so it says so.
pub fn format_specs(ram_gb: u64, disk_gb: u64) -> String {
    let ram = if ram_gb > 0 { format!("{} GB RAM", ram_gb) } else { "RAM unknown".to_string() };
    let disk = if disk_gb > 0 { format!("{} GB disk", disk_gb) } else { "disk unknown".to_string() };
    format!("\u{2022} {} \u{2022} {}", ram, disk)
}

/// Text read from the BIOS, or `unknown` when it is empty or one of the filler strings makers leave in.
pub fn text_or_unknown(raw: &str, unknown: &str) -> String {
    let t = raw.trim();
    const FILLER: [&str; 6] = ["system product name", "system manufacturer", "default string", "to be filled by o.e.m.", "not applicable", "none"];
    if t.is_empty() || FILLER.contains(&t.to_lowercase().as_str()) {
        unknown.to_string()
    } else {
        t.to_string()
    }
}

/// Tweaks that remove or turn off a Windows feature must be confirmed for that exact operation.
/// The backend refuses them without the matching word, so a stray call cannot run them.
pub fn required_confirmation(id: u32) -> Option<&'static str> {
    match id {
        4 => Some("remove-widgets"),
        5 => Some("turn-off-hibernation"),
        _ => None,
    }
}

pub fn confirmation_ok(id: u32, given: Option<&str>) -> bool {
    match required_confirmation(id) {
        None => true,
        Some(word) => given == Some(word),
    }
}

/// Tweaks that cannot work without administrator rights (HKLM writes, hibernation, app removal).
/// Restoring the Widgets button only touches the user's own registry, so it needs no admin.
pub fn requires_admin(id: u32, restoring: bool) -> bool {
    match id {
        1 | 5 => true,
        4 => !restoring,
        _ => false,
    }
}

/// `<SystemRoot>\System32\<name>`, written with backslashes because the target is Windows.
pub fn system32_exe(system_root: &str, name: &str) -> String {
    format!("{}\\System32\\{}", system_root.trim_end_matches(['\\', '/']), name)
}

pub fn powershell_exe(system_root: &str) -> String {
    system32_exe(system_root, "WindowsPowerShell\\v1.0\\powershell.exe")
}

/// True only if `dir` is the same folder as one of `allowed` (after resolving links and 8.3 names).
pub fn dir_is_allowed(dir: &Path, allowed: &[PathBuf]) -> bool {
    let Ok(real) = fs::canonicalize(dir) else { return false };
    allowed
        .iter()
        .filter_map(|a| fs::canonicalize(a).ok())
        .any(|a| a == real)
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct PurgeReport {
    pub deleted: usize,
    pub kept_recent: usize,
    pub skipped_links: usize,
    pub failed: usize,
}

/// Deletes entries directly inside `dir` that were last changed at least `min_age` before `now`.
/// Links (symbolic links and junctions) are never followed or removed.
pub fn purge_old_entries(dir: &Path, min_age: Duration, now: SystemTime) -> std::io::Result<PurgeReport> {
    let mut report = PurgeReport::default();
    for entry in fs::read_dir(dir)?.flatten() {
        let path = entry.path();
        let Ok(file_type) = entry.file_type() else {
            report.failed += 1;
            continue;
        };
        if file_type.is_symlink() {
            report.skipped_links += 1;
            continue;
        }
        let old_enough = entry
            .metadata()
            .and_then(|m| m.modified())
            .map(|t| now.duration_since(t).map(|age| age >= min_age).unwrap_or(false))
            .unwrap_or(false);
        if !old_enough {
            report.kept_recent += 1;
            continue;
        }
        let removed = if file_type.is_dir() { fs::remove_dir_all(&path) } else { fs::remove_file(&path) };
        if removed.is_ok() {
            report.deleted += 1;
        } else {
            report.failed += 1; // in use or protected
        }
    }
    Ok(report)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs::File;

    fn scratch(name: &str) -> PathBuf {
        let p = std::env::temp_dir().join(format!("lo-safety-{}-{}", name, std::process::id()));
        let _ = fs::remove_dir_all(&p);
        fs::create_dir_all(&p).unwrap();
        p
    }

    fn age(path: &Path, hours: u64) {
        let t = SystemTime::now() - Duration::from_secs(hours * 3600);
        File::options().write(true).open(path).unwrap().set_modified(t).unwrap();
    }

    // A folder can only be opened this way on Windows with the "backup semantics" flag.
    fn age_dir(path: &Path, hours: u64) {
        let t = SystemTime::now() - Duration::from_secs(hours * 3600);
        #[cfg(unix)]
        File::open(path).unwrap().set_modified(t).unwrap();
        #[cfg(windows)]
        {
            use std::os::windows::fs::OpenOptionsExt;
            File::options().write(true).custom_flags(0x0200_0000).open(path).unwrap().set_modified(t).unwrap();
        }
    }

    #[test]
    fn system32_path_is_absolute_and_tidy() {
        assert_eq!(system32_exe("C:\\Windows", "powercfg.exe"), "C:\\Windows\\System32\\powercfg.exe");
        assert_eq!(system32_exe("C:\\Windows\\", "taskkill.exe"), "C:\\Windows\\System32\\taskkill.exe");
        assert_eq!(
            powershell_exe("D:\\WINDOWS"),
            "D:\\WINDOWS\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
        );
    }

    #[test]
    fn counts_map_to_status() {
        assert_eq!(TweakResult::from_counts(5, 5, "x").status, Status::Ok);
        assert_eq!(TweakResult::from_counts(2, 5, "x").status, Status::Partial);
        assert_eq!(TweakResult::from_counts(0, 5, "x").status, Status::Failed);
        assert_eq!(TweakResult::from_counts(0, 0, "x").status, Status::Failed);
    }

    #[test]
    fn admin_gate_matches_the_tweaks_that_need_it() {
        assert!(requires_admin(1, false) && requires_admin(1, true));
        assert!(requires_admin(5, false) && requires_admin(5, true));
        assert!(requires_admin(4, false) && !requires_admin(4, true));
        for id in [2, 3, 6, 7, 8] {
            assert!(!requires_admin(id, false) && !requires_admin(id, true));
        }
    }

    #[test]
    fn status_serialises_lowercase() {
        let j = serde_json::to_string(&TweakResult::skipped("no admin")).unwrap();
        assert_eq!(j, r#"{"status":"skipped","message":"no admin"}"#);
    }

    #[test]
    fn dir_allowlist_accepts_only_exact_match() {
        let a = scratch("allow-a");
        let b = scratch("allow-b");
        assert!(dir_is_allowed(&a, &[a.clone()]));
        assert!(!dir_is_allowed(&b, &[a.clone()]));
        assert!(!dir_is_allowed(&a.join("missing"), &[a.clone()]));
        // a child of an allowed folder is not allowed
        fs::create_dir_all(a.join("child")).unwrap();
        assert!(!dir_is_allowed(&a.join("child"), &[a.clone()]));
        // the filesystem root is never allowed unless listed
        assert!(!dir_is_allowed(Path::new("/"), &[a]));
    }

    #[test]
    fn purge_keeps_new_removes_old_and_ignores_links() {
        let dir = scratch("purge");
        let old_file = dir.join("old.tmp");
        let new_file = dir.join("new.tmp");
        let old_dir = dir.join("old_dir");
        File::create(&old_file).unwrap();
        File::create(&new_file).unwrap();
        fs::create_dir_all(old_dir.join("inner")).unwrap();
        File::create(old_dir.join("inner").join("f")).unwrap();
        age(&old_file, 48);
        // directories: age the directory entry itself
        age_dir(&old_dir, 48);

        // a link to a folder outside the purge area must survive, and its target must be untouched
        let outside = scratch("purge-outside");
        File::create(outside.join("precious.txt")).unwrap();
        #[cfg(unix)]
        std::os::unix::fs::symlink(&outside, dir.join("link")).unwrap();

        let report = purge_old_entries(&dir, Duration::from_secs(24 * 3600), SystemTime::now()).unwrap();
        assert!(!old_file.exists());
        assert!(!old_dir.exists());
        assert!(new_file.exists());
        assert!(outside.join("precious.txt").exists());
        assert_eq!(report.deleted, 2);
        assert_eq!(report.kept_recent, 1);
        assert_eq!(report.failed, 0);
        #[cfg(unix)]
        {
            assert_eq!(report.skipped_links, 1);
            assert!(dir.join("link").exists());
        }
    }

    #[test]
    fn purge_missing_dir_is_an_error_not_a_success() {
        assert!(purge_old_entries(Path::new("/definitely/not/here"), Duration::ZERO, SystemTime::now()).is_err());
    }


    #[test]
    fn startup_inputs_are_validated() {
        assert_eq!(StartupHive::parse("HKCU"), Some(StartupHive::CurrentUser));
        assert_eq!(StartupHive::parse("HKLM"), Some(StartupHive::LocalMachine));
        for bad in ["", "hkcu", "HKCU ", "HKEY_USERS", "HKLM\\x"] {
            assert_eq!(StartupHive::parse(bad), None, "{bad:?}");
        }
        assert!(valid_startup_name("OneDrive"));
        assert!(!valid_startup_name(""));
        assert!(!valid_startup_name("bad\u{0}name"));
        assert!(!valid_startup_name(&"a".repeat(261)));
    }

    #[test]
    fn hardware_text_never_invents_values() {
        assert_eq!(format_specs(16, 477), "\u{2022} 16 GB RAM \u{2022} 477 GB disk");
        assert_eq!(format_specs(0, 0), "\u{2022} RAM unknown \u{2022} disk unknown");
        assert_eq!(text_or_unknown("  ", "Unknown model"), "Unknown model");
        assert_eq!(text_or_unknown("Default string", "Unknown model"), "Unknown model");
        assert_eq!(text_or_unknown("To Be Filled By O.E.M.", "Unknown model"), "Unknown model");
        assert_eq!(text_or_unknown(" 82HS ", "Unknown model"), "82HS");
    }

    #[test]
    fn risky_tweaks_need_their_own_confirmation() {
        assert!(confirmation_ok(1, None));
        assert!(!confirmation_ok(4, None));
        assert!(!confirmation_ok(4, Some("turn-off-hibernation")), "another operation's word must not work");
        assert!(confirmation_ok(4, Some("remove-widgets")));
        assert!(!confirmation_ok(5, Some("")));
        assert!(confirmation_ok(5, Some("turn-off-hibernation")));
    }

    #[test]
    fn lock_refuses_second_holder_and_releases() {
        let lock = Lock::new();
        let first = lock.try_acquire();
        assert!(first.is_some());
        assert!(lock.try_acquire().is_none(), "second change must be refused");
        drop(first);
        assert!(lock.try_acquire().is_some(), "released after the first finished");
    }

    #[test]
    fn lock_releases_after_panic() {
        static L: Lock = Lock::new();
        let r = std::panic::catch_unwind(|| {
            let _g = L.try_acquire().unwrap();
            panic!("boom");
        });
        assert!(r.is_err());
        assert!(L.try_acquire().is_some());
    }

    #[test]
    fn lock_allows_only_one_of_many_threads() {
        static L: Lock = Lock::new();
        let wins = std::sync::atomic::AtomicUsize::new(0);
        let start = std::sync::Barrier::new(8);
        std::thread::scope(|s| {
            for _ in 0..8 {
                s.spawn(|| {
                    start.wait();
                    if let Some(_g) = L.try_acquire() {
                        wins.fetch_add(1, Ordering::SeqCst);
                        std::thread::sleep(Duration::from_millis(200));
                    }
                });
            }
        });
        assert_eq!(wins.load(Ordering::SeqCst), 1);
    }

    #[test]
    fn harden_overrides_poisoned_environment() {
        let mut c = Command::new("x");
        harden(&mut c, "C:\\Windows\\");
        assert_eq!(c.get_current_dir().unwrap(), Path::new("C:\\Windows\\System32"));
        let env: std::collections::HashMap<_, _> = c.get_envs().collect();
        assert_eq!(env[std::ffi::OsStr::new("SystemRoot")].unwrap(), "C:\\Windows");
        assert_eq!(env[std::ffi::OsStr::new("PATH")].unwrap(), "C:\\Windows\\System32;C:\\Windows");
        assert!(env[std::ffi::OsStr::new("PSModulePath")].is_none(), "PSModulePath must be removed");
    }

    #[cfg(unix)]
    #[test]
    fn timeout_reports_each_outcome() {
        let mut ok = Command::new("sh");
        ok.args(["-c", "exit 0"]);
        assert_eq!(run_with_timeout(ok, Duration::from_secs(5)), Outcome::Success);

        let mut bad = Command::new("sh");
        bad.args(["-c", "exit 3"]);
        assert_eq!(run_with_timeout(bad, Duration::from_secs(5)), Outcome::ExitCode(3));

        let mut slow = Command::new("sleep");
        slow.arg("30");
        let started = Instant::now();
        assert_eq!(run_with_timeout(slow, Duration::from_millis(300)), Outcome::TimedOut);
        assert!(started.elapsed() < Duration::from_secs(5), "must stop the program promptly");

        let missing = Command::new("/no/such/program");
        assert!(matches!(run_with_timeout(missing, Duration::from_secs(1)), Outcome::CouldNotStart(_)));
    }

    #[cfg(windows)]
    #[test]
    fn timeout_reports_each_outcome_on_windows() {
        let root = std::env::var("SystemRoot").unwrap_or_else(|_| "C:\\Windows".into());
        let ping = system32_exe(&root, "ping.exe");

        let mut ok = Command::new(&ping);
        ok.args(["-n", "1", "127.0.0.1"]);
        assert_eq!(run_with_timeout(ok, Duration::from_secs(20)), Outcome::Success);

        let mut slow = Command::new(&ping);
        slow.args(["-n", "30", "127.0.0.1"]);
        let started = Instant::now();
        assert_eq!(run_with_timeout(slow, Duration::from_millis(500)), Outcome::TimedOut);
        assert!(started.elapsed() < Duration::from_secs(5));

        let missing = Command::new("C:\\definitely\\not\\here.exe");
        assert!(matches!(run_with_timeout(missing, Duration::from_secs(1)), Outcome::CouldNotStart(_)));
    }
}

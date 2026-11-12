//! The "before" state, saved to disk so Restore puts back what was there instead of a guessed default.
//! Uses only `std`, `serde` and `serde_json`, so its tests run anywhere.
//! The file holds values only. Which registry keys may be written is fixed in the code (see `optimizer.rs`),
//! so a changed file cannot make the app write to any other key.

use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

/// One registry value exactly as it was: its type number and raw bytes.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SavedValue {
    pub vtype: u32,
    pub bytes: Vec<u8>,
}

/// Everything saved for one tweak before its first change.
#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct TweakState {
    /// Registry target -> value before the change. `None` means the value did not exist.
    pub values: BTreeMap<String, Option<SavedValue>>,
    /// The active power plan GUID (tweak 1).
    pub power_plan: Option<String>,
    /// Whether hibernation was on (tweak 5).
    pub hibernate: Option<bool>,
}

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct Journal {
    pub version: u32,
    pub tweaks: BTreeMap<u32, TweakState>,
}

impl Journal {
    /// A missing file is an empty journal. A file that exists but cannot be read is an error:
    /// the caller must refuse to change anything rather than overwrite what it cannot understand.
    pub fn load(path: &Path) -> Result<Journal, String> {
        match fs::read(path) {
            Ok(bytes) => serde_json::from_slice(&bytes).map_err(|e| e.to_string()),
            Err(e) if e.kind() == io::ErrorKind::NotFound => Ok(Journal { version: 1, ..Default::default() }),
            Err(e) => Err(e.to_string()),
        }
    }

    /// Writes next to the file first, then renames, so a crash never leaves half a file.
    pub fn save(&self, path: &Path) -> io::Result<()> {
        if let Some(dir) = path.parent() {
            fs::create_dir_all(dir)?;
        }
        let tmp = path.with_extension("tmp");
        fs::write(&tmp, serde_json::to_vec_pretty(self).map_err(io::Error::other)?)?;
        fs::rename(&tmp, path)
    }

    /// Keeps the first save for a tweak. A second apply must not replace the original with an already-changed state.
    /// Returns true when the state was stored.
    pub fn keep_first(&mut self, id: u32, state: TweakState) -> bool {
        if self.tweaks.contains_key(&id) {
            return false;
        }
        self.tweaks.insert(id, state);
        true
    }
}

/// `%LOCALAPPDATA%\LaptopOptimizer\previous.json`
pub fn default_path() -> Option<PathBuf> {
    std::env::var_os("LOCALAPPDATA").map(|d| PathBuf::from(d).join("LaptopOptimizer").join("previous.json"))
}

/// A Windows power plan GUID: 36 characters, hex digits and dashes in the usual places.
pub fn valid_guid(s: &str) -> bool {
    s.len() == 36
        && s.char_indices().all(|(i, c)| match i {
            8 | 13 | 18 | 23 => c == '-',
            _ => c.is_ascii_hexdigit(),
        })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(name: &str) -> PathBuf {
        let p = std::env::temp_dir().join(format!("lo-journal-{}-{}", name, std::process::id()));
        let _ = fs::remove_dir_all(&p);
        p
    }

    fn state(value: Option<u32>) -> TweakState {
        let mut s = TweakState::default();
        s.values.insert(
            "HKCU\\k\\n".into(),
            value.map(|v| SavedValue { vtype: 4, bytes: v.to_le_bytes().to_vec() }),
        );
        s
    }

    #[test]
    fn saved_state_survives_a_round_trip_including_missing_values() {
        let path = scratch("rt").join("previous.json");
        let mut j = Journal { version: 1, ..Default::default() };
        j.keep_first(3, state(Some(400)));
        j.keep_first(7, state(None)); // value that did not exist
        j.save(&path).unwrap();
        let back = Journal::load(&path).unwrap();
        assert_eq!(back.tweaks, j.tweaks);
        assert_eq!(back.tweaks[&7].values["HKCU\\k\\n"], None);
        assert!(!path.with_extension("tmp").exists(), "no temporary file left behind");
        let _ = fs::remove_dir_all(path.parent().unwrap());
    }

    #[test]
    fn first_save_wins() {
        let mut j = Journal::default();
        assert!(j.keep_first(3, state(Some(400))));
        assert!(!j.keep_first(3, state(Some(20))), "the already-changed value must not replace the original");
        assert_eq!(j.tweaks[&3], state(Some(400)));
    }

    #[test]
    fn missing_file_is_empty_but_a_broken_file_is_an_error() {
        let dir = scratch("load");
        assert!(Journal::load(&dir.join("nope.json")).unwrap().tweaks.is_empty());
        fs::create_dir_all(&dir).unwrap();
        let bad = dir.join("bad.json");
        fs::write(&bad, b"{ not json").unwrap();
        assert!(Journal::load(&bad).is_err(), "must refuse to overwrite what it cannot read");
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn power_plan_guid_must_look_like_a_guid() {
        assert!(valid_guid("8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c"));
        assert!(valid_guid("381B4222-F694-41F0-9685-FF5BB260DF2E"));
        for bad in ["", "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635", "8c5e7fda_e8bf_4a96_9a85_a6e23a8c635c", "8c5e7fda-e8bf-4a96-9a85-a6e23a8c63zz", "; calc"] {
            assert!(!valid_guid(bad), "{bad:?}");
        }
    }
}

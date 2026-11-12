"""Source checks that keep unsafe patterns out of the Rust code. Run: python3 tests/source_guards.py"""
import pathlib, re, sys
SRC = pathlib.Path(__file__).resolve().parent.parent / "src-tauri" / "src"
code = (SRC / "optimizer.rs").read_text() + (SRC / "lib.rs").read_text()
fails = []
def check(name, ok):
    print(("PASS " if ok else "FAIL ") + name)
    if not ok: fails.append(name)

check("no winget (it lives in a user-writable folder)", "winget" not in code.lower())
check("no cmd.exe / shell strings", not re.search(r'"cmd(\.exe)?"|"/c"|"/C"', code))
check("no direct Command::new outside the absolute-path helper", len(re.findall(r"Command::new\(", code)) == 1)
check("Windows folder comes from the Windows API, not an environment variable", 'env::var("SystemRoot")' not in code and 'env::var("windir")' not in code)
check("no silent removal of the download mark", "Zone.Identifier" not in code and "unblock" not in code.lower())
check("startup toggle goes through the validated function", "set_startup_app_enabled(hive" in code or "set_startup_app_enabled(hive," in code)
sys.exit(1 if fails else 0)

# Production readiness plan

Last updated: 2026-11-09 · Branch: `safer-tweaks` (local commits only, nothing pushed) · Sources: Second Audit (F-01..F-16, N-01..N-07, U01..U23) and Third Audit (U01..U38).

**Overall status: NOT READY.** No Windows runtime verification exists yet (W1 = BLOCKED: no Windows machine in this environment). Linux tests and static checks are the only evidence so far.

Statuses: Not Started · In Progress · Completed · Blocked · Needs Review · Deferred · Rejected.
"Needs Review" = code written and the Linux-side checks passed, but the Windows behavior is not proven. Nothing is "Completed" without Windows evidence where Windows is involved.

## Evidence so far (all Linux)
- E1: `safety.rs` unit tests: 15 pass (scratch crate, `cargo test`).
- E2: `optimizer.rs` type-checks against a stand-in registry library (not a real build).
- E3: `tests/ui_regression.py`: 24 checks pass (page, injection, modes, toggle, config consistency).
- E4: page served with the exact CSP in Chromium: no violations, handlers work (WebView2 not tested).
- E5: `tests/source_guards.py`: 6 source checks pass (no winget, no cmd, one Command::new, no SystemRoot env, no download-mark code).
- W1: real Windows run, packaged EXE, UAC, two-account test: **BLOCKED**.

## Tasks
| ID | Pri | Task | Depends | Files | Verification | Status |
|---|---|---|---|---|---|---|
| U01 | Critical | Prove backend link; `withGlobalTauri` on | D1 | tauri.conf.json | Windows VM: Apply changes a registry value, log says DONE | Blocked (config changed; proof needs Windows) |
| U02 | High | Red "BACKEND NOT CONNECTED" bar, amber demo bar, block actions | U01 | app.js, index.html | tests/ui_regression.py | Needs Review |
| U03 | High | Safe Widgets removal (no winget from user folder) | U25 | optimizer.rs | tests/source_guards.py + type-check; VM for the result | Needs Review |
| U04 | High | One backend lock for all changing commands | none | safety.rs, lib.rs | 3 lock tests (refuse, release, panic, 8 threads) | Needs Review |
| U05 | High | Safe text, no inline handlers, strict CSP | U01 | index.html, app.js, tauri.conf.json | injection test E5 now clean; CSP check | Needs Review |
| U06 | Medium | Validate startup toggle, read back, UI flips only on success | U04 | lib.rs, optimizer.rs, app.js | validator test, UI tests | Needs Review |
| U08 | Medium | Pre-checks (Windows build, laptop, sleep type) for risky tweaks | U25 | optimizer.rs | mocks + VM | Not Started |
| U09 | Medium | Stop whole process trees (job object) | VM | safety.rs | helper program test | Not Started |
| U10 | Medium | More result states (timed out, unknown, conflict, no change) | U09 | safety.rs, app.js | mocks | Not Started |
| U11 | Medium | Correct tweak 1 text; confirm power-throttling key | VM | app.js, optimizer.rs | VM | Not Started |
| U12 | Medium | Restore exact previous state (incl. missing values) | U27 | optimizer.rs, journal.rs | mock registry | Not Started |
| U15 | Medium | Manifest decision | D3 | build.rs, app.manifest | VM | Not Started |
| U16 | High | Stop committing the EXE; build only in CI | U33, D7 | repo | dry run | Not Started |
| U17 | Medium | Remove build paths, release profile | none | Cargo.toml, release.yml | no `C:\Users\` in EXE (needs a build) | Needs Review |
| U18 | Low | Fix release.yml claims, pin actions | none | release.yml | claims removed (done); pinning Blocked (no GitHub access) | Needs Review |
| U19 | Medium | Code signing | D6 | CI | signature check | Deferred (needs certificate) |
| U20 | High | Mock-based tests (fake runner and registry, failure injection) | U04 | tests | cargo test | In Progress (safety.rs tests only, 15) |
| U21 | Medium | Windows CI runner | U20 | ci.yml | CI run after push | Needs Review (not run) |
| U22 | High | Windows VM checklist on packaged EXE (10 and 11, two accounts) | D1 | docs | manual | Blocked |
| U24 | High | Refuse user-scoped tweaks if signed-in user differs from elevated user | D1 | optimizer.rs, lib.rs | unit (fake SIDs) + VM, two accounts | Blocked (needs Windows API testing) |
| U25 | High | Program resolution: Windows folder from API, System32 cwd, PATH/SystemRoot overridden | none | safety.rs, optimizer.rs | harden test; type-check | Needs Review (clean environment still open) |
| U27 | High | Durable operation journal | U04, U10 | journal.rs | kill-mid-run tests | Not Started |
| U28 | Medium | Detect outside changes at restore | U27 | optimizer.rs | mock | Not Started |
| U29 | Medium | Remove invented hardware values and fixed page text | none | optimizer.rs, safety.rs, index.html, app.js | `hardware_text_never_invents_values` test; 8 page checks for author-machine text; failed reads now show "unknown" or "--" | Needs Review (real Windows reads untested) |
| U30 | Medium | Stop silently removing the download mark | D5 | optimizer.rs, lib.rs, .bat | Windows: mark survives | Needs Review (code and .bat changed; D5 assumed "remove") |
| U31 | Medium | Operation-specific confirmation enforced by backend | U04 | lib.rs, safety.rs, app.js | Rust test + 2 page tests | Needs Review |
| U32 | Medium | Verify every change after it runs | U20 | optimizer.rs | mock | Not Started |
| U33 | High | Source-to-EXE proof (CI build from tag, hash, version shown) | U01..U05 | release.yml, scripts | dry run | Not Started |
| U34 | Medium | Compatibility gate and honest support text | U22 | optimizer.rs, docs | VM matrix | Not Started |
| U35 | Low | Git hygiene (local main diverged) | none | git | `git rev-parse` both e0316a1 | Completed |
| U36 | Medium | Packaged EXE smoke test | U22 | VM | manual | Blocked |
| U37 | Medium | Split-privilege design | D2 | design | review | Deferred |
| U38 | Medium | Allow-list backend commands | U01 | build.rs, capabilities/default.json | tests/ui_regression.py (consistency only); real build untested | Needs Review |


## Task details (why, steps, expected result, verification, evidence)

### U01
- **Why:** Without the global API the page may run only its demo path, so no change is ever applied.
- **Steps:** Set `withGlobalTauri` (done). Build. Run on Windows. Check a registry value changes (guide step 4).
- **Expected result:** Log says applied and checked; value changes.
- **Evidence:** Windows only. Evidence: none yet.

### U02
- **Why:** Silent fallback to demo inside the real app would show fake success.
- **Steps:** Detect real / broken / demo; show bar; block actions in broken mode.
- **Expected result:** Red bar and blocked actions when the backend link is missing.
- **Evidence:** tests/ui_regression.py (broken-mode and demo-mode checks pass).

### U03
- **Why:** A planted winget.exe in a user-writable folder would run as administrator.
- **Steps:** Delete the winget branch. Remove the pack only via System32 PowerShell. Add source guard test.
- **Expected result:** No winget anywhere in the code.
- **Evidence:** tests/source_guards.py + type-check pass. Windows removal result untested.

### U04
- **Why:** Two clicks could run two changes at once and corrupt state.
- **Steps:** One lock around every changing command; busy reply; panic-safe release.
- **Expected result:** Second call is refused, lock frees after the first.
- **Evidence:** 4 lock tests pass (including 8 threads, panic).

### U05
- **Why:** Registry text was inserted as page markup (proven script injection).
- **Steps:** textContent for registry text; no inline handlers; strict CSP; script in app.js.
- **Expected result:** Malicious names show as plain text.
- **Evidence:** Injection test passes; page loads under the CSP in Chromium. WebView2 untested.

### U06
- **Why:** The startup toggle accepted any location/name and never checked the result.
- **Steps:** Allow HKCU/HKLM only; entry must exist; admin for HKLM; read back; page flips only on ok.
- **Expected result:** Bad input refused; page matches registry.
- **Evidence:** Validator test + 3 page tests pass. Registry behavior untested.

### U08
- **Why:** Hibernation off on laptops/Modern Standby and Widgets on Windows 10 are risky.
- **Steps:** Read Windows build, battery, sleep type; skip with a reason.
- **Expected result:** Unsuitable PCs are skipped, not forced.
- **Evidence:** Not started. Needs Windows to learn real values.

### U09
- **Why:** A timed-out program's children keep running.
- **Steps:** Windows job object with kill-on-close around each run.
- **Expected result:** No child outlives a timeout.
- **Evidence:** Not started. Needs Windows API code and Windows test.

### U10
- **Why:** Only ok/partial/skipped/failed exist.
- **Steps:** Add timed out, unknown, conflict, no change; show them.
- **Expected result:** Each appears under the right condition.
- **Evidence:** Not started.

### U11
- **Why:** The "100% floor" text is unproven and the power-throttling key is unconfirmed.
- **Steps:** Check on Windows what the key does; correct the text.
- **Expected result:** Text matches measured behavior.
- **Evidence:** Blocked on Windows.

### U12
- **Why:** Restore uses fixed defaults, not the user's real previous values.
- **Steps:** Save existence, type and data before change; restore exactly.
- **Expected result:** Apply then restore leaves the registry identical.
- **Evidence:** Not started. Needs the journal (U27).

### U15
- **Why:** The manifest decides when Windows asks for administrator rights.
- **Steps:** Decide D3, then embed or not.
- **Expected result:** Matches the decision.
- **Evidence:** Waiting for D3.

### U16
- **Why:** A committed EXE can be stale and unverified.
- **Steps:** Remove it from git; build only in CI; website links to releases.
- **Expected result:** Site never serves an unverified file.
- **Evidence:** Waiting for U33 and D7.

### U17
- **Why:** EXE held build paths and symbols; version numbers disagreed.
- **Steps:** Align versions (done), release profile (done), path remap in CI (done).
- **Expected result:** No C:\\Users text in the new EXE.
- **Evidence:** Config parses. Real EXE check blocked until a build.

### U18
- **Why:** Release text made false claims; actions were not pinned.
- **Steps:** Remove claims (done), draft releases (done), test step (done), pin SHAs (blocked: no GitHub access here).
- **Expected result:** Honest release text; pinned actions.
- **Evidence:** Workflow parses. Pinning blocked.

### U19
- **Why:** Unsigned files trigger SmartScreen warnings.
- **Steps:** Needs a certificate (D6).
- **Expected result:** Signed EXE.
- **Evidence:** Deferred.

### U20
- **Why:** Most logic is only type-checked.
- **Steps:** Fake process runner and fake registry; failure injection.
- **Expected result:** cargo test passes without touching a real system.
- **Evidence:** In progress: safety.rs tests only (15).

### U21
- **Why:** Windows evidence should not depend on this sandbox.
- **Steps:** ci.yml: cargo test on Windows, page tests on Linux.
- **Expected result:** Both jobs green after the first push.
- **Evidence:** Workflow parses. Not run yet.

### U22
- **Why:** Packaged EXE never tested.
- **Steps:** Use docs/WINDOWS_TEST_GUIDE.md on Windows 10 and 11, two accounts.
- **Expected result:** Checklist passes.
- **Evidence:** Blocked until you run it.

### U24
- **Why:** If a different admin account approves the prompt, per-user changes hit the wrong profile.
- **Steps:** Compare process user with signed-in user; refuse user-scoped tweaks if different.
- **Expected result:** Refusal with a clear reason.
- **Evidence:** Blocked: needs Windows API and a two-account test.

### U25
- **Why:** Programs could be redirected by environment variables.
- **Steps:** Windows folder from API (done); System32 cwd and SystemRoot/windir/PATH override (done); full clean environment (open).
- **Expected result:** Changed variables change nothing that runs.
- **Evidence:** Unit test + type-check pass.

### U27
- **Why:** A crash mid-change leaves unknown state.
- **Steps:** Write-ahead journal with checksum; fail closed.
- **Expected result:** Every crash scenario handled.
- **Evidence:** Not started.

### U28
- **Why:** Restore could overwrite the user's newer change.
- **Steps:** Compare current value with the value we set; conflict if different.
- **Expected result:** Never overwritten silently.
- **Evidence:** Not started (needs U27).

### U29
- **Why:** Page showed values from the author's PC.
- **Steps:** Unknown values show as unknown/--; fixed text removed; tests added.
- **Expected result:** Nothing displayed that was not read.
- **Evidence:** 1 Rust test + 8 page checks pass. Real Windows reads untested.

### U30
- **Why:** The app removed its own download mark without asking.
- **Steps:** Delete the code and the .bat line.
- **Expected result:** Mark survives.
- **Evidence:** Source guard passes. Windows untested. D5 assumed.

### U31
- **Why:** A direct backend call could remove Widgets or hibernation with no question.
- **Steps:** Per-operation word required by the backend; specific dialog in the page.
- **Expected result:** Declined or missing word changes nothing.
- **Evidence:** Rust test + 2 page tests pass.

### U32
- **Why:** Some operations report success without reading back.
- **Steps:** Read back every change including temp purge.
- **Expected result:** Success only after the new state is read.
- **Evidence:** Not started.

### U33
- **Why:** No proof the website file comes from this source.
- **Steps:** Build in CI from a tag; record commit and SHA-256; show version in app.
- **Expected result:** Site hash equals release asset.
- **Evidence:** Not started.

### U34
- **Why:** Support claims are not tied to tested versions.
- **Steps:** Detect version/edition/arch; text from VM results.
- **Expected result:** Support text equals test matrix.
- **Evidence:** Not started.

### U35
- **Why:** Local main had an unpushed commit.
- **Steps:** Move local main to origin/main (done, no push).
- **Expected result:** main equals origin/main.
- **Evidence:** Verified with git rev-parse.

### U36
- **Why:** Each build needs a quick real-app check.
- **Steps:** Smoke test steps in the guide.
- **Expected result:** Real mode, one tweak, restore.
- **Evidence:** Blocked until you run it.

### U37
- **Why:** Full fix for the wrong-user case needs a helper process.
- **Steps:** Design only after D2.
- **Expected result:** Design reviewed.
- **Evidence:** Deferred.

### U38
- **Why:** Every backend command was open to the page.
- **Steps:** Allow-list in build.rs plus permissions.
- **Expected result:** Unlisted calls refused.
- **Evidence:** Consistency test passes. Real build untested.

## Decisions still needed (owner: Vaibhav)
D1 Windows test machine · D2 wrong-user approach (U24 chosen for now) · D3 admin at start vs on demand · D4 journal folder · D5 download mark (removal assumed) · D6 signing · D7 website notice for the old EXE · D8 approvals.

## Known unverified risks in the current changes
- U38: a wrong permission name fails the Windows build; the first CI build will show it.
- U05: Tauri edits the CSP when it embeds the page; WebView2 behavior is untested.
- U25: only SystemRoot, windir and PATH are overridden; the rest of the environment is inherited on purpose.
- Website still serves the old unsigned EXE from `raw/main` (U16/U33/D7).

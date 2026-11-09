# Production readiness plan

Last updated: 2026-11-09 · Branch: `safer-tweaks` (local commits only, nothing pushed) · Sources: Second Audit (F-01..F-16, N-01..N-07, U01..U23) and Third Audit (U01..U38).

**Overall status: NOT READY.** No Windows runtime verification exists yet (W1 = BLOCKED: no Windows machine in this environment). Linux tests and static checks are the only evidence so far.

Statuses: Not Started · In Progress · Completed · Blocked · Needs Review · Deferred · Rejected.
"Needs Review" = code written and the Linux-side checks passed, but the Windows behavior is not proven. Nothing is "Completed" without Windows evidence where Windows is involved.

## Evidence so far (all Linux)
- E1: `safety.rs` unit tests: 13 pass (scratch crate, `cargo test`).
- E2: `optimizer.rs` type-checks against a stand-in registry library (not a real build).
- E3: `tests/ui_regression.py`: 14 checks pass (page, injection, modes, toggle, config consistency).
- E4: page served with the exact CSP in Chromium: no violations, handlers work (WebView2 not tested).
- W1: real Windows run, packaged EXE, UAC, two-account test: **BLOCKED**.

## Tasks
| ID | Pri | Task | Depends | Files | Verification | Status |
|---|---|---|---|---|---|---|
| U01 | Critical | Prove backend link; `withGlobalTauri` on | D1 | tauri.conf.json | Windows VM: Apply changes a registry value, log says DONE | Blocked (config changed; proof needs Windows) |
| U02 | High | Red "BACKEND NOT CONNECTED" bar, amber demo bar, block actions | U01 | app.js, index.html | tests/ui_regression.py | Needs Review |
| U03 | High | Safe Widgets removal (no winget from user folder) | U25 | optimizer.rs | unit tests + VM | Not Started |
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
| U17 | Medium | Remove build paths, release profile | none | Cargo.toml | no `C:\Users\` in EXE | Not Started |
| U18 | Low | Fix release.yml claims, pin actions | none | release.yml | dry run | Not Started |
| U19 | Medium | Code signing | D6 | CI | signature check | Deferred (needs certificate) |
| U20 | High | Mock-based tests (fake runner and registry, failure injection) | U04 | tests | cargo test | In Progress (safety.rs tests only) |
| U21 | Medium | Windows CI runner | U20 | workflow | CI run | Not Started |
| U22 | High | Windows VM checklist on packaged EXE (10 and 11, two accounts) | D1 | docs | manual | Blocked |
| U24 | High | Refuse user-scoped tweaks if signed-in user differs from elevated user | D1 | optimizer.rs, lib.rs | unit (fake SIDs) + VM, two accounts | Blocked (needs Windows API testing) |
| U25 | High | Program resolution: Windows folder from API, System32 cwd, PATH/SystemRoot overridden | none | safety.rs, optimizer.rs | harden test; type-check | Needs Review (clean environment still open) |
| U27 | High | Durable operation journal | U04, U10 | journal.rs | kill-mid-run tests | Not Started |
| U28 | Medium | Detect outside changes at restore | U27 | optimizer.rs | mock | Not Started |
| U29 | Medium | Remove invented hardware values and fixed page text | none | optimizer.rs, index.html | forced query failures | Not Started |
| U30 | Medium | Stop silently removing the download mark | D5 | optimizer.rs, lib.rs, .bat | Windows: mark survives | Needs Review (code and .bat changed; D5 assumed "remove") |
| U31 | Medium | Operation-specific confirmation enforced by backend | U04 | lib.rs, app.js | direct call refused | Not Started |
| U32 | Medium | Verify every change after it runs | U20 | optimizer.rs | mock | Not Started |
| U33 | High | Source-to-EXE proof (CI build from tag, hash, version shown) | U01..U05 | release.yml, scripts | dry run | Not Started |
| U34 | Medium | Compatibility gate and honest support text | U22 | optimizer.rs, docs | VM matrix | Not Started |
| U35 | Low | Git hygiene (local main diverged) | none | git | main equals origin/main | Not Started |
| U36 | Medium | Packaged EXE smoke test | U22 | VM | manual | Blocked |
| U37 | Medium | Split-privilege design | D2 | design | review | Deferred |
| U38 | Medium | Allow-list backend commands | U01 | build.rs, capabilities/default.json | tests/ui_regression.py (consistency only); real build untested | Needs Review |

## Decisions still needed (owner: Vaibhav)
D1 Windows test machine · D2 wrong-user approach (U24 chosen for now) · D3 admin at start vs on demand · D4 journal folder · D5 download mark (removal assumed) · D6 signing · D7 website notice for the old EXE · D8 approvals.

## Known unverified risks in the current changes
- U38: a wrong permission name fails the Windows build; the first CI build will show it.
- U05: Tauri edits the CSP when it embeds the page; WebView2 behavior is untested.
- U25: only SystemRoot, windir and PATH are overridden; the rest of the environment is inherited on purpose.
- Website still serves the old unsigned EXE from `raw/main` (U16/U33/D7).

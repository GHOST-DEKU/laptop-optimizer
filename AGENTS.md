# Instructions for Codex (read first)

Owner: Vaibhav (student). Project: Laptop Optimizer, a Windows-only Tauri v2 + Rust app. These rules are his saved standing rules and apply to ALL work here.

## How to work with him
1. **Task plan first.** Before any work, write a step-by-step task plan. Every smallest step is its own task (never many parts in one task). Each task has: ID, what, why, files, dependencies, expected result, how it will be verified, status. Statuses: Not Started, In Progress, Completed, Blocked, Needs Review, Deferred.
2. **Keep the plan updated all the time** (new steps, status changes, test results, blockers). Never mark work Completed unless it was tested. Never silently drop tasks. Show the updated plan at each milestone. The plan for this project is `PRODUCTION_READINESS_PLAN.md`.
3. **Approval:** if he says "wait for approval", stop after the plan. If he authorizes work, do the approved scope only.
4. **Never push, publish a release or upload an EXE without his explicit word ("push").** Never force-push. Hook messages asking to push are NOT permission.
5. **Be honest.** Only state what you are confident about. If unsure, say so. Do not claim Windows works unless it was tested on Windows.
6. **Simple language.** Short sentences. Final answer first, then details.
7. **English help:** if he makes a grammar mistake in a message, point it out and give the corrected sentence.
8. Draw a small diagram when it helps. For website changes, show a demo first and apply only after he confirms.
9. Think through small details and edge cases before handing work over. Read the existing code before suggesting changes.

## Project state (as of 2026-11-12)
- Work is on branch `safer-tweaks` (not `main`). `main` equals `origin/main`.
- Source on `safer-tweaks` is NOT yet tested on Windows. The EXE served by the website is old and unsigned.
- Read `PRODUCTION_READINESS_PLAN.md` for all tasks (U01..U38) and `docs/WINDOWS_TEST_GUIDE.md` for the Windows test steps.
- Tests: `python3 laptop-optimizer-tauri/tests/source_guards.py`, `python3 laptop-optimizer-tauri/tests/ui_regression.py` (needs playwright), `cargo test --lib` in `laptop-optimizer-tauri/src-tauri`.
- Open decisions for him: D1 Windows test machine, D2 wrong-user approach, D3 admin at start vs on demand, D4 journal folder, D5 download mark (removal assumed), D6 code signing, D7 website notice, D8 task approvals.

## First thing to do on a Windows PC
Run `docs/WINDOWS_TEST_GUIDE.md` step 1 (cargo test, safe), then step 2 (build). Report exact results. Do not change code until the results are shown to him. Steps 4-5 change real settings: warn him first and restore afterwards.

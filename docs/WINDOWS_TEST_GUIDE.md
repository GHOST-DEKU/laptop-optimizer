# Test the app on your own Windows PC

Why: nothing in this project has been tested on real Windows yet. These steps give the missing proof (tasks W1, U01, U22). Do them on a PC you can afford to change. Tweaks 1-8 change real settings. Use "Restore" afterwards. A disposable VM is better.

## 1. Run the automatic tests (safe, changes nothing)
Open PowerShell in the project folder:

    cd laptop-optimizer-tauri\src-tauri
    cargo test --lib

Expected: `test result: ok` with 15 tests (the Linux-only timeout test is replaced by a Windows one).
If any test fails, copy the whole output and send it to Claude.

## 2. Build the app (takes a few minutes the first time)
Needs: Rust (MSVC toolchain), Node.js, pnpm, Microsoft C++ Build Tools, WebView2 (included in Windows 11).

    cd ..\
    pnpm install
    pnpm tauri build

If the build stops with an error about a permission name (for example `allow-get-system-info`), copy the error. It is the one known risk of the command allow-list (task U38).
The installer files appear in `src-tauri\target\release\bundle\`.

## 3. Check it is really connected (task U01)
1. Start the app (right-click, Run as administrator).
2. At the top there must be NO red bar and NO amber bar.
   - Red "BACKEND NOT CONNECTED": the page cannot reach the program. Tell Claude.
   - Amber "DEMO MODE": you are looking at a browser preview, not the app.
3. The CPU, RAM and disk lines must show YOUR computer, not "DEMO".

## 4. Try one safe tweak (task U01 proof)
1. Press Win+R, type `regedit`, open `HKEY_CURRENT_USER\Control Panel\Desktop`, note the value of `MenuShowDelay`.
2. In the app, turn on only "Instant Desktop Context Menus", arm the hatch, press deploy.
3. The log must say it was applied AND checked. Refresh regedit: `MenuShowDelay` must have changed.
4. Press restore. The value must go back.

## 5. Try the extra checks
- Start the app WITHOUT administrator rights: the yellow banner must appear and admin tweaks must say "skipped".
- Click deploy twice quickly: the second one must say another change is still running (or the button is busy).
- Turn on Widgets or Hibernation: a specific question must appear first. Choose Cancel: nothing changes.
- Startup list: open it, switch one entry off, check Task Manager > Startup apps agrees. Switch it back.

## 6. What to send back
Test 1 output, whether step 2 built, what steps 3-5 showed (screenshots help). Claude will update PRODUCTION_READINESS_PLAN.md from your results.

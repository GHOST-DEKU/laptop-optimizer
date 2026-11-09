"""Browser tests for the page (no Windows needed). Run: python3 tests/ui_regression.py
Needs: pip install playwright && playwright install chromium"""
import asyncio, json, pathlib, re, sys
from playwright.async_api import async_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
URL = (ROOT / "src" / "index.html").as_uri()
FAILS = []

def check(name, ok):
    print(("PASS " if ok else "FAIL ") + name)
    if not ok:
        FAILS.append(name)

REAL = """
window.__calls=[];
window.__TAURI__={core:{invoke:async(c,a)=>{window.__calls.push([c,a]);
 if(c==='get_startup_apps')return [{name:'<img src=x onerror="window.__TAURI__.core.invoke(\\'apply_tweak\\',{id:5})">',command:'<b id=pwn>x</b>',location:'HKCU',is_enabled:true}];
 if(c==='toggle_startup_app')return window.__toggleResult||{status:'ok',message:'ok'};
 if(c==='get_elevation')return true;
 if(c==='apply_tweak')return {status:'ok',message:'applied'};
 return null;}}};
"""
BROKEN = "window.__calls=[];window.__TAURI_INTERNALS__={};"

async def page(b, init):
    pg = await b.new_page()
    if init:
        await pg.add_init_script(init)
    await pg.goto(URL)
    await pg.wait_for_timeout(400)
    return pg

async def main():
    # static checks
    html = (ROOT / "src" / "index.html").read_text()
    check("no inline onclick in index.html", "onclick=" not in html)
    conf = json.loads((ROOT / "src-tauri" / "tauri.conf.json").read_text())
    csp = conf["app"]["security"]["csp"]
    check("CSP is set and has no unsafe-eval or script unsafe-inline",
          bool(csp) and "unsafe-eval" not in csp and not re.search(r"script-src[^;]*unsafe-inline", csp))
    check("withGlobalTauri is on", conf["app"].get("withGlobalTauri") is True)
    caps = json.loads((ROOT / "src-tauri" / "capabilities" / "default.json").read_text())["permissions"]
    cmds = re.findall(r'"([a-z_]+)",', (ROOT / "src-tauri" / "build.rs").read_text().split("fn main")[0])
    check("every listed command is granted", all("allow-" + c.replace("_", "-") in caps for c in cmds) and len(cmds) == 8)
    lib = (ROOT / "src-tauri" / "src" / "lib.rs").read_text()
    reg = re.search(r"generate_handler!\[(.*?)\]", lib, re.S).group(1)
    check("handler list equals allowed commands", sorted(x.strip() for x in reg.split(",") if x.strip()) == sorted(cmds))

    async with async_playwright() as p:
        b = await p.chromium.launch()
        # injection
        pg = await page(b, REAL)
        await pg.evaluate("openStartupModal()")
        await pg.wait_for_timeout(500)
        check("registry text is not turned into markup", not await pg.evaluate("!!document.getElementById('pwn')"))
        check("no attacker backend call", not await pg.evaluate("window.__calls.some(c=>c[0]==='apply_tweak')"))
        check("real mode shows no mode bar", await pg.evaluate("document.getElementById('mode-bar').hidden"))
        # toggle only flips after ok
        await pg.evaluate("window.__toggleResult={status:'failed',message:'nope'}")
        await pg.click("#su-card-0")
        await pg.wait_for_timeout(300)
        check("failed toggle leaves the switch unchanged", await pg.evaluate("document.getElementById('su-track-0').classList.contains('sw-track-on')"))
        await pg.evaluate("window.__toggleResult={status:'skipped',message:'Another change is still running.'}")
        await pg.click("#su-card-0")
        await pg.wait_for_timeout(300)
        check("busy toggle leaves the switch unchanged", await pg.evaluate("document.getElementById('su-track-0').classList.contains('sw-track-on')"))
        await pg.evaluate("window.__toggleResult={status:'ok',message:'ok'}")
        await pg.click("#su-card-0")
        await pg.wait_for_timeout(300)
        check("confirmed toggle moves the switch", await pg.evaluate("!document.getElementById('su-track-0').classList.contains('sw-track-on')"))
        await pg.close()
        # broken backend link
        pg = await page(b, BROKEN)
        bar = await pg.evaluate("document.getElementById('mode-bar').textContent")
        check("broken mode shows the red bar", "BACKEND NOT CONNECTED" in bar)
        await pg.evaluate("deployTuning()")
        await pg.evaluate("deployTuning()")
        await pg.wait_for_timeout(300)
        log = await pg.evaluate("document.body.innerText")
        check("broken mode never reaches the simulated success path", "DEMO ONLY" not in log and "simulated" not in log)
        await pg.close()
        # browser demo
        pg = await page(b, None)
        bar = await pg.evaluate("document.getElementById('mode-bar').textContent")
        check("browser shows the demo bar", "DEMO MODE" in bar)
        await b.close()
    print("\n%d failed" % len(FAILS))
    sys.exit(1 if FAILS else 0)

asyncio.run(main())

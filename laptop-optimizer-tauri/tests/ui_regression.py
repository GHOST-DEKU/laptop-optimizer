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
 if(c==='get_live_telemetry')return window.__telem||{cpu_mhz:2803,ram_used_gb:12.3,ram_total_gb:15.6,ram_usage_pct:79,ssd_free_gb:62.2,ssd_total_gb:355.6,cpu_load:[5,50,100,0]};
 if(c==='get_system_info')return {model:'// TEST MAKER TEST MODEL',cpu_name:'Test CPU 9000',cores:'[8 Threads]',specs:'\\u2022 16 GB RAM \\u2022 356 GB disk'};
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
    js = (ROOT / "src" / "app.js").read_text()
    for bad in ["LENOVO", "82HS", "1165G7", "Tiger Lake", "4C/8T", "6.22", "512GB", "NVMe 512"]:
        check("no author-machine text: " + bad, bad not in html and bad not in js)
    conf = json.loads((ROOT / "src-tauri" / "tauri.conf.json").read_text())
    csp = conf["app"]["security"]["csp"]
    check("CSP is set and has no unsafe-eval or script unsafe-inline",
          bool(csp) and "unsafe-eval" not in csp and not re.search(r"script-src[^;]*unsafe-inline", csp))
    check("withGlobalTauri is on", conf["app"].get("withGlobalTauri") is True)
    caps = json.loads((ROOT / "src-tauri" / "capabilities" / "default.json").read_text())["permissions"]
    cmds = re.findall(r'"([a-z_]+)",', (ROOT / "src-tauri" / "build.rs").read_text().split("fn main")[0])
    check("every listed command is granted", all("allow-" + c.replace("_", "-") in caps for c in cmds) and len(cmds) == 9)
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
        check("no tweak starts switched on", await pg.evaluate("tweaks.length===8 && tweaks.every(t=>!t.on)"))
        # live load map: one box per logical processor, filled from measured load, independent of the cards
        boxes = "[...document.querySelectorAll('#die-cores .die-core-box')].map(b=>[b.textContent,b.style.getPropertyValue('--load')])"
        check("load map has one box per logical processor with its measured percent",
              await pg.evaluate(boxes) == [["T0 5%", "5"], ["T1 50%", "50"], ["T2 100%", "100"], ["T3 0%", "0"]])
        check("only the busy box is marked hot",
              await pg.evaluate("[...document.querySelectorAll('#die-cores .die-core-box')].map(b=>b.classList.contains('hot'))") == [False, False, True, False])
        before_map = await pg.evaluate("document.getElementById('die-cores').innerHTML")
        await pg.evaluate("toggleItem(1)")
        check("switching card 1 on does not change the load map", before_map == await pg.evaluate("document.getElementById('die-cores').innerHTML"))
        await pg.evaluate("toggleItem(1)")
        check("RAM box shows the total and the measured use", (await pg.evaluate("document.getElementById('core-ram').innerText")).replace("\n", "") == "RAM16GB 79%")
        check("RAM box fill follows the measured use", await pg.evaluate("document.getElementById('core-ram').style.getPropertyValue('--load')") == "79")
        check("old always-lit bus lines are gone", await pg.evaluate("document.querySelectorAll('.die-bus-line').length") == 0)
        # disk free line: free / total from the backend, and 'gained' = rise since the app opened
        label = await pg.evaluate("document.getElementById('telem-storage-gain').previousElementSibling.textContent")
        check("disk label says 'since app opened', not 'gained'", label == "DISK FREE // SINCE APP OPENED" and "GAINED" not in label.upper())
        check("disk line shows free / total exactly as reported", await pg.evaluate("document.getElementById('telem-storage-gain').innerText") == "62.2 / 355.6 GB")
        await pg.evaluate("window.__telem={cpu_mhz:2803,ram_used_gb:12.3,ram_total_gb:15.6,ram_usage_pct:79,ssd_free_gb:68.3,ssd_total_gb:355.6}")
        await pg.wait_for_timeout(2200)
        check("disk line shows the rise since opening (+6.1 GB)", await pg.evaluate("document.getElementById('telem-storage-gain').innerText") == "68.3 / 355.6 GB (+6.1 GB)")
        await pg.evaluate("window.__telem={cpu_mhz:2803,ram_used_gb:12.3,ram_total_gb:15.6,ram_usage_pct:79,ssd_free_gb:50.0,ssd_total_gb:355.6}")
        await pg.wait_for_timeout(2200)
        check("disk line shows no 'gained' text when space shrinks", await pg.evaluate("document.getElementById('telem-storage-gain').innerText") == "50.0 / 355.6 GB")
        # the number of boxes follows the data; no data shows a message instead of invented boxes
        base = "cpu_mhz:2803,ram_used_gb:12.3,ram_total_gb:15.6,ram_usage_pct:79,ssd_free_gb:62.2,ssd_total_gb:355.6"
        await pg.evaluate("window.__telem={%s,cpu_load:[10,20]}" % base)
        await pg.wait_for_timeout(2200)
        check("box count follows the number of processors reported", await pg.evaluate("document.querySelectorAll('#die-cores .die-core-box').length") == 2)
        await pg.evaluate("window.__telem={%s,cpu_load:[]}" % base)
        await pg.wait_for_timeout(2200)
        check("no reading shows 'CPU load unavailable', not made-up boxes",
              await pg.evaluate("document.getElementById('die-cores').textContent") == "CPU load unavailable")
        await pg.evaluate("window.__telem=null")
        hdr = await pg.evaluate("({model:document.getElementById('system-model').textContent, cpu:document.getElementById('cpu-name').textContent, cores:document.getElementById('cpu-cores')&&document.getElementById('cpu-cores').textContent, specs:document.getElementById('system-specs')&&document.getElementById('system-specs').textContent})")
        check("header shows the device model (not 'READING DEVICE')", hdr["model"] == "// TEST MAKER TEST MODEL")
        check("header shows processor, thread count and RAM/disk together",
              "Test CPU 9000" in hdr["cpu"] and hdr["cores"] == "[8 Threads]" and hdr["specs"] == "• 16 GB RAM • 356 GB disk")
        check("console log text can be selected and copied",
              await pg.evaluate("getComputedStyle(document.getElementById('drawer-logs')).userSelect==='text'")
              and await pg.evaluate("getComputedStyle(document.querySelector('.tweak-card')).userSelect==='none'"))
        # really select and read it back, as Ctrl+C would
        await pg.evaluate("document.getElementById('docked-drawer').classList.add('expanded')")  # a user copies from the open console
        await pg.evaluate("logToDrawer('// test line: error 0x80070005','err')")
        picked = await pg.evaluate("""(()=>{const el=document.querySelector('#drawer-logs .log-line:last-child');
          const r=document.createRange(); r.selectNodeContents(el); const s=getSelection(); s.removeAllRanges(); s.addRange(r); return s.toString();})()""")
        check("selecting a console line returns its text", picked == "// test line: error 0x80070005")
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
        # confirmation for tweaks 4 and 5
        for accept in (False, True):
            pg2 = await b.new_page()
            pg2.on("dialog", lambda d, a=accept: asyncio.ensure_future(d.accept() if a else d.dismiss()))
            await pg2.add_init_script(REAL)
            await pg2.goto(URL)
            await pg2.wait_for_timeout(300)
            await pg2.evaluate("tweaks.forEach(t=>t.on=[4,5].includes(t.id)); coverOpen=true")
            await pg2.evaluate("deployTuning()")
            await pg2.wait_for_timeout(600)
            calls = await pg2.evaluate("window.__calls.filter(c=>c[0]==='apply_tweak').map(c=>c[1])")
            if accept:
                check("accepted dialogs send the matching word", calls == [{"id": 4, "confirmation": "remove-widgets"}, {"id": 5, "confirmation": "turn-off-hibernation"}])
            else:
                check("declined dialogs make no backend call", calls == [])
            await pg2.close()
        # a tweak that is already applied: no question, no apply call, a "no change" line
        asked = []
        pg3 = await b.new_page()
        pg3.on("dialog", lambda d: (asked.append(d.message), asyncio.ensure_future(d.dismiss())))
        await pg3.add_init_script(REAL.replace("if(c==='apply_tweak')", "if(c==='is_applied')return true; if(c==='apply_tweak')"))
        await pg3.goto(URL)
        await pg3.wait_for_timeout(300)
        await pg3.evaluate("tweaks.forEach(t=>t.on=[4,5].includes(t.id)); coverOpen=true")
        await pg3.evaluate("deployTuning()")
        await pg3.wait_for_timeout(600)
        applied_calls = await pg3.evaluate("window.__calls.filter(c=>c[0]==='apply_tweak').length")
        text = await pg3.evaluate("document.getElementById('drawer-logs').innerText")
        check("already-applied tweaks ask no question", asked == [])
        check("already-applied tweaks make no apply call", applied_calls == 0)
        check("already-applied tweaks are shown as no change", text.count("already applied; nothing was changed") == 2 and "NOTHING TO CHANGE" in await pg3.evaluate("document.getElementById('drawer-status-pill').innerText"))
        await pg3.close()
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

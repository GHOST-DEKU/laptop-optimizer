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
    check("every listed command is granted", all("allow-" + c.replace("_", "-") in caps for c in cmds) and len(cmds) == 10)
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
        # capacity battery: one cell per logical processor, filled from measured load, independent of the cards
        cells = "[...document.querySelectorAll('#cap-cells .cap-cell')].map(c=>[c.textContent,c.style.getPropertyValue('--f'),c.dataset.load])"
        check("battery has one cell per logical processor with its measured load",
              await pg.evaluate(cells) == [["T0 5", "5", "5"], ["T1 50", "50", "50"], ["T2 100", "100", "100"], ["T3 0", "0", "0"]])
        check("only the busy cell is marked hot",
              await pg.evaluate("[...document.querySelectorAll('#cap-cells .cap-cell')].map(c=>c.classList.contains('hot'))") == [False, False, True, False])
        verdict = "document.getElementById('cap-verdict').textContent"
        check("overall load is the average (39%), with a verdict and the busiest processor",
              await pg.evaluate("document.getElementById('cap-total').textContent") == "39%"
              and "Working" in await pg.evaluate(verdict) and "busiest T2 100%" in await pg.evaluate(verdict))
        before_map = await pg.evaluate("document.getElementById('silicon-banner').innerHTML")
        await pg.evaluate("toggleItem(1)")
        check("switching card 1 on does not change the load map", before_map == await pg.evaluate("document.getElementById('silicon-banner').innerHTML"))
        await pg.evaluate("toggleItem(1)")
        check("RAM meter shows the measured use and total",
              "RAM 79% of 16 GB" in await pg.evaluate("document.getElementById('cap-ram').textContent")
              and await pg.evaluate("[...document.querySelectorAll('#cap-ram s')].filter(s=>s.style.background!=='').length") == 13)
        # drive line of the deploy plan: free / total from the backend, and 'since opened' = rise since the app opened
        drive = "document.getElementById('plan-drive').textContent"
        check("deploy plan shows the drive free and total exactly as reported", await pg.evaluate(drive) == "Drive 62.2 GB free of 355.6")
        base = "cpu_mhz:2803,ram_used_gb:12.3,ram_total_gb:15.6,ram_usage_pct:79,ssd_total_gb:355.6"
        await pg.evaluate("window.__telem={%s,ssd_free_gb:68.3,cpu_load:[5,50,100,0]}" % base)
        await pg.wait_for_timeout(2200)
        check("drive line shows the rise since opening (+6.1 GB)", await pg.evaluate(drive) == "Drive 68.3 GB free of 355.6 · +6.1 GB since opened")
        await pg.evaluate("window.__telem={%s,ssd_free_gb:50.0,cpu_load:[5,50,100,0]}" % base)
        await pg.wait_for_timeout(2200)
        check("drive line shows no 'since opened' text when space shrinks", await pg.evaluate(drive) == "Drive 50.0 GB free of 355.6")
        check("drive meter shows the used share", await pg.evaluate("document.getElementById('plan-meter').style.width") == "86%")
        # the verdict words follow the average load
        for loads, word in (([4, 8, 6, 10], "Running calm"), ([80, 80, 80, 80], "Busy"), ([95, 98, 99, 97], "Straining")):
            await pg.evaluate("window.__telem={%s,ssd_free_gb:62.2,cpu_load:[%s]}" % (base, ",".join(map(str, loads))))
            await pg.wait_for_timeout(2200)
            check("verdict for loads %s is '%s'" % (loads, word), word in await pg.evaluate(verdict))
        # the number of cells follows the data; no data shows a message instead of invented cells
        await pg.evaluate("window.__telem={%s,ssd_free_gb:62.2,cpu_load:[10,20]}" % base)
        await pg.wait_for_timeout(2200)
        check("cell count follows the number of processors reported", await pg.evaluate("document.querySelectorAll('#cap-cells .cap-cell').length") == 2)
        await pg.evaluate("window.__telem={%s,ssd_free_gb:62.2,cpu_load:[]}" % base)
        await pg.wait_for_timeout(2200)
        check("no reading shows 'CPU load unavailable', not made-up cells",
              await pg.evaluate("document.getElementById('cap-cells').textContent") == "CPU load unavailable"
              and await pg.evaluate("document.getElementById('cap-total').textContent") == "--")
        # deploy plan: what a deploy would do, from the cards that are switched on
        pl = "({count:document.getElementById('plan-count').innerText, chips:document.getElementById('plan-chips').innerText.replace(/\\s+/g,' ').trim()})"
        p0 = await pg.evaluate(pl)
        check("deploy plan starts as 'nothing selected'", p0["count"] == "nothing selected" and "Switch on the tweaks" in p0["chips"])
        await pg.evaluate("tweaks.forEach(t=>t.on=[1,4,5].includes(t.id)); renderGrid()")
        p1 = await pg.evaluate(pl)
        check("plan counts the selected cards, the administrator needs (1, 4, 5, 7) and the confirmations (4, 5)",
              p1["count"] == "3 of 8 selected" and "3 need administrator ✓" in p1["chips"] and "2 will ask to confirm" in p1["chips"])
        await pg.evaluate("tweaks.forEach(t=>t.on=[2,3,6].includes(t.id)); renderGrid()")
        p2 = await pg.evaluate(pl)
        check("plan says no administrator is needed when none of the selected tweaks needs one", "no administrator needed" in p2["chips"] and "confirm" not in p2["chips"])
        await pg.evaluate("tweaks.forEach(t=>t.on=false); renderGrid()")
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
        # Restore asks a second question only for tweaks changed after the app set them, and passes on only the agreed ones
        async def restore_run(changed, answers, check_fails=False):
            pgr = await b.new_page()
            msgs = []
            async def on_dialog(d):
                msgs.append(d.message)
                await (d.accept() if answers[len(msgs) - 1] else d.dismiss())
            pgr.on("dialog", lambda d: asyncio.ensure_future(on_dialog(d)))
            init = REAL.replace(
                "if(c==='apply_tweak')",
                ("if(c==='restore_needs_confirmation')throw new Error('check failed'); " if check_fails else "if(c==='restore_needs_confirmation')return " + json.dumps(changed) + "; ")
                + "if(c==='restore_all')return [1,2,3,4,5,6,7].map(()=>({status:'ok',message:'put back'})); if(c==='apply_tweak')")
            await pgr.add_init_script(init)
            await pgr.goto(URL)
            await pgr.wait_for_timeout(300)
            await pgr.evaluate("restoreDefaults()")
            await pgr.wait_for_timeout(300)
            sent = await pgr.evaluate("window.__calls.filter(c=>c[0]==='restore_all').map(c=>c[1])")
            text = await pgr.evaluate("document.getElementById('drawer-logs').innerText")
            await pgr.close()
            return msgs, sent, text
        msgs, sent, _ = await restore_run([], [True])
        check("Restore with nothing changed since asks one question only", len(msgs) == 1)
        check("Restore with nothing changed since overwrites nothing extra", sent == [{"overwrite": []}])
        msgs, sent, _ = await restore_run([3, 6], [True, False])
        check("Restore names the tweaks changed since the app set them in a second question", len(msgs) == 2 and "Instant Desktop Context Menus" in msgs[1] and "Decouple Bing Cloud" in msgs[1])
        check("declining the second question restores the rest but overwrites none", sent == [{"overwrite": []}])
        msgs, sent, _ = await restore_run([3, 6], [True, True])
        check("accepting the second question passes exactly those tweaks", sent == [{"overwrite": [3, 6]}])
        msgs, sent, _ = await restore_run([3], [False])
        check("declining the first question stops everything: no second question, no restore call", len(msgs) == 1 and sent == [])
        msgs, sent, text = await restore_run([], [True], check_fails=True)
        check("if the check fails nothing is overwritten, the user is told, and Restore still runs", sent == [{"overwrite": []}] and "Nothing will be overwritten" in text and len(msgs) == 1)

        # load map layout at the real window size (940), the minimum (700), and with many processors
        for width, n, max_rows in ((940, 8, 1), (700, 8, 1), (940, 16, 2)):
            lp = await b.new_page(viewport={"width": width, "height": 680})
            await lp.add_init_script(REAL.replace("cpu_load:[5,50,100,0]", "cpu_load:[" + ",".join(["40"] * n) + "]"))
            await lp.goto(URL)
            await lp.wait_for_timeout(700)
            lay = await lp.evaluate("""(()=>{const bs=[...document.querySelectorAll('#cap-cells .cap-cell')];
              const rows=new Set(bs.map(x=>Math.round(x.getBoundingClientRect().top))).size;
              const hud=document.getElementById('silicon-banner').getBoundingClientRect();
              const inside=bs.every(x=>{const r=x.getBoundingClientRect();return r.left>=hud.left-1&&r.right<=hud.right+1});
              return {count:bs.length, rows, inside, overflow:document.documentElement.scrollWidth>window.innerWidth};})()""")
            check("load map: %d processors in %d px wide window fit in <=%d row(s), inside the panel, no sideways scroll" % (n, width, max_rows),
                  lay["count"] == n and lay["rows"] <= max_rows and lay["inside"] and not lay["overflow"])
            await lp.close()
        # layout a little below the design size (940x680) down to the minimum (700x500)
        SIZES = ((940, 680, 2), (900, 640, 2), (800, 560, 2), (760, 540, 2), (700, 500, 1))
        for w, h, want_cols in SIZES:
            sp = await b.new_page(viewport={"width": w, "height": h})
            await sp.add_init_script(REAL)
            await sp.goto(URL)
            await sp.wait_for_timeout(700)
            m = await sp.evaluate("""(()=>{
              const foot=document.querySelector('.app-footer'); const items=[...foot.querySelectorAll('button.btn-secondary'), document.getElementById('safety-assembly')].map(e=>e.getBoundingClientRect());
              let overlap=false; for(let i=0;i<items.length;i++)for(let j=i+1;j<items.length;j++){const a=items[i],c=items[j]; if(a.left<c.right-1&&c.left<a.right-1&&a.top<c.bottom-1&&c.top<a.bottom-1) overlap=true;}
              const launch=document.getElementById('safety-assembly').getBoundingClientRect();
              const title=document.querySelector('.die-head').getBoundingClientRect();
              const boxes=[...document.querySelectorAll('#cap-cells .cap-cell')].map(x=>x.getBoundingClientRect());
              const vp=document.querySelector('.tweak-viewport').getBoundingClientRect();
              return {footerFits: foot.scrollWidth<=foot.clientWidth, overlap, launchInside: launch.right<=window.innerWidth && launch.left>=0,
                      cols:getComputedStyle(document.getElementById('tweak-grid')).gridTemplateColumns.split(' ').length,
                      titleOneLine: title.height<30, mapRows:new Set(boxes.map(x=>Math.round(x.top))).size, boxW:Math.round(boxes[0].width),
                      cardPct:Math.round(vp.height/window.innerHeight*100), sideways:document.documentElement.scrollWidth>window.innerWidth};})()""")
            tag = "%dx%d" % (w, h)
            check("%s: footer fits in one row, nothing overlaps, launch box inside the window" % tag, m["footerFits"] and not m["overlap"] and m["launchInside"])
            check("%s: %d card column(s), no sideways scroll" % (tag, want_cols), m["cols"] == want_cols and not m["sideways"])
            check("%s: load map title on one line, one row of cells, cells at least 54 px wide" % tag, m["titleOneLine"] and m["mapRows"] == 1 and m["boxW"] >= 54)
            check("%s: cards get at least half of the window height" % tag, m["cardPct"] >= 50)
            await sp.close()
        # deploy plan shape: a card beside the title when there is room, ONE compact line when it has to drop below it
        LONG = REAL.replace("cpu_name:'Test CPU 9000'", "cpu_name:'11th Gen Intel Core i7-1165G7'")
        for w, line in ((1100, False), (860, True), (700, True)):
            pp = await b.new_page(viewport={"width": w, "height": 640})
            await pp.add_init_script(LONG)
            await pp.goto(URL)
            await pp.wait_for_timeout(900)
            await pp.evaluate("tweaks.forEach(t=>t.on=[1,4,5].includes(t.id)); renderGrid()")
            await pp.wait_for_timeout(300)
            g = await pp.evaluate("""(()=>{const c=document.getElementById('plan-card'), t=document.getElementById('cpu-name'), h=document.querySelector('.app-header');
              const r=c.getBoundingClientRect(), hr=h.getBoundingClientRect();
              return {line:c.classList.contains('plan-line'), beside:r.top<t.getBoundingClientRect().bottom-4, cardH:Math.round(r.height), headerH:Math.round(hr.height), inside:r.right<=hr.right+1&&r.left>=hr.left-1}})()""")
            ok = (g["line"] and not g["beside"] and g["cardH"] <= 40 and g["headerH"] <= 125 and g["inside"]) if line else ((not g["line"]) and g["beside"] and g["inside"])
            check("deploy plan at %d px wide is %s" % (w, "one compact line under the title (<= 40 px, header <= 125 px)" if line else "a card beside the title"), ok)
            await pp.close()
        # nothing selected, narrow window: the compact line must still be ONE line (the hint is shortened)
        nl = await b.new_page(viewport={"width": 700, "height": 500})
        await nl.add_init_script(LONG)
        await nl.goto(URL)
        await nl.wait_for_timeout(900)
        nlh = await nl.evaluate("(()=>{const c=document.getElementById('plan-card');return {line:c.classList.contains('plan-line'), h:Math.round(c.getBoundingClientRect().height)}})()")
        check("with nothing selected the narrow plan is still one line (<= 40 px)", nlh["line"] and nlh["h"] <= 40)
        await nl.close()        # not running as administrator: the plan says so
        na = await b.new_page()
        await na.add_init_script(REAL.replace("if(c==='get_elevation')return true;", "if(c==='get_elevation')return false;"))
        await na.goto(URL)
        await na.wait_for_timeout(700)
        await na.evaluate("tweaks.forEach(t=>t.on=[1].includes(t.id)); renderGrid()")
        check("without administrator rights the plan warns about it", "this app is not running as one" in (await na.evaluate("document.getElementById('plan-chips').innerText")))
        await na.close()
        # demo header shows sample values, not [--] placeholders
        dp = await page(b, None)
        dh = await dp.evaluate("({c:document.getElementById('cpu-cores').textContent, s:document.getElementById('system-specs').textContent})")
        check("demo header shows sample threads and RAM/disk, not placeholders", dh["c"] == "[8 Threads]" and "--" not in dh["s"] and "16 GB RAM" in dh["s"])
        await dp.close()
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

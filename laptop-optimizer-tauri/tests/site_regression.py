"""Browser tests for the project website (index.html at the repository root). Run: python3 tests/site_regression.py
Needs: pip install playwright && playwright install chromium"""
import asyncio, hashlib, pathlib, sys
from playwright.async_api import async_playwright

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
URL = (REPO / "index.html").as_uri()
FAILS = []


def check(name, ok):
    print(("PASS " if ok else "FAIL ") + name)
    if not ok:
        FAILS.append(name)


async def rect(pg, sel):
    r = await pg.evaluate("(s)=>{const b=document.querySelector(s).getBoundingClientRect();return [b.left,b.top,b.right,b.bottom]}", sel)
    return [round(v) for v in r]


async def inner(pg):
    r = await pg.evaluate("(()=>{const b=document.getElementById('app-live-frame').getBoundingClientRect();return [b.width,b.height]})()")
    return [round(v) for v in r]


async def drag(pg, sel, dx, dy, ox=0, oy=0):
    """Press on the middle of `sel` (plus an offset), move by dx,dy, release."""
    box = await pg.locator(sel).bounding_box()
    x, y = box["x"] + box["width"] / 2 + ox, box["y"] + box["height"] / 2 + oy
    await pg.mouse.move(x, y)
    await pg.mouse.down()
    await pg.mouse.move(x + dx, y + dy, steps=6)
    await pg.mouse.up()


async def reset(pg):
    await pg.reload()
    await pg.wait_for_timeout(1200)
    await pg.evaluate("(()=>{const d=document.getElementById('app-desktop');window.scrollTo(0,d.getBoundingClientRect().top+window.scrollY-120)})()")
    await pg.wait_for_timeout(200)


async def main():
    sha = hashlib.sha256((REPO / "LaptopOptimizer.exe").read_bytes()).hexdigest()
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page(viewport={"width": 1500, "height": 1000})
        errs = []
        pg.on("pageerror", lambda e: errs.append(str(e)))
        await pg.goto(URL)
        await pg.wait_for_timeout(1500)
        await pg.evaluate("(()=>{const d=document.getElementById('app-desktop');window.scrollTo(0,d.getBoundingClientRect().top+window.scrollY-120)})()")
        await pg.wait_for_timeout(200)

        txt = await pg.evaluate("document.body.innerText")
        check("the author's computer name is not shown", "LENOVO-82HS" not in txt)
        check("the older-build notice is shown", "older, unsigned build" in txt)
        check("the page shows the SHA-256 of the committed .exe", sha in (await pg.content()))
        check("resizing needs no buttons: the demo stage has none", await pg.evaluate("document.querySelectorAll('#app-preview button').length") == 0)
        check("the demo window opens at the real app's size, 940 x 680", await inner(pg) == [940, 680])

        # --- all eight edges and corners: the dragged edge moves, the opposite edge stays
        win0 = await rect(pg, "#app-window")
        cases = {  # direction: (dx, dy, which edges should move: L T R B)
            "e": (40, 0, "R"), "w": (-40, 0, "L"), "s": (0, 30, "B"), "n": (0, -20, "T"),
            "se": (40, 30, "RB"), "sw": (-40, 30, "LB"), "ne": (40, -20, "RT"), "nw": (-40, -20, "LT"),
        }
        ok_all = True
        for d, (dx, dy, moved) in cases.items():
            await reset(pg)
            before = await rect(pg, "#app-window")
            await drag(pg, ".rz-" + d, dx, dy)
            after = await rect(pg, "#app-window")
            exp = list(before)
            if "L" in moved: exp[0] += dx
            if "R" in moved: exp[2] += dx
            if "T" in moved: exp[1] += dy
            if "B" in moved: exp[3] += dy
            good = all(abs(a - e) <= 1 for a, e in zip(after, exp))
            ok_all &= good
            if not good:
                print("   edge", d, "expected", exp, "got", after)
        check("each of the 8 edges and corners moves only its own edges; the opposite edges stay put", ok_all)

        # --- limits
        await reset(pg)
        before = await rect(pg, "#app-window")
        await drag(pg, ".rz-se", -2000, -2000)
        small = await rect(pg, "#app-window")
        check("the window cannot get smaller than the app's minimum, 700 x 500", await inner(pg) == [700, 500] and small[0] == before[0] and small[1] == before[1])
        await reset(pg)
        await drag(pg, ".rz-se", 3000, 3000)
        dk = await rect(pg, "#app-desktop")
        big = await rect(pg, "#app-window")
        check("the window cannot leave the desktop (right and bottom edges stop at its border)", big[2] <= dk[2] and big[3] <= dk[3] and big[2] >= dk[2] - 2 and big[3] >= dk[3] - 2)

        # --- move and maximize
        await reset(pg)
        before = await rect(pg, "#app-window")
        await drag(pg, "#app-titlebar", 60, 25, ox=-200)
        after = await rect(pg, "#app-window")
        check("dragging the title bar moves the window without changing its size",
              abs(after[0] - before[0] - 60) <= 1 and abs(after[1] - before[1] - 25) <= 1 and (after[2] - after[0]) == (before[2] - before[0]))
        await reset(pg)
        await pg.locator("#app-titlebar").dblclick(position={"x": 200, "y": 10})
        mx = await rect(pg, "#app-window")
        dk = await rect(pg, "#app-desktop")
        check("double-clicking the title bar maximizes the window to the whole desktop", abs(mx[0] - dk[0]) <= 2 and abs(mx[2] - dk[2]) <= 2 and abs(mx[3] - dk[3]) <= 2)
        await pg.locator("#app-titlebar").dblclick(position={"x": 200, "y": 10})
        check("double-clicking again restores the previous size", await inner(pg) == [940, 680])

        # --- no drag gets stuck on the iframe
        await reset(pg)
        box = await pg.locator(".rz-se").bounding_box()
        await pg.mouse.move(box["x"] + 5, box["y"] + 5); await pg.mouse.down(); await pg.mouse.move(box["x"] - 20, box["y"] - 20, steps=3)
        during = await pg.evaluate("document.getElementById('app-window').classList.contains('dragging')")
        await pg.mouse.up()
        after = await pg.evaluate("document.getElementById('app-window').classList.contains('dragging')")
        check("the page, not the demo inside, gets the pointer during a drag, and lets go afterwards", during and not after)

        # --- keyboard
        await reset(pg)
        await pg.focus("#app-window")
        before = await rect(pg, "#app-window")
        await pg.keyboard.press("ArrowRight")
        moved = await rect(pg, "#app-window")
        await pg.keyboard.press("Shift+ArrowDown")
        grown = await rect(pg, "#app-window")
        await pg.keyboard.press("Enter")
        maxed = await rect(pg, "#app-window")
        await pg.keyboard.press("Enter")
        back = await rect(pg, "#app-window")
        check("keyboard: arrows move by 20, Shift+arrows resize by 20, Enter maximizes and restores",
              moved[0] == before[0] + 20 and grown[3] == moved[3] + 20 and abs(maxed[2] - maxed[0] - (dk[2] - dk[0])) <= 2 and back == grown)

        # --- the app inside re-lays out as the window changes, like the real app
        await reset(pg)
        app = [f for f in pg.frames if "laptop-optimizer-tauri" in f.url][0]
        cols = "getComputedStyle(document.getElementById('tweak-grid')).gridTemplateColumns.split(' ').length"
        wide = await app.evaluate(cols)
        await drag(pg, ".rz-se", -2000, -2000)
        await pg.wait_for_timeout(300)
        narrow = await app.evaluate(cols)
        fits = await app.evaluate("(()=>{const f=document.querySelector('.app-footer');return f.scrollWidth<=f.clientWidth})()")
        check("the app inside re-lays out: 2 card columns at 940 wide, 1 at 700 wide, footer still fits", wide == 2 and narrow == 1 and fits)
        check("no page errors", errs == [])
        await pg.close()

        # --- a phone-width screen: no desktop, the window is shown scaled down, no resizing, no sideways scroll
        ph = await b.new_page(viewport={"width": 390, "height": 800})
        await ph.goto(URL)
        await ph.wait_for_timeout(1500)
        m = await ph.evaluate("""(()=>({noDesk:document.getElementById('app-preview').classList.contains('no-desk'),
            zone:getComputedStyle(document.querySelector('.rz-se')).display,
            scaled:document.getElementById('app-live-frame').style.transform.startsWith('scale('),
            sideways:document.documentElement.scrollWidth>window.innerWidth}))()""")
        check("on a phone-width screen the window is scaled down, cannot be resized, and the page does not scroll sideways",
              m["noDesk"] and m["zone"] == "none" and m["scaled"] and not m["sideways"])
        await b.close()
    print("\n%d failed" % len(FAILS))
    sys.exit(1 if FAILS else 0)


asyncio.run(main())

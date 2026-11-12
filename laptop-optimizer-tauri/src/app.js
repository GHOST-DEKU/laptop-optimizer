    // --- PROCEDURAL WEB AUDIO API ACOUSTICS (TASK 59.5) ---
    let audioCtx = null;
    let sfxEnabled = true;

    function getAudioCtx() {
      if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
      }
      return audioCtx;
    }

    function toggleAudio() {
      sfxEnabled = !sfxEnabled;
      const icon = document.getElementById("audio-icon");
      const label = document.getElementById("audio-label");
      if (sfxEnabled) {
        if (icon) icon.innerText = "🔊";
        if (label) label.innerText = "SFX: ON";
        playAudio('click');
      } else {
        if (icon) icon.innerText = "🔇";
        if (label) label.innerText = "SFX: OFF";
      }
    }

    function playAudio(type) {
      if (!sfxEnabled) return;
      try {
        const ctx = getAudioCtx();
        if (!ctx) return;
        const osc = ctx.createOscillator();
        const gain = ctx.createGain();
        osc.connect(gain);
        gain.connect(ctx.destination);
        const t = ctx.currentTime;

        if (type === 'click') {
          osc.type = 'triangle';
          osc.frequency.setValueAtTime(820, t);
          gain.gain.setValueAtTime(0.04, t);
          gain.gain.exponentialRampToValueAtTime(0.001, t + 0.04);
          osc.start(t);
          osc.stop(t + 0.04);
        } else if (type === 'pneumatic') {
          osc.type = 'sawtooth';
          osc.frequency.setValueAtTime(1500, t);
          osc.frequency.exponentialRampToValueAtTime(360, t + 0.08);
          gain.gain.setValueAtTime(0.08, t);
          gain.gain.exponentialRampToValueAtTime(0.001, t + 0.09);
          osc.start(t);
          osc.stop(t + 0.09);
        } else if (type === 'vault') {
          osc.type = 'sine';
          osc.frequency.setValueAtTime(180, t);
          gain.gain.setValueAtTime(0.16, t);
          gain.gain.exponentialRampToValueAtTime(0.001, t + 0.16);
          osc.start(t);
          osc.stop(t + 0.16);
        } else if (type === 'success') {
          osc.type = 'sine';
          osc.frequency.setValueAtTime(523.25, t);
          osc.frequency.setValueAtTime(659.25, t + 0.08);
          osc.frequency.setValueAtTime(783.99, t + 0.16);
          osc.frequency.setValueAtTime(1046.50, t + 0.24);
          gain.gain.setValueAtTime(0.06, t);
          gain.gain.exponentialRampToValueAtTime(0.001, t + 0.38);
          osc.start(t);
          osc.stop(t + 0.38);
        }
      } catch (e) {}
    }

    // --- 8 TACTICAL OPTIMIZATION PROTOCOLS ---
    const tweaks = [
      { 
        id: 1,
        sys: "SYS: CPU_GOVERNOR [INTEL_SPEEDSTEP]",
        title: "Zero-Ramp CPU Frequency Floor on AC", 
        tag: "100% FLOOR", 
        desc: "Locks minimum processor floor to 100% on AC to bypass frequency ramp-up latency.", 
        gain: "GAIN: \u2212380ms DPC LATENCY",
        lockProtocol: "GOV_LOCK_AC",
        on: false 
      },
      { 
        id: 2,
        sys: "SYS: DIRECTX_DWM [FRAME_LATENCY]",
        title: "Bypass Game DVR Background Video Buf...", 
        tag: "ZERO VRAM DRAIN", 
        desc: "Disables continuous DirectX background video recording and capture hooks.", 
        gain: "GAIN: ZERO VRAM OVERHEAD",
        lockProtocol: "DWM_HOOK_KILL",
        on: false 
      },
      { 
        id: 3,
        sys: "SYS: USER32_SHELL [MENU_SPEED]",
        title: "Instant Desktop Context Menus", 
        tag: "20ms DELAY", 
        desc: "Reduces MenuShowDelay from 400ms to 20ms for instant desktop responsiveness.", 
        gain: "GAIN: \u2212380ms DELAY CUT",
        lockProtocol: "WIN32_DISPATCH",
        on: false 
      },
      { 
        id: 4,
        sys: "SYS: WEBVIEW2_DAEMON [RAM_PURGE]",
        title: "Purge Windows 11 Widget Daemon Pack", 
        tag: "\u2212300MB RAM", 
        desc: "Removes the Widgets (WebExperience) package for your account. Windows 11 may block apps from hiding the taskbar button; if so, hide it in Settings > Personalization > Taskbar.",
        gain: "GAIN: \u2212300MB LEAK RECLAIMED",
        lockProtocol: "VFS_ISOLATION",
        on: false 
      },
      { 
        id: 5,
        sys: "SYS: NT_KERNEL [HIBERNATE_DEL]",
        title: "Clean Kernel Boots & Delete hiberfil.s...", 
        tag: "FREES DISK", 
        desc: "Turns off hibernation and Fast Startup and deletes hiberfil.sys. Hibernate stops working until restored.", 
        gain: "GAIN: frees hiberfil.sys (size depends on your RAM)",
        lockProtocol: "KERNEL_FASTBOOT",
        on: false 
      },
      { 
        id: 6,
        sys: "SYS: SEARCH_HOST [CLOUD_DECOUPLE]",
        title: "Decouple Bing Cloud Integration from Sta...", 
        tag: "LOCAL ONLY", 
        desc: "Eliminates remote cloud latency on Start search; makes app launch search instant.", 
        gain: "GAIN: 0ms CLOUD ROUNDTRIP",
        lockProtocol: "OFFLINE_SEARCH",
        on: false 
      },
      { 
        id: 7,
        sys: "SYS: BITS_P2P [NETWORK_ISOLATE]",
        title: "Halt Delivery Optimization P2P Seeding", 
        tag: "HTTP ONLY", 
        desc: "Sets Delivery Optimization to HTTP only, so Windows Update does not exchange updates with other PCs. Needs administrator rights. Settings may then say some settings are managed by your organization.",
        gain: "CHECKED WITH WINDOWS AFTER APPLY",
        lockProtocol: "NET_P2P_DECOUPLE",
        on: false 
      },
      { 
        id: 8,
        sys: "SYS: NVME_VFS [TEMP_PURGE]",
        title: "Purge Stale Temporary Files (>24h)", 
        tag: "CLEAR TEMP", 
        desc: "Wipes stale %TEMP% artifacts older than 24h; frees disk space.", 
        gain: "GAIN: +1.4GB CLEARED",
        lockProtocol: "IO_TRIM_CACHE",
        on: false 
      }
    ];

    // Which world are we in? 'real' = installed app with a working backend,
    // 'broken' = inside the app window but the backend link is missing (nothing may run),
    // 'demo' = ordinary browser (nothing is ever changed).
    const hasApi = !!(window.__TAURI__ && window.__TAURI__.core);
    const MODE = hasApi ? 'real' : (window.__TAURI_INTERNALS__ ? 'broken' : 'demo');
    function backend(cmd, args) { return window.__TAURI__.core.invoke(cmd, args); }
    function showMode() {
      const bar = document.getElementById('mode-bar');
      if (MODE === 'real') return;
      bar.hidden = false;
      if (MODE === 'broken') {
        bar.className = 'mode-bar mode-broken';
        bar.textContent = 'BACKEND NOT CONNECTED: nothing can be applied or restored. Close and reinstall the app.';
      } else {
        document.getElementById('system-model').textContent = '// DEMO DEVICE';
        document.getElementById('cpu-name').firstChild.textContent = 'DEMO PREVIEW ';
        // Sample values, like the other demo numbers (8 processors in the load map, 477 GB drive); the demo bar says so.
        document.getElementById('cpu-cores').textContent = '[8 Threads]';
        document.getElementById('system-specs').textContent = '• 16 GB RAM • 477 GB disk';
        bar.className = 'mode-bar mode-demo';
        bar.textContent = 'DEMO MODE: this is a preview in a browser. Nothing on your device is changed.';
      }
    }
    function blockedByMode(what) {
      if (MODE !== 'broken') return false;
      logToDrawer(`// BLOCKED: backend not connected, so "${what}" was not run.`, 'err');
      return true;
    }
    // These two remove or switch off a Windows feature, so each asks its own question.
    // The backend refuses them without the matching word (see safety.rs).
    const CONFIRMATIONS = {
      4: { word: 'remove-widgets', text: 'Remove Windows Widgets?\n\nThis hides the Widgets button and uninstalls the Widgets component. Restore brings the button back, but the component must be reinstalled from the Microsoft Store.' },
      5: { word: 'turn-off-hibernation', text: 'Turn off hibernation and Fast Startup?\n\nHibernate stops working and hiberfil.sys is deleted. Restore turns hibernation back on.' }
    };
    let working = false; // the backend lock is the real guard; this only avoids double clicks

    // --- RENDER 4. TACTICAL LOCK-BAY CARDS GRID ---
    function renderGrid() {
      const container = document.getElementById("tweak-grid");
      container.innerHTML = "";

      tweaks.forEach(item => {
        const card = document.createElement("div");
        card.className = "tweak-card" + (item.on ? " card-active" : "");
        card.onclick = () => toggleItem(item.id);
        card.innerHTML = `
          <div class="tweak-left">
            <div class="tweak-channel-badge">
              <span class="channel-prefix">CH</span>
              <span class="channel-num">${item.id < 10 ? '0' + item.id : item.id}</span>
            </div>
            <div class="tweak-content-col">
              <div class="tweak-sys-path">${item.sys}</div>
              <div class="tweak-title-row">
                <span class="tweak-title">${item.title}</span>
                <span class="tag-benefit ${item.on ? 'tag-on' : 'tag-off'}">${item.tag}</span>
              </div>
              <div class="tweak-desc">${item.desc}</div>
              <div class="tweak-gain-row">
                <div class="tweak-gain-bar"><span></span><span></span><span></span><span></span></div>
                <span class="tweak-gain-text">${item.gain}</span>
              </div>
              <div class="hazard-lock-badge ${item.on ? 'lock-armed' : 'lock-standby'}">
                <span>${item.on ? '⬡ ARMED // PROTOCOL_LOCK' : 'STANDBY // UNLATCHED'}</span>
                <span style="opacity: 0.6; font-size: 7px;">[${item.lockProtocol}]</span>
              </div>
            </div>
          </div>
          <div class="switch-bay">
            <div class="switch-track ${item.on ? 'sw-track-on' : 'sw-track-off'}">
              <div class="switch-thumb ${item.on ? 'sw-thumb-on' : 'sw-thumb-off'}">${item.on ? 'ON' : 'OFF'}</div>
            </div>
          </div>
        `;
        container.appendChild(card);
      });

      updatePlan();
    }

    function toggleItem(id) {
      playAudio('click');
      const item = tweaks.find(t => t.id === id);
      if (item) {
        item.on = !item.on;
        renderGrid();
      }
    }

    function toggleAll() {
      playAudio('click');
      const anyOn = tweaks.some(t => t.on);
      tweaks.forEach(t => t.on = !anyOn);
      renderGrid();
    }

    // What a deploy would do, worked out from the cards that are switched on. The administrator list mirrors
    // requires_admin() in src-tauri/src/safety.rs (for applying a tweak); the confirmation list is CONFIRMATIONS.
    const NEEDS_ADMIN = [1, 4, 5, 7];
    let appIsAdmin = null; // null = not known yet, or the browser demo

    function updatePlan() {
      const card = document.getElementById('plan-card');
      if (!card) return;
      const on = tweaks.filter(t => t.on);
      const admin = on.filter(t => NEEDS_ADMIN.includes(t.id)).length;
      const ask = on.filter(t => Object.prototype.hasOwnProperty.call(CONFIRMATIONS, t.id)).length;
      document.getElementById('plan-count').innerHTML = on.length === 0 ? 'nothing selected' : `<b>${on.length}</b> of ${tweaks.length} selected`;
      const chips = [];
      if (on.length === 0) {
        chips.push('<span><span class="plan-hint-long">Switch on the tweaks you want, then deploy.</span><span class="plan-hint-short">switch on tweaks to deploy</span></span>');
      } else {
        if (admin === 0) {
          chips.push('<span class="plan-chip ok"><span class="long">no administrator needed</span><span class="short">no admin needed</span></span>');
        } else if (appIsAdmin === false) {
          chips.push(`<span class="plan-chip warn" title="Restart the app with Run as administrator, or these will be skipped."><b>${admin}</b>&nbsp;<span class="long">need administrator: this app is not running as one</span><span class="short">admin: not running as one</span></span>`);
        } else {
          chips.push(`<span class="plan-chip ${appIsAdmin ? 'ok' : ''}"><b>${admin}</b>&nbsp;<span class="long">need administrator${appIsAdmin ? ' ✓' : ''}</span><span class="short">admin${appIsAdmin ? ' ✓' : ''}</span></span>`);
        }
        if (ask > 0) chips.push(`<span class="plan-chip warn"><b>${ask}</b>&nbsp;<span class="long">will ask to confirm</span><span class="short">confirm</span></span>`);
      }
      document.getElementById('plan-chips').innerHTML = chips.join('');
      layoutPlan();
    }

    // Beside the title when there is room; one compact line when the card has to drop below the title.
    function layoutPlan() {
      const card = document.getElementById('plan-card'), title = document.getElementById('cpu-name');
      if (!card || !title) return;
      card.classList.toggle('plan-line', card.getBoundingClientRect().top > title.getBoundingClientRect().bottom - 4);
    }

    // CPU load map, "capacity battery". Drawn from measured values only: the overall load with a plain-language
    // verdict, one cell per logical processor, and a RAM meter. Nothing here depends on which tweaks are switched on.
    // (The wave that used to sit in the header was decoration tied to the number of selected cards, so it is gone.)
    const loadColor = (v) => `hsl(${Math.round(78 - 78 * Math.min(Math.max(v, 0), 100) / 100)},100%,${v > 85 ? 56 : 52}%)`;
    const loadVerdict = (avg) => (avg < 25 ? ['Running calm', '#10b981'] : avg < 60 ? ['Working', '#ccff00'] : avg < 85 ? ['Busy', '#ff9900'] : ['Straining', '#f87171']);

    function renderLoadMap(cpuLoad, ramTotalGb, ramPct) {
      const cells = document.getElementById('cap-cells'), total = document.getElementById('cap-total'), verdict = document.getElementById('cap-verdict');
      const load = Array.isArray(cpuLoad) ? cpuLoad : [];
      if (load.length === 0) {
        cells.replaceChildren(Object.assign(document.createElement('span'), { className: 'cap-note', textContent: 'CPU load unavailable' }));
        total.textContent = '--';
        verdict.textContent = 'CPU load unavailable';
      } else {
        if (cells.querySelectorAll('.cap-cell').length !== load.length) {
          cells.replaceChildren(...load.map((_, i) => {
            const c = document.createElement('div');
            c.className = 'cap-cell';
            c.id = 'cap-cell-' + i;
            c.appendChild(document.createElement('em'));
            return c;
          }));
        }
        load.forEach((v, i) => {
          const c = document.getElementById('cap-cell-' + i);
          c.style.setProperty('--f', String(v));
          c.style.setProperty('--c', loadColor(v));
          c.dataset.load = String(v);
          c.classList.toggle('hot', v >= 70);
          c.firstChild.textContent = `T${i} ${v}`;
          c.title = `Logical processor ${i}: ${v}% busy`;
        });
        const avg = load.reduce((a, b) => a + b, 0) / load.length, mx = Math.max(...load), [word, color] = loadVerdict(avg);
        total.textContent = Math.round(avg) + '%';
        const dot = Object.assign(document.createElement('span'), { className: 'cap-dot' });
        dot.style.background = color;
        verdict.replaceChildren(dot, document.createTextNode(`${word} · busiest T${load.indexOf(mx)} ${mx}%`));
      }
      const ram = document.getElementById('cap-ram');
      if (ramTotalGb > 0) {
        ram.innerHTML = `RAM ${ramPct}% of ${Math.round(ramTotalGb)} GB` + Array.from({ length: 16 }, (_, k) => `<s style="${k < ramPct / 100 * 16 ? 'background:' + loadColor(ramPct * 0.8) : ''}"></s>`).join('');
        ram.title = `RAM in use: ${ramPct}%`;
      } else {
        ram.textContent = 'RAM --';
        ram.title = '';
      }
    }

    // --- CLEAN SCI-FI NUKE SAFETY COVER MECHANICS (FLIP HOOD) ---
    let coverOpen = false;

    function toggleSafetyCover() {
      if (coverOpen) closeSafetyCover();
      else openSafetyCover();
    }

    function openSafetyCover() {
      playAudio('pneumatic');
      coverOpen = true;
      const cov = document.getElementById("safety-cover");
      const assembly = document.getElementById("safety-assembly");
      const title = document.getElementById("guard-stencil-title");
      const hint = document.getElementById("guard-action-hint");
      const svg = document.getElementById("guard-lock-svg");

      if (cov) {
        cov.classList.add("open");
        cov.title = "Click to Seal Silo Blast Hatch";
      }
      if (assembly) assembly.classList.add("guard-open-state");

      if (title) title.innerText = "WARHEAD: ARMED";
      if (hint) hint.innerText = "/// TAP TO SEAL ///";
      if (svg) {
        svg.innerHTML = `
          <circle cx="12" cy="12" r="8"/>
          <circle cx="12" cy="12" r="2.5" fill="#ff3355"/>
          <line x1="12" y1="2" x2="12" y2="6"/>
          <line x1="12" y1="18" x2="12" y2="22"/>
          <line x1="2" y1="12" x2="6" y2="12"/>
          <line x1="18" y1="12" x2="22" y2="12"/>
        `;
      }
    }

    function closeSafetyCover() {
      playAudio('vault');
      coverOpen = false;
      const cov = document.getElementById("safety-cover");
      const assembly = document.getElementById("safety-assembly");
      const title = document.getElementById("guard-stencil-title");
      const hint = document.getElementById("guard-action-hint");
      const svg = document.getElementById("guard-lock-svg");

      if (cov) {
        cov.classList.remove("open");
        cov.title = "Click to Arm Silo Blast Hatch";
      }
      if (assembly) assembly.classList.remove("guard-open-state");

      if (title) title.innerText = "SILO: SEALED";
      if (hint) hint.innerText = "/// TAP TO UNLATCH ///";
      if (svg) {
        svg.innerHTML = `
          <circle cx="12" cy="12" r="8"/>
          <circle cx="12" cy="12" r="2.5" fill="#ff9900"/>
          <line x1="12" y1="2" x2="12" y2="6"/>
          <line x1="12" y1="18" x2="12" y2="22"/>
          <line x1="2" y1="12" x2="6" y2="12"/>
          <line x1="18" y1="12" x2="22" y2="12"/>
        `;
      }
    }

    // --- DOCKED CONSOLE DRAWER LOGIC ---
    let drawerExpanded = false;
    function toggleDrawer() {
      playAudio('click');
      drawerExpanded = !drawerExpanded;
      const drawer = document.getElementById("docked-drawer");
      const arrow = document.getElementById("drawer-arrow");
      if (drawerExpanded) {
        drawer.classList.add("expanded");
        arrow.innerText = "▼";
      } else {
        drawer.classList.remove("expanded");
        arrow.innerText = "▲";
      }
    }

    function logToDrawer(text, type = 'info') {
      const logs = document.getElementById("drawer-logs");
      const line = document.createElement("div");
      line.className = `log-line log-${type}`;
      line.innerText = text;
      logs.appendChild(line);
      logs.scrollTop = logs.scrollHeight;
    }

    // Shows one backend result with a marker that matches what really happened.
    function logResult(r) {
      const marks = { ok: ['[✓]', 'success'], nochange: ['[=]', 'info'], partial: ['[!]', 'warn'], skipped: ['[-]', 'warn'], failed: ['[✗]', 'err'] };
      const [mark, type] = marks[r.status] || marks.failed;
      logToDrawer(`[${(performance.now()/1000).toFixed(3)}s] ${mark} ${r.message}`, type);
    }

    // --- DEPLOY TUNING LOGIC WITH TAURI BACKEND INTEGRATION ---
    async function deployTuning() {
      if (!coverOpen) {
        // If cover is still down, tapping button unlatches it first!
        openSafetyCover();
        return;
      }

      if (working) return;
      if (blockedByMode('Deploy')) return;
      working = true;
      try {
        await deployInner();
      } finally {
        working = false;
      }
    }

    async function deployInner() {
      playAudio('click');
      const drawer = document.getElementById("docked-drawer");
      const arrow = document.getElementById("drawer-arrow");
      drawer.classList.add("expanded");
      drawerExpanded = true;
      arrow.innerText = "▼";

      const statusPill = document.getElementById("drawer-status-pill");
      statusPill.innerText = "[DISPATCHING // ACTIVE]";
      statusPill.style.color = "#ff9900";

      const activeTweaks = tweaks.filter(t => t.on);
      logToDrawer(`[${(performance.now()/1000).toFixed(3)}s] // STARTING DISPATCH: ${activeTweaks.length} PROTOCOLS ARMED`, 'info');

      const counts = { ok: 0, nochange: 0, partial: 0, skipped: 0, failed: 0 };
      const inApp = MODE === 'real';
      for (let i = 0; i < activeTweaks.length; i++) {
        const item = activeTweaks[i];
        logToDrawer(`[${(performance.now()/1000).toFixed(3)}s] >> [CH0${item.id}] ${item.title}...`, 'info');

        let r;
        if (inApp) {
          try {
            const ask = CONFIRMATIONS[item.id];
            if (ask && (await backend('is_applied', { id: item.id })) === true) {
              // Nothing to remove or switch off, so there is nothing to ask.
              r = { status: 'nochange', message: `${item.title}: already applied; nothing was changed.` };
            } else if (ask && !confirm(ask.text)) {
              r = { status: 'skipped', message: `${item.title}: you chose not to continue. Nothing was changed.` };
            } else {
              r = await backend('apply_tweak', { id: item.id, confirmation: ask ? ask.word : null });
            }
          } catch (err) {
            r = { status: 'failed', message: String(err) };
          }
        } else {
          // Browser demo: nothing is changed on the visitor's device.
          await new Promise(res => setTimeout(res, 220));
          r = { status: 'ok', message: `${item.title}: simulated (demo only, nothing changed)` };
        }
        counts[r.status] = (counts[r.status] || 0) + 1;
        logResult(r);
      }

      const total = activeTweaks.length;
      const issues = counts.partial + counts.skipped + counts.failed;
      if (issues === 0) {
        playAudio('success');
        statusPill.innerText = inApp ? (counts.ok === 0 && counts.nochange > 0 ? "[NOTHING TO CHANGE]" : "[ALL APPLIED]") : "[DEMO // NOTHING CHANGED]";
        statusPill.style.color = "var(--acid-volt)";
        logToDrawer(inApp
          ? `// DONE: ${counts.ok} applied and checked, ${counts.nochange} already in place (of ${total}).`
          : `// DEMO ONLY: no changes were made. Download the app to apply them.`, 'success');
      } else {
        statusPill.innerText = `[${counts.ok} OK // ${issues} NEED ATTENTION]`;
        statusPill.style.color = "#ff9900";
        logToDrawer(`// DONE WITH PROBLEMS: ${counts.ok} ok, ${counts.partial} partial, ${counts.skipped} skipped, ${counts.failed} failed (of ${total}).`, 'warn');
      }
    }

    // Restore the values the app saved before it changed them
    async function restoreDefaults() {
      if (working || blockedByMode('Restore')) return;
      playAudio('vault');
      if (!confirm("Put back the values this app saved before it changed them?\n\nTweaks the app never changed are skipped. Deleted temporary files and a removed Widgets pack cannot be brought back.")) return;

      const drawer = document.getElementById("docked-drawer");
      const arrow = document.getElementById("drawer-arrow");
      drawer.classList.add("expanded");
      drawerExpanded = true;
      arrow.innerText = "▼";

      logToDrawer(`[${(performance.now()/1000).toFixed(3)}s] // RESTORING THE VALUES SAVED BEFORE THE CHANGES...`, 'info');
      working = true;

      try {
        if (hasApi) {
          const results = await backend('restore_all');
          const ids = [1, 2, 3, 4, 5, 6, 7]; // same order the backend returns them
          results.forEach((r, i) => {
            logResult(r);
            if (r.status === 'ok' || r.status === 'partial') {
              const t = tweaks.find(x => x.id === ids[i]);
              if (t) t.on = false;
            }
          });
          const bad = results.filter(r => r.status !== 'ok').length;
          logToDrawer(bad === 0
            ? '// RESTORE DONE: every saved value was put back.'
            : `// RESTORE DONE: ${results.length - bad} of ${results.length} put back; the others were skipped or need attention (see the lines above).`, bad === 0 ? 'success' : 'warn');
        } else {
          await new Promise(r => setTimeout(r, 400));
          logToDrawer(`// DEMO ONLY: nothing was changed on this device.`, 'success');
          tweaks.forEach(t => t.on = false);
        }
        renderGrid();
        playAudio('success');
      } catch (e) {
        logToDrawer(`[${(performance.now()/1000).toFixed(3)}s] [✗] Restore error: ${e}`, 'err');
      } finally {
        working = false;
      }
    }

    // Tactical Lock-BAY Startup Apps Manager
    async function openStartupModal() {
      playAudio('click');
      const modal = document.getElementById('startup-modal');
      const list = document.getElementById('startup-list-container');
      modal.classList.remove('modal-hidden');
      list.innerHTML = '<div style="color:#6b7280; padding:16px; font-family:var(--font-mono); font-size:11px;">[SCANNING REGISTRY RUN KEYS // HKCU & HKLM...]</div>';

      let apps = [];
      if (MODE === 'broken') {
        list.textContent = '[!] Backend not connected: the startup list cannot be read.';
        list.style.color = '#f87171';
        return;
      }
      try {
        if (hasApi) {
          apps = await backend('get_startup_apps');
        } else {
          // Browser demo only: sample entries, not read from any device.
          apps = [
            { name: "OneDrive", command: "OneDrive.exe /background", location: "HKCU", is_enabled: true },
            { name: "Grammarly", command: "GrammarlyDesktop.exe", location: "HKCU", is_enabled: true },
            { name: "UniGetUI", command: "UniGetUI.exe --daemon", location: "HKCU", is_enabled: true },
            { name: "RiotClient", command: "RiotClientServices.exe", location: "HKCU", is_enabled: false }
          ];
        }
      } catch (e) {
        const msg = document.createElement('div');
        msg.style.cssText = 'color:#f87171; padding:12px; font-family:var(--font-mono);';
        msg.textContent = `[!] Failed to load startup apps: ${e}`;
        list.replaceChildren(msg);
        return;
      }

      list.innerHTML = '';
      if (apps.length === 0) {
        list.innerHTML = '<div style="color:#6b7280; padding:16px; font-family:var(--font-mono); font-size:11px;">No startup apps found in the Run registry keys.</div>';
        updateStartupStats();
        return;
      }
      apps.forEach((app, idx) => {
        const item = document.createElement('div');
        const isEnabled = app.is_enabled;
        item.className = 'startup-card ' + (isEnabled ? 'card-active' : 'card-decoupled');
        item.id = 'su-card-' + idx;
        item.onclick = () => toggleStartupAppItem(app.name, app.location, idx);

        const chNum = (idx + 1).toString().padStart(2, '0');
        const tagText = isEnabled ? 'DAEMON HOOK' : 'DECOUPLED';
        const tagClass = isEnabled ? 'tag-on' : 'tag-off';
        const lockText = isEnabled ? '🔒 ARMED // AUTORUN_HOOK [ACTIVE]' : '🔓 DECOUPLED // ZERO_LATENCY [HALTED]';
        const lockClass = isEnabled ? 'lock-armed' : 'lock-standby';

        item.innerHTML = `
          <div class="tweak-left">
            <div class="tweak-channel-badge" id="su-ch-${idx}">
              <span class="channel-prefix">ST</span>
              <span class="channel-num">${chNum}</span>
            </div>
            <div class="tweak-content-col">
              <div class="tweak-title-row">
                <span class="tweak-title"></span>
                <span class="tag-benefit ${tagClass}" id="su-tag-${idx}">${tagText}</span>
                <span class="su-loc-badge"></span>
              </div>
              <div class="su-cmd-path"></div>
              <div class="hazard-lock-badge ${lockClass}" id="su-lock-${idx}">
                <span>${lockText}</span>
              </div>
            </div>
          </div>
          <div class="switch-bay">
            <div class="switch-track ${isEnabled ? 'sw-track-on' : 'sw-track-off'}" id="su-track-${idx}">
              <div class="switch-thumb ${isEnabled ? 'sw-thumb-on' : 'sw-thumb-off'}" id="su-thumb-${idx}">
                ${isEnabled ? 'ON' : 'OFF'}
              </div>
            </div>
          </div>
        `;
        // Names and commands come from the registry: they are shown as plain text, never as markup.
        item.querySelector('.tweak-title').textContent = String(app.name);
        item.querySelector('.su-loc-badge').textContent = (app.location === 'HKLM' ? 'HKLM' : 'HKCU') + '\\RUN';
        item.querySelector('.su-cmd-path').textContent = String(app.command);
        list.appendChild(item);
      });

      updateStartupStats();
    }

    function setStartupCardState(idx, isNowOn) {
      const card = document.getElementById('su-card-' + idx);
      const track = document.getElementById('su-track-' + idx);
      const thumb = document.getElementById('su-thumb-' + idx);
      const tag = document.getElementById('su-tag-' + idx);
      const lock = document.getElementById('su-lock-' + idx);
      if (!track || !thumb) return;
      if (card) card.className = 'startup-card ' + (isNowOn ? 'card-active' : 'card-decoupled');
      track.className = 'switch-track ' + (isNowOn ? 'sw-track-on' : 'sw-track-off');
      thumb.className = 'switch-thumb ' + (isNowOn ? 'sw-thumb-on' : 'sw-thumb-off');
      thumb.textContent = isNowOn ? 'ON' : 'OFF';
      if (tag) {
        tag.className = 'tag-benefit ' + (isNowOn ? 'tag-on' : 'tag-off');
        tag.textContent = isNowOn ? 'DAEMON HOOK' : 'DECOUPLED';
      }
      if (lock) {
        lock.className = 'hazard-lock-badge ' + (isNowOn ? 'lock-armed' : 'lock-standby');
        const span = document.createElement('span');
        span.textContent = isNowOn ? '🔒 ARMED // AUTORUN_HOOK [ACTIVE]' : '🔓 DECOUPLED // ZERO_LATENCY [HALTED]';
        lock.replaceChildren(span);
      }
      updateStartupStats();
    }

    // The switch only moves after the backend confirms the change.
    async function toggleStartupAppItem(name, location, idx) {
      playAudio('click');
      const track = document.getElementById('su-track-' + idx);
      if (!track || working) return;
      if (blockedByMode('Startup toggle')) return;
      const isNowOn = !track.classList.contains('sw-track-on');
      if (MODE === 'demo') { setStartupCardState(idx, isNowOn); return; } // demo: nothing is changed
      working = true;
      try {
        const r = await backend('toggle_startup_app', { name, location, enable: isNowOn });
        if (r.status === 'ok') {
          setStartupCardState(idx, isNowOn);
        } else {
          logToDrawer(`// STARTUP ${r.status.toUpperCase()}: ${r.message}`, 'warn');
        }
      } catch (e) {
        logToDrawer(`// STARTUP FAILED: ${e}`, 'err');
      } finally {
        working = false;
      }
    }

    function updateStartupStats() {
      const activeCards = document.querySelectorAll('.startup-card.card-active').length;
      const totalCards = document.querySelectorAll('.startup-card').length;
      const decoupledCards = totalCards - activeCards;
      const armedEl = document.getElementById('su-stat-armed');
      const decoupledEl = document.getElementById('su-stat-decoupled');
      const totalEl = document.getElementById('su-stat-total');
      if (armedEl) armedEl.innerText = activeCards;
      if (decoupledEl) decoupledEl.innerText = decoupledCards;
      if (totalEl) totalEl.innerText = totalCards;
    }

    function closeStartupModal() {
      playAudio('click');
      document.getElementById('startup-modal').classList.add('modal-hidden');
    }

    // Dynamic Hardware Detection & Telemetry Sync
    async function initSystemInfo() {
      try {
        if (hasApi) {
          appIsAdmin = !!(await backend('get_elevation'));
          if (!appIsAdmin) {
            document.getElementById('admin-banner').style.display = 'block';
          }
          updatePlan();
          const info = await backend('get_system_info');
          if (info) {
            // #cpu-name also contains the cores and specs spans, so only its first text node is replaced.
            // (Setting innerText wiped those spans, the next line threw, and the model stayed on "READING DEVICE...".)
            if (info.cpu_name) document.getElementById('cpu-name').firstChild.textContent = info.cpu_name + ' ';
            if (info.cores) document.getElementById('cpu-cores').innerText = info.cores;
            if (info.specs) document.getElementById('system-specs').innerText = info.specs;
            if (info.model) document.getElementById('system-model').innerText = info.model;
            layoutPlan(); // the title line just got its real length
          }
        }
      } catch (e) {
        console.warn("Dynamic hardware query fallback:", e);
      }
    }

    // Real-Time Hardware Telemetry Polling (Task 59.4)
    let diskBaselineGb = null;
    // The drive line of the deploy plan: free space, and the rise since the app opened (from any cause).
    function showDrive(freeGb, totalGb) {
      const el = document.getElementById('plan-drive'), fill = document.getElementById('plan-meter');
      if (!el || !fill) return;
      if (!(totalGb > 0)) {
        el.textContent = 'Drive -- free';
        fill.style.width = '0';
        return;
      }
      if (diskBaselineGb === null) diskBaselineGb = freeGb; // free space when the app opened
      const gained = freeGb - diskBaselineGb;
      // The part inside plan-more is dropped when the plan is a single compact line.
      el.innerHTML = `Drive <b>${freeGb.toFixed(1)} GB</b> free<span class="plan-more"> of ${totalGb.toFixed(1)}` + (gained >= 0.1 ? ` · <b>+${gained.toFixed(1)} GB</b> since opened` : '') + '</span>';
      fill.style.width = Math.min(100, Math.max(0, (1 - freeGb / totalGb) * 100)).toFixed(0) + '%';
    }
    async function updateLiveTelemetry() {
      try {
        if (hasApi) {
          const telem = await backend('get_live_telemetry');
          if (telem) {
            renderLoadMap(telem.cpu_load, telem.ram_total_gb, telem.ram_usage_pct);
            showDrive(telem.ssd_free_gb, telem.ssd_total_gb);
            layoutPlan();
          }
        } else if (MODE === 'demo') {
          // Simulation fallback for the browser preview: samples, not measured (the demo bar says so)
          renderLoadMap([12, 48, 30, 76, 22, 9, 55, 31], 16, 51);
          showDrive(116.1, 476.9);
          layoutPlan();
        }
      } catch (e) {
        console.warn("Live telemetry query error:", e);
      }
    }

    // Initialization
    const ACTIONS = { toggleAudio, toggleDrawer, restoreDefaults, toggleAll, openStartupModal, toggleSafetyCover, deployTuning, closeStartupModal };
    document.addEventListener('click', (ev) => {
      const el = ev.target.closest('[data-action]');
      const fn = el && Object.prototype.hasOwnProperty.call(ACTIONS, el.dataset.action) ? ACTIONS[el.dataset.action] : null;
      if (fn) fn();
    });

    window.addEventListener('DOMContentLoaded', () => {
      showMode();
      renderGrid();
      initSystemInfo();
      updateLiveTelemetry();
      setInterval(updateLiveTelemetry, 1500);
      // The deploy plan changes shape when it has to drop below the title, so re-check whenever the header changes size.
      const header = document.querySelector('.app-header');
      if (header && window.ResizeObserver) new ResizeObserver(layoutPlan).observe(header);
      window.addEventListener('resize', layoutPlan);
    });

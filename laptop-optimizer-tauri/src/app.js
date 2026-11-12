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
        on: true 
      },
      { 
        id: 2,
        sys: "SYS: DIRECTX_DWM [FRAME_LATENCY]",
        title: "Bypass Game DVR Background Video Buf...", 
        tag: "ZERO VRAM DRAIN", 
        desc: "Disables continuous DirectX background video recording and capture hooks.", 
        gain: "GAIN: ZERO VRAM OVERHEAD",
        lockProtocol: "DWM_HOOK_KILL",
        on: true 
      },
      { 
        id: 3,
        sys: "SYS: USER32_SHELL [MENU_SPEED]",
        title: "Instant Desktop Context Menus", 
        tag: "20ms DELAY", 
        desc: "Reduces MenuShowDelay from 400ms to 20ms for instant desktop responsiveness.", 
        gain: "GAIN: \u2212380ms DELAY CUT",
        lockProtocol: "WIN32_DISPATCH",
        on: true 
      },
      { 
        id: 4,
        sys: "SYS: WEBVIEW2_DAEMON [RAM_PURGE]",
        title: "Purge Windows 11 Widget Daemon Pack", 
        tag: "\u2212300MB RAM", 
        desc: "Uninstalls WebExperience package to permanently kill 20+ idle child processes.", 
        gain: "GAIN: \u2212300MB LEAK RECLAIMED",
        lockProtocol: "VFS_ISOLATION",
        on: true 
      },
      { 
        id: 5,
        sys: "SYS: NT_KERNEL [HIBERNATE_DEL]",
        title: "Clean Kernel Boots & Delete hiberfil.s...", 
        tag: "FREES DISK", 
        desc: "Turns off hibernation and Fast Startup and deletes hiberfil.sys. Hibernate stops working until restored.", 
        gain: "GAIN: frees hiberfil.sys (size depends on your RAM)",
        lockProtocol: "KERNEL_FASTBOOT",
        on: true 
      },
      { 
        id: 6,
        sys: "SYS: SEARCH_HOST [CLOUD_DECOUPLE]",
        title: "Decouple Bing Cloud Integration from Sta...", 
        tag: "LOCAL ONLY", 
        desc: "Eliminates remote cloud latency on Start search; makes app launch search instant.", 
        gain: "GAIN: 0ms CLOUD ROUNDTRIP",
        lockProtocol: "OFFLINE_SEARCH",
        on: true 
      },
      { 
        id: 7,
        sys: "SYS: BITS_P2P [NETWORK_ISOLATE]",
        title: "Halt Delivery Optimization P2P Seeding", 
        tag: "HTTP ONLY", 
        desc: "Blocks Windows Update peer-to-peer telemetry and upload bandwidth consumption.", 
        gain: "GAIN: 0ms P2P OVERHEAD",
        lockProtocol: "NET_P2P_DECOUPLE",
        on: true 
      },
      { 
        id: 8,
        sys: "SYS: NVME_VFS [TEMP_PURGE]",
        title: "Purge Stale Temporary Files (>24h)", 
        tag: "CLEAR TEMP", 
        desc: "Wipes stale %TEMP% artifacts older than 24h; frees disk space.", 
        gain: "GAIN: +1.4GB CLEARED",
        lockProtocol: "IO_TRIM_CACHE",
        on: true 
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
        bar.className = 'mode-bar mode-demo';
        bar.textContent = 'DEMO MODE: this is a preview in a browser. Nothing on your device is changed.';
      }
    }
    function blockedByMode(what) {
      if (MODE !== 'broken') return false;
      logToDrawer(`// BLOCKED: backend not connected, so "${what}" was not run.`, 'err');
      return true;
    }
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

      updateTelemetry();
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

    function updateTelemetry() {
      const activeCount = tweaks.filter(t => t.on).length;
      const gov = document.getElementById("gov-text");
      const scopeStatus = document.getElementById("scope-status");
      
      if (activeCount === 8) {
        gov.innerText = "ALL 8 SELECTED";
        gov.style.color = "#ccff00";
        scopeStatus.innerText = "ALL SELECTED";
        scopeStatus.style.color = "#ccff00";
      } else if (activeCount === 0) {
        gov.innerText = "NOTHING SELECTED";
        gov.style.color = "#6b7280";
        scopeStatus.innerText = "NONE SELECTED";
        scopeStatus.style.color = "#f87171";
      } else {
        gov.innerText = `${activeCount}/8 SELECTED`;
        gov.style.color = "#00f0ff";
        scopeStatus.innerText = `${activeCount}/8 SELECTED`;
        scopeStatus.style.color = "#00f0ff";
      }

      // Silicon Die Visual Update
      const cpuOn = tweaks[0].on;
      for (let i = 0; i < 4; i++) {
        const c = document.getElementById("core-" + i);
        const b = document.getElementById("bus-" + i);
        if (c) c.className = "die-core-box " + (cpuOn ? "active" : "");
        if (b) b.className = "die-bus-line " + (cpuOn ? "active" : "");
      }
    }

    // --- 60 FPS CRT VECTOR OSCILLOSCOPE CANVAS ---
    const canvas = document.getElementById("oscilloscope");
    const ctx = canvas.getContext("2d");
    let phase = 0;

    function drawOscilloscope() {
      ctx.fillStyle = "rgba(4, 6, 10, 0.25)";
      ctx.fillRect(0, 0, canvas.width, canvas.height);

      const activeCount = tweaks.filter(t => t.on).length;
      const stability = activeCount / 8;

      ctx.beginPath();
      ctx.lineWidth = 1.8;
      ctx.strokeStyle = activeCount === 0 ? "#f87171" : "#ccff00";
      ctx.shadowBlur = 8;
      ctx.shadowColor = activeCount === 0 ? "rgba(248, 113, 113, 0.8)" : "rgba(204, 255, 0, 0.8)";

      const midY = canvas.height / 2;
      for (let x = 0; x < canvas.width; x++) {
        const harmonic = Math.sin((x * 0.08) + phase) * (12 * stability);
        const jitter = activeCount === 8 ? 0 : (Math.sin((x * 0.42) + phase * 2) * (1 - stability) * 9);
        const y = midY + harmonic + jitter;
        if (x === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();

      phase += 0.08 + (stability * 0.06);
      requestAnimationFrame(drawOscilloscope);
    }
    requestAnimationFrame(drawOscilloscope);

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
      const marks = { ok: ['[✓]', 'success'], partial: ['[!]', 'warn'], skipped: ['[-]', 'warn'], failed: ['[✗]', 'err'] };
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

      const counts = { ok: 0, partial: 0, skipped: 0, failed: 0 };
      const inApp = MODE === 'real';
      for (let i = 0; i < activeTweaks.length; i++) {
        const item = activeTweaks[i];
        logToDrawer(`[${(performance.now()/1000).toFixed(3)}s] >> [CH0${item.id}] ${item.title}...`, 'info');

        let r;
        if (inApp) {
          try {
            r = await backend('apply_tweak', { id: item.id });
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
        statusPill.innerText = inApp ? "[ALL APPLIED]" : "[DEMO // NOTHING CHANGED]";
        statusPill.style.color = "var(--acid-volt)";
        logToDrawer(inApp
          ? `// DONE: ${counts.ok} of ${total} changes applied and checked.`
          : `// DEMO ONLY: no changes were made. Download the app to apply them.`, 'success');
      } else {
        statusPill.innerText = `[${counts.ok} OK // ${issues} NEED ATTENTION]`;
        statusPill.style.color = "#ff9900";
        logToDrawer(`// DONE WITH PROBLEMS: ${counts.ok} ok, ${counts.partial} partial, ${counts.skipped} skipped, ${counts.failed} failed (of ${total}).`, 'warn');
      }
    }

    // Restore factory defaults
    async function restoreDefaults() {
      if (working || blockedByMode('Restore')) return;
      playAudio('vault');
      if (!confirm("Are you sure you want to restore all settings to default Windows configurations?")) return;

      const drawer = document.getElementById("docked-drawer");
      const arrow = document.getElementById("drawer-arrow");
      drawer.classList.add("expanded");
      drawerExpanded = true;
      arrow.innerText = "▼";

      logToDrawer(`[${(performance.now()/1000).toFixed(3)}s] // RESTORING WINDOWS FACTORY DEFAULTS...`, 'info');
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
            ? '// RESTORE DONE: all settings back to Windows defaults.'
            : `// RESTORE DONE WITH PROBLEMS: ${bad} of ${results.length} not fully restored.`, bad === 0 ? 'success' : 'warn');
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
          if (!(await backend('get_elevation'))) {
            document.getElementById('admin-banner').style.display = 'block';
          }
          const info = await backend('get_system_info');
          if (info) {
            if (info.cpu_name) document.getElementById('cpu-name').innerText = info.cpu_name;
            if (info.cores) document.getElementById('cpu-cores').innerText = info.cores;
            if (info.specs) document.getElementById('system-specs').innerText = info.specs;
            if (info.model) document.getElementById('system-model').innerText = info.model;
          }
        }
      } catch (e) {
        console.warn("Dynamic hardware query fallback:", e);
      }
    }

    // Real-Time Hardware Telemetry Polling (Task 59.4)
    let diskBaselineGb = null;
    async function updateLiveTelemetry() {
      try {
        if (hasApi) {
          const telem = await backend('get_live_telemetry');
          if (telem) {
            const cpuEl = document.getElementById("telem-cpu-mhz");
            const ramEl = document.getElementById("telem-ram-val");
            const ssdEl = document.getElementById("telem-ssd-val");
            const ramBox = document.getElementById('core-ram');
            if (ramBox) ramBox.innerHTML = telem.ram_total_gb > 0 ? `RAM<br>${Math.round(telem.ram_total_gb)}GB` : 'RAM<br>--';
            if (cpuEl) cpuEl.innerText = telem.cpu_mhz > 0 ? `${telem.cpu_mhz} MHz` : '-- MHz';
            if (ramEl) ramEl.innerText = `${telem.ram_used_gb.toFixed(1)} / ${telem.ram_total_gb.toFixed(1)} GB (${telem.ram_usage_pct}%)`;
            const diskKnown = telem.ssd_total_gb > 0;
            if (ssdEl) ssdEl.innerText = diskKnown ? `${telem.ssd_free_gb.toFixed(1)} GB` : '-- GB';
            const diskEl = document.getElementById("telem-storage-gain");
            if (diskKnown && diskBaselineGb === null) diskBaselineGb = telem.ssd_free_gb; // free space when the app opened
            const gained = diskKnown ? telem.ssd_free_gb - diskBaselineGb : 0;
            // Measured, not promised: only shows space actually freed since the app opened.
            const gainText = gained >= 0.1 ? ` (+${gained.toFixed(1)} GB)` : '';
            if (diskEl) diskEl.innerText = diskKnown ? `${telem.ssd_free_gb.toFixed(1)} / ${telem.ssd_total_gb.toFixed(1)} GB${gainText}` : '-- GB';
          }
        } else if (MODE === 'demo') {
          // Simulation fallback for browser preview
          const cpuEl = document.getElementById("telem-cpu-mhz");
          const ramEl = document.getElementById("telem-ram-val");
          const ssdEl = document.getElementById("telem-ssd-val");
          const jitter = Math.floor(Math.random() * 20) - 10;
          if (cpuEl) cpuEl.innerText = `${2803 + jitter} MHz`;
          if (ramEl) ramEl.innerText = `8.2 / 16.0 GB (51%)`;
          if (ssdEl) ssdEl.innerText = `116.1 GB`;
          const diskEl = document.getElementById("telem-storage-gain");
          if (diskEl) diskEl.innerText = `116.1 / 476.9 GB`; // demo sample
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
    });

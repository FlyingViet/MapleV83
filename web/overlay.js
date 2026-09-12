/*
 * Touch overlay for the MapleStory WASM client.
 *
 * - Plain HTML controls drawn above the game canvas: a joystick and any number of
 *   buttons, each bound to a keyboard key. Presses are injected straight into the
 *   client through the exported `mw_key` hook (falls back to synthetic DOM
 *   keyboard events on builds without it).
 * - Everything is remappable, movable and resizable from the gear menu ("Edit
 *   layout"). Layouts are stored in localStorage; `web/overlay-config.json` holds
 *   the defaults and can be exported/imported as JSON.
 * - Touches on the canvas itself are turned into mouse events so the in-game UI
 *   (windows, NPC dialogs, inventory) stays usable on phones.
 */
(function () {
    'use strict';

    const STORAGE_KEY = 'mw.overlay.layout.v1';
    const PREF_KEY = 'mw.overlay.prefs.v1';
    const REF_WIDTH = 900;

    // KeyboardEvent.code -> GLFW key code (what the client's key callback expects).
    const GLFW = {
        Space: 32, Quote: 39, Comma: 44, Minus: 45, Period: 46, Slash: 47,
        Digit0: 48, Digit1: 49, Digit2: 50, Digit3: 51, Digit4: 52, Digit5: 53, Digit6: 54, Digit7: 55, Digit8: 56, Digit9: 57,
        Semicolon: 59, Equal: 61,
        BracketLeft: 91, Backslash: 92, BracketRight: 93, Backquote: 96,
        Escape: 256, Enter: 257, Tab: 258, Backspace: 259, Insert: 260, Delete: 261,
        ArrowRight: 262, ArrowLeft: 263, ArrowDown: 264, ArrowUp: 265,
        PageUp: 266, PageDown: 267, Home: 268, End: 269, CapsLock: 280, ScrollLock: 281,
        ShiftLeft: 340, ControlLeft: 341, AltLeft: 342, ShiftRight: 344, ControlRight: 345, AltRight: 346
    };
    for (let i = 0; i < 26; i++) {
        GLFW['Key' + String.fromCharCode(65 + i)] = 65 + i;
    }
    for (let i = 1; i <= 12; i++) {
        GLFW['F' + i] = 289 + i;
    }

    // Friendly names for the remap dropdown.
    const KEY_LABELS = {
        AltLeft: 'Alt', ControlLeft: 'Ctrl', ShiftLeft: 'Shift', Space: 'Space', Enter: 'Enter', Escape: 'Esc',
        ArrowUp: 'Up', ArrowDown: 'Down', ArrowLeft: 'Left', ArrowRight: 'Right',
        PageUp: 'Page Up', PageDown: 'Page Down', Home: 'Home', End: 'End', Insert: 'Insert', Delete: 'Delete',
        Backquote: '`', Minus: '-', Equal: '=', BracketLeft: '[', BracketRight: ']', Backslash: '\\',
        Semicolon: ';', Quote: "'", Comma: ',', Period: '.', Slash: '/', Tab: 'Tab', Backspace: 'Backspace',
        ScrollLock: 'Scroll Lock', CapsLock: 'Caps Lock', ControlRight: 'Right Ctrl', ShiftRight: 'Right Shift', AltRight: 'Right Alt'
    };
    function keyLabel(code) {
        if (KEY_LABELS[code]) return KEY_LABELS[code];
        if (/^Key[A-Z]$/.test(code)) return code.slice(3);
        if (/^Digit\d$/.test(code)) return code.slice(5);
        return code;
    }
    const KEY_ORDER = ['AltLeft', 'ControlLeft', 'ShiftLeft', 'Space', 'Enter', 'Escape',
        'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'PageUp', 'PageDown', 'Home', 'End', 'Insert', 'Delete']
        .concat(Array.from({ length: 26 }, (_, i) => 'Key' + String.fromCharCode(65 + i)))
        .concat(Array.from({ length: 10 }, (_, i) => 'Digit' + i))
        .concat(Array.from({ length: 12 }, (_, i) => 'F' + (i + 1)))
        .concat(['Backquote', 'Minus', 'Equal', 'BracketLeft', 'BracketRight', 'Backslash', 'Semicolon', 'Quote', 'Comma', 'Period', 'Slash', 'Tab', 'Backspace', 'ScrollLock']);

    // ---------------------------------------------------------------- key injection

    const held = new Map(); // code -> refcount (several controls may share a key)

    function rawKey(code, down) {
        const glfw = GLFW[code];
        if (glfw === undefined) {
            console.warn('[overlay] unknown key code', code);
            return;
        }
        if (typeof Module !== 'undefined' && typeof Module._mw_key === 'function') {
            Module._mw_key(glfw, down ? 1 : 0);
            return;
        }
        // Fallback: synthetic DOM event (Emscripten's GLFW port listens on window).
        const ev = new KeyboardEvent(down ? 'keydown' : 'keyup', {
            code: code, key: keyLabel(code), keyCode: DOM_KEYCODES[code] || 0, which: DOM_KEYCODES[code] || 0,
            bubbles: true, cancelable: true
        });
        window.dispatchEvent(ev);
    }

    const DOM_KEYCODES = {
        AltLeft: 18, ControlLeft: 17, ShiftLeft: 16, Space: 32, Enter: 13, Escape: 27, Tab: 9, Backspace: 8,
        ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, PageUp: 33, PageDown: 34, End: 35, Home: 36,
        Insert: 45, Delete: 46, Backquote: 192, Minus: 189, Equal: 187, BracketLeft: 219, BracketRight: 221,
        Backslash: 220, Semicolon: 186, Quote: 222, Comma: 188, Period: 190, Slash: 191, ScrollLock: 145, CapsLock: 20
    };
    for (let i = 0; i < 26; i++) DOM_KEYCODES['Key' + String.fromCharCode(65 + i)] = 65 + i;
    for (let i = 0; i < 10; i++) DOM_KEYCODES['Digit' + i] = 48 + i;
    for (let i = 1; i <= 12; i++) DOM_KEYCODES['F' + i] = 111 + i;

    function press(code) {
        const n = (held.get(code) || 0) + 1;
        held.set(code, n);
        if (n === 1) rawKey(code, true);
    }

    function release(code) {
        const n = (held.get(code) || 0) - 1;
        if (n <= 0) {
            held.delete(code);
            rawKey(code, false);
        } else {
            held.set(code, n);
        }
    }

    function releaseAll() {
        for (const code of Array.from(held.keys())) {
            held.delete(code);
            rawKey(code, false);
        }
    }

    function tap(code, ms) {
        press(code);
        setTimeout(() => release(code), ms || 60);
    }

    // ---------------------------------------------------------------- state

    let defaults = null;   // from overlay-config.json
    let layout = null;     // active layout (defaults + user edits)
    let prefs = loadJSON(PREF_KEY) || {};
    let editing = false;
    let root, panel, banner, textInput;
    const ctlEls = new Map(); // id -> element

    function loadJSON(key) {
        try {
            const v = localStorage.getItem(key);
            return v ? JSON.parse(v) : null;
        } catch (e) {
            return null;
        }
    }

    function saveJSON(key, value) {
        try {
            localStorage.setItem(key, JSON.stringify(value));
        } catch (e) { /* private mode etc. */ }
    }

    function clone(o) {
        return JSON.parse(JSON.stringify(o));
    }

    function scale() {
        return Math.min(1.6, Math.max(0.55, window.innerWidth / REF_WIDTH));
    }

    function applyLayout(next) {
        layout = next;
        saveJSON(STORAGE_KEY, layout);
        render();
    }

    // ---------------------------------------------------------------- rendering

    function render() {
        if (!root) return;
        for (const el of ctlEls.values()) el.remove();
        ctlEls.clear();

        if (layout.joystick) {
            root.appendChild(buildJoystick(layout.joystick));
        }
        for (const b of layout.buttons) {
            root.appendChild(buildButton(b));
        }
        positionAll();
    }

    function positionAll() {
        const s = scale();
        for (const [id, el] of ctlEls) {
            const cfg = id === '__joystick' ? layout.joystick : layout.buttons.find(b => b.id === id);
            if (!cfg) continue;
            const px = cfg.size * s;
            el.style.left = cfg.x + '%';
            el.style.top = cfg.y + '%';
            el.style.width = px + 'px';
            el.style.height = px + 'px';
            el.style.opacity = cfg.opacity !== undefined ? cfg.opacity : 0.85;
            el.classList.toggle('mw-hidden', !!cfg.hidden);
            const lbl = el.querySelector('.mw-label');
            if (lbl) lbl.style.fontSize = Math.max(10, px * 0.28) + 'px';
        }
    }

    function buildButton(cfg) {
        const el = document.createElement('div');
        el.className = 'mw-ctl mw-btn';
        el.dataset.id = cfg.id;
        el.style.setProperty('--mw-color', cfg.color || 'rgba(255,255,255,0.25)');
        if (cfg.icon) {
            const img = document.createElement('img');
            img.src = cfg.icon;
            img.alt = cfg.label || cfg.id;
            el.appendChild(img);
        } else {
            const span = document.createElement('span');
            span.className = 'mw-label';
            span.textContent = cfg.label || keyLabel(cfg.key);
            el.appendChild(span);
        }
        attachButtonInput(el, cfg);
        attachEditHandles(el, cfg.id);
        ctlEls.set(cfg.id, el);
        return el;
    }

    function attachButtonInput(el, cfg) {
        let activePointer = null;
        el.addEventListener('pointerdown', (e) => {
            if (editing) return;
            e.preventDefault();
            if (activePointer !== null) return;
            activePointer = e.pointerId;
            el.setPointerCapture(e.pointerId);
            el.classList.add('mw-down');
            press(cfg.key);
        });
        const up = (e) => {
            if (activePointer !== e.pointerId) return;
            activePointer = null;
            el.classList.remove('mw-down');
            release(cfg.key);
        };
        el.addEventListener('pointerup', up);
        el.addEventListener('pointercancel', up);
        el.addEventListener('lostpointercapture', up);
        el.addEventListener('contextmenu', (e) => e.preventDefault());
    }

    function buildJoystick(cfg) {
        const el = document.createElement('div');
        el.className = 'mw-ctl mw-stick';
        el.dataset.id = '__joystick';
        el.innerHTML = '<span class="mw-arrow up">▲</span><span class="mw-arrow down">▼</span>'
            + '<span class="mw-arrow left">◀</span><span class="mw-arrow right">▶</span><div class="mw-knob"></div>';
        attachJoystickInput(el, cfg);
        attachEditHandles(el, '__joystick');
        ctlEls.set('__joystick', el);
        return el;
    }

    function attachJoystickInput(el, cfg) {
        const knob = el.querySelector('.mw-knob');
        let pointer = null;
        const dirs = { left: false, right: false, up: false, down: false };

        function setDir(name, on) {
            if (dirs[name] === on) return;
            dirs[name] = on;
            const code = { left: 'ArrowLeft', right: 'ArrowRight', up: 'ArrowUp', down: 'ArrowDown' }[name];
            if (on) press(code); else release(code);
            // Pushing up doubles as jump so ropes, portals and plain jumps all work
            // from the stick. Re-pressed each time the stick re-enters the up zone.
            if (name === 'up' && layout.joystick.upAlsoJumps !== false) {
                const jk = layout.joystick.jumpKey || 'AltLeft';
                if (on) press(jk); else release(jk);
            }
        }

        function update(e) {
            const r = el.getBoundingClientRect();
            const cx = r.left + r.width / 2, cy = r.top + r.height / 2;
            const radius = r.width / 2;
            let dx = e.clientX - cx, dy = e.clientY - cy;
            const dist = Math.hypot(dx, dy);
            if (dist > radius) {
                dx *= radius / dist;
                dy *= radius / dist;
            }
            knob.style.transform = `translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px))`;
            const dz = (layout.joystick.deadzone !== undefined ? layout.joystick.deadzone : 0.28) * radius;
            const nx = dx / radius, ny = dy / radius;
            // Horizontal and vertical zones overlap so diagonals hold both.
            setDir('left', dx < -dz && Math.abs(nx) >= Math.abs(ny) * 0.5);
            setDir('right', dx > dz && Math.abs(nx) >= Math.abs(ny) * 0.5);
            setDir('up', dy < -dz && Math.abs(ny) >= Math.abs(nx) * 0.6);
            setDir('down', dy > dz && Math.abs(ny) >= Math.abs(nx) * 0.6);
        }

        function reset() {
            pointer = null;
            knob.style.transform = 'translate(-50%, -50%)';
            for (const d of Object.keys(dirs)) setDir(d, false);
        }

        el.addEventListener('pointerdown', (e) => {
            if (editing) return;
            e.preventDefault();
            if (pointer !== null) return;
            pointer = e.pointerId;
            el.setPointerCapture(e.pointerId);
            update(e);
        });
        el.addEventListener('pointermove', (e) => {
            if (pointer !== e.pointerId) return;
            update(e);
        });
        const end = (e) => {
            if (pointer !== e.pointerId) return;
            reset();
        };
        el.addEventListener('pointerup', end);
        el.addEventListener('pointercancel', end);
        el.addEventListener('lostpointercapture', end);
        el.addEventListener('contextmenu', (e) => e.preventDefault());
    }

    // ---------------------------------------------------------------- edit mode

    function cfgOf(id) {
        return id === '__joystick' ? layout.joystick : layout.buttons.find(b => b.id === id);
    }

    function attachEditHandles(el, id) {
        const handle = document.createElement('div');
        handle.className = 'mw-resize';
        el.appendChild(handle);

        let drag = null;
        el.addEventListener('pointerdown', (e) => {
            if (!editing || e.target === handle) return;
            e.preventDefault();
            e.stopPropagation();
            const cfg = cfgOf(id);
            drag = { id: e.pointerId, sx: e.clientX, sy: e.clientY, ox: cfg.x, oy: cfg.y, moved: false };
            el.setPointerCapture(e.pointerId);
        }, true);
        el.addEventListener('pointermove', (e) => {
            if (!drag || drag.id !== e.pointerId) return;
            const cfg = cfgOf(id);
            const dx = e.clientX - drag.sx, dy = e.clientY - drag.sy;
            if (Math.hypot(dx, dy) > 6) drag.moved = true;
            if (drag.moved) {
                cfg.x = clamp(drag.ox + dx / window.innerWidth * 100, 2, 98);
                cfg.y = clamp(drag.oy + dy / window.innerHeight * 100, 2, 98);
                positionAll();
            }
        });
        const endDrag = (e) => {
            if (!drag || drag.id !== e.pointerId) return;
            const moved = drag.moved;
            drag = null;
            saveJSON(STORAGE_KEY, layout);
            if (!moved) openControlPanel(id);
        };
        el.addEventListener('pointerup', endDrag);
        el.addEventListener('pointercancel', endDrag);

        let resize = null;
        handle.addEventListener('pointerdown', (e) => {
            if (!editing) return;
            e.preventDefault();
            e.stopPropagation();
            const cfg = cfgOf(id);
            resize = { id: e.pointerId, sx: e.clientX, sy: e.clientY, os: cfg.size };
            handle.setPointerCapture(e.pointerId);
        });
        handle.addEventListener('pointermove', (e) => {
            if (!resize || resize.id !== e.pointerId) return;
            const cfg = cfgOf(id);
            const d = ((e.clientX - resize.sx) + (e.clientY - resize.sy)) / 2;
            cfg.size = clamp(Math.round(resize.os + d / scale()), 28, 320);
            positionAll();
        });
        const endResize = (e) => {
            if (!resize || resize.id !== e.pointerId) return;
            resize = null;
            saveJSON(STORAGE_KEY, layout);
        };
        handle.addEventListener('pointerup', endResize);
        handle.addEventListener('pointercancel', endResize);
    }

    function clamp(v, lo, hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    function setEditing(on) {
        editing = on;
        releaseAll();
        root.classList.toggle('mw-editing', on);
        const gear = root.querySelector('[data-sys="edit"]');
        if (gear) gear.classList.toggle('mw-active', on);
        // Hidden controls become visible (faded) while editing so they can be re-enabled.
        for (const el of ctlEls.values()) el.classList.toggle('mw-editing-visible', on);
        if (!on) closePanel();
    }

    // ---------------------------------------------------------------- panel UI

    function h(tag, attrs, children) {
        const el = document.createElement(tag);
        if (attrs) for (const [k, v] of Object.entries(attrs)) {
            if (k === 'text') el.textContent = v;
            else if (k === 'html') el.innerHTML = v;
            else if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
            else el.setAttribute(k, v);
        }
        (children || []).forEach(c => el.appendChild(c));
        return el;
    }

    function closePanel() {
        panel.classList.remove('mw-open');
        panel.innerHTML = '';
    }

    function keySelect(current, onChange) {
        const sel = h('select');
        for (const code of KEY_ORDER) {
            const opt = h('option', { value: code, text: keyLabel(code) });
            if (code === current) opt.selected = true;
            sel.appendChild(opt);
        }
        sel.addEventListener('change', () => onChange(sel.value));
        return sel;
    }

    function openControlPanel(id) {
        const cfg = cfgOf(id);
        const isStick = id === '__joystick';
        panel.innerHTML = '';
        panel.appendChild(h('h3', { text: isStick ? 'Joystick' : ('Button: ' + (cfg.label || cfg.id)) }));

        if (!isStick) {
            panel.appendChild(h('label', {}, [h('span', { text: 'Label' }), h('input', {
                type: 'text', value: cfg.label || '', oninput: (e) => { cfg.label = e.target.value; render(); }
            })]));
            panel.appendChild(h('label', {}, [h('span', { text: 'Key' }), keySelect(cfg.key, (v) => { cfg.key = v; render(); })]));
            const capture = h('button', { text: 'Press a key…', onclick: () => {
                capture.textContent = 'Listening… press any key';
                const once = (e) => {
                    e.preventDefault();
                    if (GLFW[e.code] !== undefined) { cfg.key = e.code; render(); openControlPanel(id); }
                    window.removeEventListener('keydown', once, true);
                };
                window.addEventListener('keydown', once, true);
            } });
            panel.appendChild(h('p', { class: 'mw-hint', text: 'On a PC you can also capture a physical key:' }));
            panel.appendChild(capture);

            const colorIn = h('input', { type: 'color', class: 'mw-swatch', value: cfg.color || '#ffffff' });
            colorIn.addEventListener('input', () => { cfg.color = colorIn.value; render(); });
            panel.appendChild(h('label', {}, [h('span', { text: 'Colour' }), colorIn]));

            const iconIn = h('input', { type: 'file', accept: 'image/*' });
            iconIn.addEventListener('change', () => {
                const f = iconIn.files && iconIn.files[0];
                if (!f) return;
                const rd = new FileReader();
                rd.onload = () => { cfg.icon = rd.result; render(); };
                rd.readAsDataURL(f);
            });
            panel.appendChild(h('label', {}, [h('span', { text: 'Icon' }), iconIn]));
            if (cfg.icon) {
                panel.appendChild(h('button', { text: 'Remove icon', onclick: () => { delete cfg.icon; render(); openControlPanel(id); } }));
            }
        } else {
            panel.appendChild(h('label', {}, [h('span', { text: 'Up also jumps' }), h('input', {
                type: 'checkbox', onchange: (e) => { cfg.upAlsoJumps = e.target.checked; }
            })]));
            panel.querySelector('input[type=checkbox]').checked = cfg.upAlsoJumps !== false;
            panel.appendChild(h('label', {}, [h('span', { text: 'Jump key' }), keySelect(cfg.jumpKey || 'AltLeft', (v) => { cfg.jumpKey = v; })]));
            const dz = h('input', { type: 'range', min: '0.1', max: '0.6', step: '0.02', value: cfg.deadzone !== undefined ? cfg.deadzone : 0.28 });
            dz.addEventListener('input', () => { cfg.deadzone = parseFloat(dz.value); });
            panel.appendChild(h('label', {}, [h('span', { text: 'Dead zone' }), dz]));
        }

        const size = h('input', { type: 'range', min: '28', max: '320', value: cfg.size });
        size.addEventListener('input', () => { cfg.size = parseInt(size.value, 10); positionAll(); });
        panel.appendChild(h('label', {}, [h('span', { text: 'Size' }), size]));

        const op = h('input', { type: 'range', min: '0.15', max: '1', step: '0.05', value: cfg.opacity !== undefined ? cfg.opacity : 0.85 });
        op.addEventListener('input', () => { cfg.opacity = parseFloat(op.value); positionAll(); });
        panel.appendChild(h('label', {}, [h('span', { text: 'Opacity' }), op]));

        const row = h('div', { class: 'mw-row' });
        row.appendChild(h('button', { class: 'mw-primary', text: 'Done', onclick: () => { saveJSON(STORAGE_KEY, layout); closePanel(); } }));
        row.appendChild(h('button', { text: cfg.hidden ? 'Show' : 'Hide', onclick: () => { cfg.hidden = !cfg.hidden; positionAll(); openControlPanel(id); } }));
        if (!isStick) {
            row.appendChild(h('button', { class: 'mw-danger', text: 'Delete', onclick: () => {
                layout.buttons = layout.buttons.filter(b => b.id !== id);
                applyLayout(layout);
                closePanel();
            } }));
        }
        panel.appendChild(row);
        panel.classList.add('mw-open');
    }

    function openMainPanel() {
        panel.innerHTML = '';
        panel.appendChild(h('h3', { text: 'Controls' }));
        panel.appendChild(h('p', { class: 'mw-hint', text: 'Edit layout: drag controls to move, drag the blue corner to resize, tap a control to remap it.' }));
        const row = h('div', { class: 'mw-row' });
        row.appendChild(h('button', { class: 'mw-primary', text: editing ? 'Finish editing' : 'Edit layout', onclick: () => { setEditing(!editing); closePanel(); } }));
        row.appendChild(h('button', { text: 'Add button', onclick: () => {
            const id = 'btn' + Date.now().toString(36);
            layout.buttons.push({ id, label: 'NEW', key: 'Space', x: 50, y: 50, size: 56, color: '#ffffff' });
            applyLayout(layout);
            if (!editing) setEditing(true);
            closePanel();
            openControlPanel(id);
        } }));
        row.appendChild(h('button', { text: prefs.hideOverlay ? 'Show controls' : 'Hide controls', onclick: () => {
            prefs.hideOverlay = !prefs.hideOverlay;
            saveJSON(PREF_KEY, prefs);
            closePanel();
            applyVisibility();
        } }));
        row.appendChild(h('button', { text: 'Reset to defaults', onclick: () => {
            if (confirm('Reset the layout to the defaults?')) {
                applyLayout(clone(defaults));
                closePanel();
            }
        } }));
        panel.appendChild(row);

        panel.appendChild(h('h3', { text: 'Layout JSON', style: 'margin-top:14px' }));
        const ta = h('textarea');
        ta.value = JSON.stringify(layout, null, 2);
        panel.appendChild(ta);
        const row2 = h('div', { class: 'mw-row' });
        row2.appendChild(h('button', { text: 'Import from text', onclick: () => {
            try {
                const next = JSON.parse(ta.value);
                if (!next.buttons) throw new Error('missing buttons');
                applyLayout(next);
                closePanel();
            } catch (e) {
                alert('Invalid layout JSON: ' + e.message);
            }
        } }));
        row2.appendChild(h('button', { text: 'Copy', onclick: () => { ta.select(); document.execCommand('copy'); } }));
        row2.appendChild(h('button', { text: 'Close', onclick: closePanel }));
        panel.appendChild(row2);
        panel.classList.add('mw-open');
    }

    // ---------------------------------------------------------------- system bar

    function buildSystemBar() {
        const bar = h('div', { class: 'mw-sys' });
        bar.appendChild(h('button', { title: 'Keyboard', text: '⌨', 'data-sys': 'kbd', onclick: toggleKeyboard }));
        bar.appendChild(h('button', { title: 'Fullscreen', text: '⛶', 'data-sys': 'fs', onclick: toggleFullscreen }));
        bar.appendChild(h('button', { title: 'Controls', text: '⚙', 'data-sys': 'edit', onclick: () => {
            if (panel.classList.contains('mw-open')) closePanel(); else openMainPanel();
        } }));
        return bar;
    }

    function toggleFullscreen() {
        const doc = document;
        if (!doc.fullscreenElement) {
            const el = doc.documentElement;
            const req = el.requestFullscreen || el.webkitRequestFullscreen;
            if (req) {
                Promise.resolve(req.call(el)).then(() => {
                    if (screen.orientation && screen.orientation.lock) {
                        screen.orientation.lock('landscape').catch(() => { });
                    }
                }).catch(() => { });
            }
        } else {
            (doc.exitFullscreen || doc.webkitExitFullscreen).call(doc);
        }
    }

    // On-screen keyboard: a hidden input receives the phone's keyboard and we
    // replay each character as key presses (with Shift for capitals/symbols).
    const SHIFTED = { '~': 'Backquote', '!': 'Digit1', '@': 'Digit2', '#': 'Digit3', '$': 'Digit4', '%': 'Digit5', '^': 'Digit6',
        '&': 'Digit7', '*': 'Digit8', '(': 'Digit9', ')': 'Digit0', '_': 'Minus', '+': 'Equal', '{': 'BracketLeft', '}': 'BracketRight',
        '|': 'Backslash', ':': 'Semicolon', '"': 'Quote', '<': 'Comma', '>': 'Period', '?': 'Slash' };
    const PLAIN = { '`': 'Backquote', '-': 'Minus', '=': 'Equal', '[': 'BracketLeft', ']': 'BracketRight', '\\': 'Backslash',
        ';': 'Semicolon', "'": 'Quote', ',': 'Comma', '.': 'Period', '/': 'Slash', ' ': 'Space' };

    function typeChar(ch) {
        let code = null, shift = false;
        if (/^[a-z]$/.test(ch)) code = 'Key' + ch.toUpperCase();
        else if (/^[A-Z]$/.test(ch)) { code = 'Key' + ch; shift = true; }
        else if (/^[0-9]$/.test(ch)) code = 'Digit' + ch;
        else if (PLAIN[ch]) code = PLAIN[ch];
        else if (SHIFTED[ch]) { code = SHIFTED[ch]; shift = true; }
        if (!code) return;
        if (shift) rawKey('ShiftLeft', true);
        rawKey(code, true);
        rawKey(code, false);
        if (shift) rawKey('ShiftLeft', false);
    }

    function toggleKeyboard() {
        const btn = root.querySelector('[data-sys="kbd"]');
        if (document.activeElement === textInput) {
            textInput.blur();
            btn.classList.remove('mw-active');
        } else {
            textInput.value = '';
            textInput.focus();
            btn.classList.add('mw-active');
        }
    }

    function buildTextInput() {
        textInput = h('input', { id: 'mw-textin', type: 'text', autocomplete: 'off', autocapitalize: 'off', autocorrect: 'off', spellcheck: 'false' });
        textInput.addEventListener('beforeinput', (e) => {
            if (e.inputType === 'insertText' && e.data) {
                for (const ch of e.data) typeChar(ch);
            } else if (e.inputType === 'deleteContentBackward') {
                tap('Backspace');
            } else if (e.inputType === 'insertLineBreak' || e.inputType === 'insertParagraph') {
                tap('Enter');
            }
            e.preventDefault();
        });
        textInput.addEventListener('keydown', (e) => {
            // Physical keyboards attached to a phone/tablet: forward control keys.
            if (e.key === 'Enter') { tap('Enter'); e.preventDefault(); }
            else if (e.key === 'Backspace') { tap('Backspace'); e.preventDefault(); }
            else if (e.key === 'Escape') { tap('Escape'); textInput.blur(); e.preventDefault(); }
            else if (e.key === 'Tab') { tap('Tab'); e.preventDefault(); }
        });
        textInput.addEventListener('blur', () => {
            const btn = root.querySelector('[data-sys="kbd"]');
            if (btn) btn.classList.remove('mw-active');
        });
        return textInput;
    }

    // ---------------------------------------------------------------- canvas touch → mouse

    function installCanvasTouch(canvas) {
        let touchPointer = null;
        function synth(type, e, buttons) {
            canvas.dispatchEvent(new MouseEvent(type, {
                clientX: e.clientX, clientY: e.clientY, screenX: e.screenX, screenY: e.screenY,
                button: 0, buttons: buttons, bubbles: true, cancelable: true
            }));
        }
        canvas.addEventListener('pointerdown', (e) => {
            if (e.pointerType !== 'touch' || touchPointer !== null) return;
            touchPointer = e.pointerId;
            synth('mousemove', e, 0);
            synth('mousedown', e, 1);
        });
        canvas.addEventListener('pointermove', (e) => {
            if (e.pointerType !== 'touch' || e.pointerId !== touchPointer) return;
            synth('mousemove', e, 1);
        });
        const end = (e) => {
            if (e.pointerType !== 'touch' || e.pointerId !== touchPointer) return;
            touchPointer = null;
            synth('mouseup', e, 0);
        };
        canvas.addEventListener('pointerup', end);
        canvas.addEventListener('pointercancel', end);
        // Stop the browser's own touch→mouse emulation (it would double-click).
        canvas.addEventListener('touchstart', (e) => e.preventDefault(), { passive: false });
        canvas.addEventListener('touchend', (e) => e.preventDefault(), { passive: false });
    }

    // ---------------------------------------------------------------- boot

    function applyVisibility() {
        const coarse = window.matchMedia && window.matchMedia('(pointer: coarse)').matches;
        const auto = !(defaults.system && defaults.system.showOnCoarsePointerOnly) || coarse;
        const show = prefs.hideOverlay !== undefined ? !prefs.hideOverlay : auto;
        // The system bar (gear) is always available so desktop users can enable the overlay.
        for (const el of ctlEls.values()) el.style.display = show ? '' : 'none';
        root.hidden = false;
    }

    async function boot() {
        root = document.getElementById('mw-overlay');
        if (!root) return;
        try {
            const res = await fetch('web/overlay-config.json', { cache: 'no-store' });
            defaults = await res.json();
        } catch (e) {
            console.warn('[overlay] no overlay-config.json, using a minimal default', e);
            defaults = { joystick: { x: 14, y: 70, size: 150, upAlsoJumps: true, jumpKey: 'AltLeft' }, buttons: [
                { id: 'jump', label: 'JUMP', key: 'AltLeft', x: 90, y: 80, size: 72 },
                { id: 'attack', label: 'ATK', key: 'ControlLeft', x: 78, y: 86, size: 80 }] };
        }
        layout = loadJSON(STORAGE_KEY) || clone(defaults);
        if (!layout.buttons) layout = clone(defaults);

        banner = h('div', { class: 'mw-banner', text: 'Editing layout — drag to move, blue corner to resize, tap to remap. ⚙ to finish.' });
        panel = h('div', { class: 'mw-panel' });
        root.appendChild(banner);
        root.appendChild(panel);
        root.appendChild(buildSystemBar());
        document.body.appendChild(buildTextInput());
        render();
        applyVisibility();
        installCanvasTouch(document.getElementById('canvas'));

        window.addEventListener('blur', releaseAll);
        document.addEventListener('visibilitychange', () => { if (document.hidden) releaseAll(); });
    }

    window.MapleOverlay = {
        onResize: () => { if (layout) positionAll(); },
        onRuntimeReady: () => { console.log('[overlay] key injection', typeof Module._mw_key === 'function' ? 'native' : 'DOM fallback'); },
        press, release, tap, layout: () => layout
    };

    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
    else boot();
})();

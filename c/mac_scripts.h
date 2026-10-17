/* The two macOS helpers (JavaScript for Automation, run with osascript -l JavaScript; see win.c).
 * OSA_WATCH prints the clawd-room windows and split panes of iTerm2 in the PowerShell watcher's format, then "--", about
 * four times a second: title|l|t|r|b|al|at|ar|ab|dpi|bar, in points (dpi 96 = one unit per point), top-left origin.
 * iTerm2 does not tell where a pane is or where the text starts, so the terminal area is worked out: window bounds minus
 * the title bar (none in full screen), the tab bar when there are several tabs, and iTerm2's margins. Panes are split
 * by their columns (side by side) or rows (stacked); other layouts report the whole window for each pane.
 * OSA_TOOL does one command: resize|title|w|h; screen|title|wpercent|hpercent (the window to that part of the screen's usable area,
 * at least MIN_COLS x MIN_ROWS cells; at 50 x 50 it is a macOS quarter tile, snapped into the quarter its centre is in,
 * else moved only as far as needed to stay on screen); or zoomfg|title|step and
 * resetfg|title|0 (font keys, only when that window is the focused one). */
#ifndef MAC_SCRIPTS_H
#define MAC_SCRIPTS_H

static const char OSA_WATCH[] =
    "ObjC.import('Foundation'); ObjC.import('AppKit'); ObjC.import('unistd');\n"
    "const TITLE_H = 28, TAB_H = 25, CELL_RATIO = 2.05;\n"
    "function run(argv) {\n"
    "  const prefix = argv[0], parent = $.getppid();\n"
    "  const it = Application('iTerm2');\n"
    "  const d = $.NSUserDefaults.alloc.initWithSuiteName('com.googlecode.iterm2');\n"
    "  const num = (k, v) => d.objectForKey(k).isNil() ? v : d.integerForKey(k);\n"
    "  const mx = num('TerminalMargin', 5), my = num('TerminalVMargin', 2);\n"
    "  const out = $.NSFileHandle.fileHandleWithStandardOutput;\n"
    "  for (;;) {\n"
    "    if ($.getppid() !== parent) return; /* the room is gone */\n"
    "    let lines = [];\n"
    "    try {\n"
    "      const scr = $.NSScreen.mainScreen.frame.size;\n"
    "      for (const w of it.windows()) {\n"
    "        const ss = w.currentTab().sessions();\n"
    "        const info = ss.map(s => ({t: String(s.variable({named: 'terminalWindowName'}) || ''), c: s.columns(), r: s.rows()}));\n"
    "        if (!info.some(i => i.t.startsWith(prefix))) continue;\n"
    "        const b = w.bounds(), L = b.x, T = b.y, R = b.x + b.width, B = b.y + b.height;\n"
    "        const full = b.y <= 0 && b.width >= scr.width && b.height >= scr.height;\n"
    "        const top = T + (full ? 0 : TITLE_H) + (w.tabs().length > 1 ? TAB_H : 0);\n"
    "        const al = L + mx, ar = R - mx, at = top + my, ab = B - my, aw = ar - al, ah = ab - at;\n"
    "        const totC = info.reduce((a, i) => a + i.c, 0), totR = info.reduce((a, i) => a + i.r, 0);\n"
    "        const rowsSame = info.every(i => Math.abs(i.r - info[0].r) <= 1), colsSame = info.every(i => Math.abs(i.c - info[0].c) <= 1);\n"
    "        /* both can be true (two equal panes): pick the one whose cells look like cells */\n"
    "        const sideErr = Math.abs((ah / info[0].r) / (aw / totC) - CELL_RATIO), stackErr = Math.abs((ah / totR) / (aw / info[0].c) - CELL_RATIO);\n"
    "        const side = info.length > 1 && rowsSame && (!colsSame || sideErr <= stackErr), stack = info.length > 1 && colsSame && !side;\n"
    "        let x = al, y = at;\n"
    "        for (const i of info) {\n"
    "          let a = [al, at, ar, ab], r = [L, T, R, B];\n"
    "          if (side) { const pw = aw * i.c / totC; a = [x, at, x + pw, ab]; r = a; x += pw; }\n"
    "          else if (stack) { const ph = ah * i.r / totR; a = [al, y, ar, y + ph]; r = a; y += ph; }\n"
    "          if (i.t.startsWith(prefix)) lines.push([i.t].concat(r.concat(a).map(Math.round), [96, 0]).join('|'));\n"
    "        }\n"
    "      }\n"
    "    } catch (e) {}\n"
    "    lines.push('--');\n"
    "    out.writeData($(lines.join('\\n') + '\\n').dataUsingEncoding($.NSUTF8StringEncoding));\n"
    "    delay(0.25);\n"
    "  }\n"
    "}\n";

static const char OSA_TOOL[] =
    "ObjC.import('AppKit');\n"
    "const MIN_COLS = 44, MIN_ROWS = 20, TITLE_H = 28;\n"
    "function run(argv) {\n"
    "  const cmd = argv[0], title = argv[1], n = parseInt(argv[2] || '0', 10), m = parseInt(argv[3] || '0', 10);\n"
    "  const it = Application('iTerm2');\n"
    "  const named = s => String(s.variable({named: 'terminalWindowName'}) || '') === title;\n"
    "  if (cmd === 'resize') {\n"
    "    for (const w of it.windows()) if (w.currentTab().sessions().some(named)) { const b = w.bounds(); w.bounds = {x: b.x, y: b.y, width: n, height: m}; return; }\n"
    "    return;\n"
    "  }\n"
    "  if (cmd === 'screen') {\n"
    "    for (const w of it.windows()) {\n"
    "      const s = w.currentTab().sessions().find(named);\n"
    "      if (!s) continue;\n"
    "      const scr = $.NSScreen.mainScreen, f = scr.frame, v = scr.visibleFrame; /* Cocoa: origin bottom-left */\n"
    "      const vx = v.origin.x, vy = f.size.height - v.origin.y - v.size.height, vw = v.size.width, vh = v.size.height;\n"
    "      const b = w.bounds(), cw = (b.width - 10) / s.columns(), ch = (b.height - TITLE_H - 4) / s.rows();\n"
    "      let ww = Math.max(vw * n / 100, MIN_COLS * cw + 10), wh = Math.max(vh * (m || n) / 100, MIN_ROWS * ch + TITLE_H + 4);\n"
    "      ww = Math.min(ww, vw); wh = Math.min(wh, vh);\n"
    "      let x = Math.min(Math.max(b.x, vx), vx + vw - ww), y = Math.min(Math.max(b.y, vy), vy + vh - wh);\n"
    "      if (n === 50 && m === 50) { /* a quarter tile: the quarter the window's centre is in */\n"
    "        x = b.x + b.width / 2 < vx + vw / 2 ? vx : vx + vw - ww; y = b.y + b.height / 2 < vy + vh / 2 ? vy : vy + vh - wh;\n"
    "      }\n"
    "      w.bounds = {x: Math.round(x), y: Math.round(y), width: Math.round(ww), height: Math.round(wh)};\n"
    "      return;\n"
    "    }\n"
    "    return;\n"
    "  }\n"
    "  /* font keys: only into our own window, and only when it already has the focus */\n"
    "  if (!it.frontmost() || !it.windows().length || !named(it.currentWindow().currentSession())) return;\n"
    "  const se = Application('System Events');\n"
    "  if (cmd === 'resetfg') se.keystroke('0', {using: 'command down'});\n"
    "  else if (cmd === 'zoomfg') for (let k = 0; k < Math.abs(n); k++) { se.keystroke(n > 0 ? '=' : '-', {using: 'command down'}); delay(0.05); }\n"
    "}\n";
#endif

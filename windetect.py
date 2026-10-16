#!/usr/bin/env python3
"""Where are the clawd-room windows on the Windows desktop? (WSL only, standard library only)

A room sets its terminal title to "clawd-room <room id>". This module starts one small
powershell.exe that lists the visible top-level windows whose title starts with "clawd-room " and
prints their on-screen rectangles about twice a second. Two rooms are "close enough" when their
windows are side by side with a small gap (or a small overlap) and overlap vertically.

It only reads window titles that start with our prefix and their rectangles. It does not read
other windows, screens or keystrokes. powershell.exe dies with the room (and is killed on exit).

Try it:   python3 windetect.py        (lists clawd-room windows; use --prefix to look for others)
"""
import argparse
import base64
import math
import shutil
import subprocess
import threading
import time

PREFIX = "clawd-room "
NEAR_PX = 30  # windows with a gap smaller than this (screen pixels) are attached
MAX_OVERLAP_PX = 30  # windows overlap a little when they touch (their frames have invisible borders)
HYSTERESIS_PX = 15  # an attached pair stays attached until it is this much further apart

# One compile of a tiny C# helper, then a loop printing "title|left|top|right|bottom" lines and "--".
_PS = r'''
param([string]$prefix)
Add-Type -TypeDefinition @"
using System; using System.Text; using System.Runtime.InteropServices;
public class CW {
  public delegate bool Proc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
  // windows seen with our title prefix, by handle: in focus mode the terminal changes the window title,
  // so after the first sight we follow the handle, not the title
  static System.Collections.Generic.Dictionary<IntPtr, string> known = new System.Collections.Generic.Dictionary<IntPtr, string>();
  [DllImport("user32.dll")] static extern bool EnumWindows(Proc p, IntPtr l);
  [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
  static CW() { SetProcessDPIAware(); }  // real pixels, not scaled ones
  [DllImport("user32.dll")] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr h, Proc p, IntPtr l);
  [DllImport("user32.dll")] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static string List(string prefix) {
    var sb = new StringBuilder();
    EnumWindows((h, l) => {
      var t = new StringBuilder(256); GetWindowText(h, t, 256);
      if (t.ToString().StartsWith(prefix)) known[h] = t.ToString();
      return true;
    }, IntPtr.Zero);
    foreach (var h in new System.Collections.Generic.List<IntPtr>(known.Keys)) {
      if (!IsWindow(h)) { known.Remove(h); continue; }
      if (!IsWindowVisible(h) || IsIconic(h)) continue;
      RECT r, area = new RECT(); int bar = 0; bool found = false;
      GetWindowRect(h, out r);
      // the terminal part of a Windows Terminal window: its content area minus the tab bar
      EnumChildWindows(h, (k, l2) => {
        var c = new StringBuilder(128); GetClassName(k, c, 128); RECT kr; GetWindowRect(k, out kr);
        if (c.ToString() == "DRAG_BAR_WINDOW_CLASS") bar = kr.B - kr.T;
        else if (c.ToString() == "Windows.UI.Composition.DesktopWindowContentBridge") { area = kr; found = true; }
        return true; }, IntPtr.Zero);
      if (!found) { area = r; bar = 0; }
      sb.AppendLine(known[h] + "|" + r.L + "|" + r.T + "|" + r.R + "|" + r.B + "|" + area.L + "|" + (area.T + bar) + "|" + area.R + "|" + area.B + "|" + GetDpiForWindow(h) + "|" + bar);
    }
    return sb.ToString();
  }
}
"@
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding $false
[Console]::Out.AutoFlush = $true
while ($true) { [Console]::Out.Write([CW]::List($prefix)); [Console]::Out.WriteLine("--"); Start-Sleep -Milliseconds 400 }
'''


def available():
    return shutil.which("powershell.exe") is not None


class WindowWatcher:
    """Keeps `windows` = {title: (left, top, right, bottom)} up to date from a powershell.exe child."""

    def __init__(self, prefix=PREFIX):
        self.prefix = prefix
        self.windows = {}  # title -> outer rectangle (left, top, right, bottom), screen pixels
        self.areas = {}  # title -> the terminal part of the window (without title and tab bar)
        self.dpis = {}  # title -> the window's DPI (96 = 100 %)
        self.updated = 0.0  # time of the last full list
        self.proc = None
        self._pending = {}
        self._pending_areas = {}
        self._pending_dpis = {}

    def start(self):
        script = _PS.replace("param([string]$prefix)", '$prefix = "%s"' % self.prefix.replace('"', ""))
        encoded = base64.b64encode(script.encode("utf-16-le")).decode()
        self.proc = subprocess.Popen(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", encoded],
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding="utf-8", errors="replace", bufsize=1)
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        for line in self.proc.stdout:
            line = line.strip()
            if line == "--":  # one complete list
                self.windows, self._pending, self.updated = self._pending, {}, time.time()
                self.areas, self._pending_areas = self._pending_areas, {}
                self.dpis, self._pending_dpis = self._pending_dpis, {}
                continue
            try:
                title, *nums = line.rsplit("|", 10)
                nums = [int(n) for n in nums]
                self._pending[title] = tuple(nums[:4])
                self._pending_areas[title] = tuple(nums[4:8])
                self._pending_dpis[title] = nums[8]
            except ValueError:
                continue

    def stop(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()

    def dpi_of(self, room_id):
        return self.dpis.get(self.prefix + room_id, 96)

    def area_of(self, room_id):
        """The terminal part of that window (screen pixels), or None."""
        if time.time() - self.updated > 3.0:
            return None
        return self.areas.get(self.prefix + room_id)

    def rect_of(self, room_id):
        """Rectangle of the window titled for `room_id`, or None (not found, minimized, stale)."""
        if time.time() - self.updated > 3.0:
            return None
        return self.windows.get(self.prefix + room_id)


def neighbor_side(mine, other, near=NEAR_PX, slack=0):
    """"right", "left", "down" or "up" if window `other` is next to window `mine` and close enough, else None."""
    if not mine or not other:
        return None
    top, bottom = max(mine[1], other[1]), min(mine[3], other[3])
    smaller = min(mine[3] - mine[1], other[3] - other[1])
    if smaller > 0 and bottom - top >= 0.3 * smaller:  # side by side: they overlap vertically
        gap_right = other[0] - mine[2]  # other's left edge minus my right edge
        gap_left = mine[0] - other[2]
        if -MAX_OVERLAP_PX - slack <= gap_right <= near + slack:
            return "right"
        if -MAX_OVERLAP_PX - slack <= gap_left <= near + slack:
            return "left"
    left, right = max(mine[0], other[0]), min(mine[2], other[2])
    narrower = min(mine[2] - mine[0], other[2] - other[0])
    if narrower > 0 and right - left >= 0.3 * narrower:  # stacked: they overlap horizontally
        gap_down = other[1] - mine[3]
        gap_up = mine[1] - other[3]
        if -MAX_OVERLAP_PX - slack <= gap_down <= near + slack:
            return "down"
        if -MAX_OVERLAP_PX - slack <= gap_up <= near + slack:
            return "up"
    return None


def overlap_center(a, b, axis):
    """Middle of where two rectangles overlap along an axis (0: left-right, 1: top-bottom), in screen pixels."""
    lo, hi = max(a[axis], b[axis]), min(a[axis + 2], b[axis + 2])
    return (lo + hi) / 2.0


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="list clawd-room windows on the Windows desktop")
    ap.add_argument("--prefix", default=PREFIX, help="window title prefix to look for")
    ap.add_argument("--seconds", type=float, default=3)
    a = ap.parse_args()
    if not available():
        raise SystemExit("powershell.exe not found (this needs WSL)")
    w = WindowWatcher(a.prefix)
    w.start()
    time.sleep(a.seconds)
    print(w.windows or "no windows with that title prefix")
    w.stop()


# ---- changing the window: font size and window size (only when the user asks, with the f key) --------
_TOOL_PS = r'''
Add-Type -AssemblyName System.Windows.Forms
Add-Type -ReferencedAssemblies System.Windows.Forms -TypeDefinition @"
using System; using System.Text; using System.Runtime.InteropServices;
public class CT {
  public delegate bool Proc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
  static System.Collections.Generic.Dictionary<string, IntPtr> cache = new System.Collections.Generic.Dictionary<string, IntPtr>();
  [DllImport("user32.dll")] static extern bool EnumWindows(Proc p, IntPtr l);
  [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
  static CT() { SetProcessDPIAware(); }  // real pixels, not scaled ones
  [DllImport("user32.dll")] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] static extern short GetAsyncKeyState(int k);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static IntPtr Find(string title) {
    IntPtr f;
    if (cache.TryGetValue(title, out f) && IsWindow(f)) return f;  // (the title changes in focus mode)
    f = IntPtr.Zero;
    EnumWindows((h, l) => { var t = new StringBuilder(256); GetWindowText(h, t, 256); if (t.ToString() == title) { f = h; return false; } return true; }, IntPtr.Zero);
    if (f != IntPtr.Zero) cache[title] = f;
    return f;
  }
  // make the window w x h pixels (keeps its top left corner)
  public static bool Resize(string title, int w, int h) {
    IntPtr hw = Find(title); if (hw == IntPtr.Zero) return false;
    return SetWindowPos(hw, IntPtr.Zero, 0, 0, w, h, 0x0006);
  }
  // Ctrl+numpad minus / plus (the terminal's font size shortcuts) sent to that window;
  // with onlyIfFocused the window is never brought to the front (nothing is sent if it is not already)
  public static bool Zoom(string title, int step, bool onlyIfFocused, bool waitMouse) { return Press(title, step, onlyIfFocused, false, waitMouse); }
  public static bool ResetZoom(string title, bool onlyIfFocused) { return Press(title, 0, onlyIfFocused, true, true); }
  static bool Press(string title, int step, bool onlyIfFocused, bool reset, bool waitMouse) {
    IntPtr hw = Find(title); if (hw == IntPtr.Zero) return false;
    for (int i = 0; waitMouse && i < 100 && (GetAsyncKeyState(1) & 0x8000) != 0; i++) System.Threading.Thread.Sleep(50);  // never while the mouse button is held (a drag)
    IntPtr prev = GetForegroundWindow();
    if (onlyIfFocused && prev != hw) return false;
    if (prev != hw) {
      SetForegroundWindow(hw); System.Threading.Thread.Sleep(150);
      if (GetForegroundWindow() != hw) return false;  // never type into some other window
    }
    if (reset) System.Windows.Forms.SendKeys.SendWait("^0");  // the terminal's "reset font size"
    for (int i = 0; i < Math.Abs(step); i++) {  // |step| presses in one go
      System.Windows.Forms.SendKeys.SendWait(step < 0 ? "^{SUBTRACT}" : "^{ADD}");
      System.Threading.Thread.Sleep(15);
    }
    System.Threading.Thread.Sleep(100);
    if (prev != hw && prev != IntPtr.Zero) SetForegroundWindow(prev);
    return true;
  }
}
"@
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding $false
[Console]::Out.AutoFlush = $true
while (($line = [Console]::In.ReadLine()) -ne $null) {
  $p = $line.Split('|')
  $ok = $false
  if ($p[0] -eq 'resize') { $ok = [CT]::Resize($p[1], [int]$p[2], [int]$p[3]) }
  elseif ($p[0] -eq 'zoom') { $ok = [CT]::Zoom($p[1], [int]$p[2], $false, $true) }
  elseif ($p[0] -eq 'resetfg') { $ok = [CT]::ResetZoom($p[1], $true) }
  elseif ($p[0] -eq 'zoomfg') { $ok = [CT]::Zoom($p[1], [int]$p[2], $true, $true) }
  elseif ($p[0] -eq 'zoomlive') { $ok = [CT]::Zoom($p[1], [int]$p[2], $true, $false) }
  [Console]::Out.WriteLine($(if ($ok) { 'ok' } else { 'fail' }))
}
'''


class WindowTool:
    """A powershell.exe that changes one window's size or font size when asked (started on first use)."""

    def __init__(self):
        self.proc = None

    def _send(self, line):
        if self.proc is None:
            encoded = base64.b64encode(_TOOL_PS.encode("utf-16-le")).decode()
            self.proc = subprocess.Popen(
                ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", encoded],
                stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True)
        try:
            self.proc.stdin.write(line + "\n")
            self.proc.stdin.flush()
        except (OSError, ValueError):
            self.proc = None

    def resize(self, title, width, height):
        self._send("resize|%s|%d|%d" % (title, width, height))

    def zoom(self, title, step, only_if_focused=False, live=False):
        """live: do not wait for the mouse button (zoom while the window is being dragged)."""
        kind = "zoomlive" if live else "zoomfg" if only_if_focused else "zoom"
        self._send("%s|%s|%d" % (kind, title, step))

    def reset_zoom(self, title):
        self._send("resetfg|%s|0" % title)

    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.stdin.close()
            except OSError:
                pass
            self.proc.terminate()
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()


class Fitter:
    """Brings a terminal window to exactly cols x rows cells.

    First the font is zoomed (as if you pressed Ctrl+minus / Ctrl+plus) until it is the biggest font
    for which the window still holds at least that many cells, then the window is resized to hold
    exactly that many. Call update() every frame; it acts at most once per second.
    """
    STEP_WAIT = 1.0

    def __init__(self, tool, title, cols, rows):
        self.tool, self.title, self.target = tool, title, (cols, rows)
        self.stage = "zoom"
        self.last = None  # last zoom direction: "in" or "out"
        self.prev = None  # grid size before the last action
        self.steps = 0
        self.cw = self.ch = None  # learnt pixels per cell
        self.next_at = 0.0
        self.sent = None  # (cols, rows, width, height) when the last resize was sent
        self.done = False
        self.message = ""

    def update(self, now, cols, rows, rect):
        if self.done or now < self.next_at or not rect:
            return
        tc, tr = self.target
        self.next_at = now + self.STEP_WAIT
        if self.stage == "zoom":
            ok = cols >= tc and rows >= tr
            stuck = self.prev == (cols, rows)  # the last zoom changed nothing: at the font size limit
            if self.steps >= 14 or stuck or (self.last == "in" and not ok):
                if self.last == "in" and not ok:
                    self.tool.zoom(self.title, -1)  # one step too far: go back
                    self.next_at = now + self.STEP_WAIT
                self.stage = "resize"
                self.prev = None
                return
            if ok and self.last == "out":
                self.stage = "resize"
                self.prev = None
                return
            self.last = "in" if ok else "out"
            self.prev = (cols, rows)
            self.steps += 1
            self.tool.zoom(self.title, 1 if ok else -1)
            return
        # resize stage: move the window edges by the cells still missing
        if (cols, rows) == (tc, tr):
            self.done, self.message = True, "FIT OK %dX%d" % (tc, tr)
            return
        w, h = rect[2] - rect[0], rect[3] - rect[1]
        if self.cw is None:
            self.cw, self.ch = max(4.0, (w - 30) / cols), max(8.0, (h - 90) / rows)
        elif self.sent:  # learn the real cell size from the last resize
            pc, pr, pw, ph = self.sent
            if cols != pc and w != pw:
                self.cw = max(4.0, abs((w - pw) / (cols - pc)))
            if rows != pr and h != ph:
                self.ch = max(8.0, abs((h - ph) / (rows - pr)))
        if self.steps >= 24:
            self.done, self.message = True, "FIT FAILED (%dX%d)" % (cols, rows)
            return
        self.steps += 1
        self.sent = (cols, rows, w, h)
        self.tool.resize(self.title, int(w + (tc - cols) * self.cw), int(h + (tr - rows) * self.ch))

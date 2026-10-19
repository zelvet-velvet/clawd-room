#!/usr/bin/env python3
"""clawd-room.sh --try on WSL: four linked Clawd Room windows in Windows Terminal, one in each quarter of the screen
(office, coffee shop / park, office), like the iTerm2 --try.

    python3 tools/try_wsl.py BIN TRY_DIR EVENTS [--desktop N]

The four open minimized one after another, then are placed and shown together (their visible frames on the quarters, like a
snap layout). On your own desktop they are then snapped with the keyboard (hold Win, tap two arrows, sent only to each room window while it
has the focus), which makes them a snap group: one taskbar entry. That takes the focus for a few seconds. --desktop N puts the windows
on virtual desktop N (1 = the first) without switching to it: each is moved there while minimized, then shown without activating. That uses Explorer's undocumented virtual desktop interfaces
(Windows 11 24H2 and later); it is for testing while you keep working on another desktop.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
ROOMS = [("office", "office", 0, 0), ("coffee", "coffee", 1, 0), ("park", "park", 0, 1), ("office2", "office", 1, 1)]

# Explorer's virtual desktop interfaces (IIDs of Windows 11 24H2 / 25H2; earlier builds use other ones and fail cleanly)
VD_CS = r'''
using System; using System.Text; using System.Runtime.InteropServices;
namespace ClawdTry {
[ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("6D5140C1-7436-11CE-8034-00AA006009FA")]
interface IServiceProvider10 { [return: MarshalAs(UnmanagedType.IUnknown)] object QueryService(ref Guid service, ref Guid riid); }
[ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("92CA9DCD-5622-4BBA-A805-5E9F541BD8C9")]
interface IObjectArray { void GetCount(out int count); void GetAt(int index, ref Guid iid, [MarshalAs(UnmanagedType.Interface)] out object obj); }
[ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("372E1D3B-38D3-42E4-A15B-8AB2B178F513")]
interface IApplicationView { }
[ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("3F07F4BE-B107-441A-AF0F-39D82529072C")]
interface IVirtualDesktop { bool IsViewVisible(IApplicationView view); Guid GetId(); }
[ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("53F5CA0B-158F-4124-900C-057158060B27")]
interface IVirtualDesktopManagerInternal {
  int GetCount(); void MoveViewToDesktop(IApplicationView view, IVirtualDesktop desktop); bool CanViewMoveDesktops(IApplicationView view);
  IVirtualDesktop GetCurrentDesktop(); void GetDesktops(out IObjectArray desktops); }
[ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("1841C6D7-4F9D-42C0-AF41-8747538F10E5")]
interface IApplicationViewCollection { int GetViews(out IObjectArray a); int GetViewsByZOrder(out IObjectArray a); int GetViewsByAppUserModelId(string id, out IObjectArray a); int GetViewForHwnd(IntPtr hwnd, out IApplicationView view); }
public static class T {
  [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] static extern bool SystemParametersInfo(int a, int b, int[] r, int c);
  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  public static int[] WorkArea() { SetProcessDPIAware(); var r = new int[4]; SystemParametersInfo(0x30, 0, r, 0); return r; } /* SPI_GETWORKAREA, real pixels */
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  static System.Collections.Generic.HashSet<IntPtr> seen = new System.Collections.Generic.HashSet<IntPtr>();
  /* Windows Terminal's windows; Fresh() gives one that was not there at the last call (the room sets its own title later) */
  public static void Mark() { EnumWindows((h, l) => { var s = new StringBuilder(64); GetClassName(h, s, 64); if (s.ToString() == "CASCADIA_HOSTING_WINDOW_CLASS") seen.Add(h); return true; }, IntPtr.Zero); }
  /* the room in it has started (it sets the title "clawd-room NAME-PID"): Windows Terminal is done opening the window, which
     would otherwise minimize it again after it was shown */
  public static bool Started(IntPtr h) { var s = new StringBuilder(256); GetWindowText(h, s, 256); return s.ToString().StartsWith("clawd-room "); }
  public static IntPtr Fresh() { IntPtr r = IntPtr.Zero; EnumWindows((h, l) => { var s = new StringBuilder(64); GetClassName(h, s, 64); if (s.ToString() == "CASCADIA_HOSTING_WINDOW_CLASS" && !seen.Contains(h)) { r = h; return false; } return true; }, IntPtr.Zero); if (r != IntPtr.Zero) seen.Add(r); return r; }
  static IServiceProvider10 Shell() { return (IServiceProvider10)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("C2F03A33-21F5-47FA-B4BB-156362A2F239"))); }
  static IVirtualDesktopManagerInternal Mgr() { Guid c = new Guid("C5E0CDCA-7B6E-41B2-9FC4-D93975CC467B"), i = typeof(IVirtualDesktopManagerInternal).GUID; return (IVirtualDesktopManagerInternal)Shell().QueryService(ref c, ref i); }
  static IApplicationViewCollection Views() { Guid i = typeof(IApplicationViewCollection).GUID, c = i; return (IApplicationViewCollection)Shell().QueryService(ref c, ref i); }
  static IVirtualDesktop At(int n) { IObjectArray a; Mgr().GetDesktops(out a); object o; Guid i = typeof(IVirtualDesktop).GUID; a.GetAt(n, ref i, out o); return (IVirtualDesktop)o; }
  public static int Desktops() { return Mgr().GetCount(); }
  public static bool OnCurrent(int n) { return Mgr().GetCurrentDesktop().GetId() == At(n).GetId(); }
  public static void ToDesktop(IntPtr h, int n) { /* a new window has no view for a moment: try again for up to 5 s */
    for (int k = 0; ; k++) { try { IApplicationView v; Views().GetViewForHwnd(h, out v); Mgr().MoveViewToDesktop(v, At(n)); return; }
      catch (COMException) { if (k >= 250) throw; System.Threading.Thread.Sleep(20); } } }
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, int[] r);
  [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(IntPtr h, int a, int[] r, int n);
  /* SW_SHOWNOACTIVATE, then the visible frame onto r (x, y, w, h) like Windows' snap layout: the window rectangle includes an
     invisible resize border, so it is grown by that border (DWMWA_EXTENDED_FRAME_BOUNDS is the visible part); no z-order, no activate */
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
  /* the focus to h (a tap of Alt lets a background process set it); true only if h really has it */
  public static bool Focus(IntPtr h) { keybd_event(0x12, 0, 0, UIntPtr.Zero); keybd_event(0x12, 0, 2, UIntPtr.Zero); SetForegroundWindow(h); System.Threading.Thread.Sleep(250); return GetForegroundWindow() == h; }
  static void Tap(byte k) { keybd_event(k, 0, 1, UIntPtr.Zero); keybd_event(k, 0, 3, UIntPtr.Zero); System.Threading.Thread.Sleep(60); }
  /* snap h into a quarter with the keyboard as a person would: hold Win, tap left / right then up / down, let go of Win;
     windows snapped this way become a snap group (one taskbar entry). Keys go only to h while it has the focus; no Esc (it
     quits a room). False if h could not be focused. */
  public static bool Snap(IntPtr h, byte side, byte half) {
    if (!Focus(h)) return false;
    keybd_event(0x5B, 0, 0, UIntPtr.Zero); Tap(side); System.Threading.Thread.Sleep(300); Tap(half); keybd_event(0x5B, 0, 2, UIntPtr.Zero);  // (the second arrow only once the first snap is done, or it stays a half)
    System.Threading.Thread.Sleep(400); return true;
  }
  public static void Show(IntPtr h, int[] r) {
    ShowWindow(h, 4); SetWindowPos(h, IntPtr.Zero, r[0], r[1], r[2], r[3], 0x0014);
    int[] w = new int[4], f = new int[4];
    if (GetWindowRect(h, w) && DwmGetWindowAttribute(h, 9, f, 16) == 0)
      SetWindowPos(h, IntPtr.Zero, r[0] - (f[0] - w[0]), r[1] - (f[1] - w[1]), r[2] + (f[0] - w[0]) + (w[2] - f[2]), r[3] + (f[1] - w[1]) + (w[3] - f[3]), 0x0014);
  }
}}
'''


def ps_quote(s):
    return "'" + s.replace("'", "''") + "'"


def main():
    args = sys.argv[1:]
    desk = 0
    if "--desktop" in args:
        i = args.index("--desktop")
        desk = int(args[i + 1])
        del args[i:i + 2]
    check_only = args == ["--check"]  # (only the desktop checks: run before a running --try is ended)
    binp, tr, ev = ("", "", "") if check_only else args
    distro = os.environ.get("WSL_DISTRO_NAME", "Ubuntu")
    lines = ["$ErrorActionPreference = 'Stop'", "Add-Type -TypeDefinition " + ps_quote(VD_CS),
             "$clk = [Diagnostics.Stopwatch]::StartNew(); $all = @(); [ClawdTry.T]::Mark(); $wa = [ClawdTry.T]::WorkArea(); $w = [int](($wa[2] - $wa[0]) / 2); $h = [int](($wa[3] - $wa[1]) / 2)"]
    if desk:
        lines += ["if ([ClawdTry.T]::Desktops() -lt %d) { throw 'there is no desktop %d' }" % (desk, desk),
                  "if ([ClawdTry.T]::OnCurrent(%d)) { throw 'desktop %d is the one you are on' }" % (desk - 1, desk)]
    for name, theme, qx, qy in [] if check_only else ROOMS:  # (PATH: wt.exe started from WSL passes on a PATH without Windows' folders, so no powershell.exe)
        cmd = ["wsl.exe", "-d", distro, "-e", "env", "CLAWD_HERE=1", "PATH=" + os.environ.get("PATH", ""), "CLAWD_LINK_DIR=%s/link" % tr, "CLAWD_SIZE_LOG=%s/log-%s" % (tr, name),
               "sh", os.path.join(ROOT, "tools", "try_room.sh"), binp, tr + "/stop", "--room", name, "--theme", theme,
               "--scale", "1", "--size", "off", "--profile-font", "5", "--events", ev]  # (a fixed scale and the window's own size: the quarter)
        wt = ["-w", "new", "-p", "Clawd Room"] + cmd
        wt = ['"%s"' % a if " " in a else a for a in wt]  # (Start-Process joins the arguments with spaces)
        lines += ["$r = [int[]]@(($wa[0] + %d * $w), ($wa[1] + %d * $h), $w, $h)" % (qx, qy),  # (the comma binds before + and *)
                  "Start-Process wt.exe -WindowStyle Minimized -ArgumentList @(%s)" % ", ".join(ps_quote(a) for a in wt),
                  "$hw = [IntPtr]::Zero; $t = [Diagnostics.Stopwatch]::StartNew()",
                  "while ($hw -eq [IntPtr]::Zero -and $t.ElapsedMilliseconds -lt 15000) { Start-Sleep -Milliseconds 20; $hw = [ClawdTry.T]::Fresh() }",
                  "if ($hw -eq [IntPtr]::Zero) { throw 'the %s window did not open' }" % name]
        if desk:
            lines.append("[ClawdTry.T]::ToDesktop($hw, %d)" % (desk - 1))
        lines.append("$all += ,@($hw, $r); 'opened %s ' + $clk.ElapsedMilliseconds" % name)
    # all four show together: each opened minimized (and moved to its desktop) above, now placed and shown in one go;
    # on your own desktop the first one gets the focus
    lines += [] if check_only else [
        "$t = [Diagnostics.Stopwatch]::StartNew(); while (@($all | ? { -not [ClawdTry.T]::Started($_[0]) }).Count -gt 0 -and $t.ElapsedMilliseconds -lt 15000) { Start-Sleep -Milliseconds 50 }",
        "'started all ' + $clk.ElapsedMilliseconds",
        "foreach ($a in $all) { [ClawdTry.T]::Show($a[0], $a[1]) }", "'shown all ' + $clk.ElapsedMilliseconds"]
    if not desk and not check_only and os.environ.get("CLAWD_TRY_SNAP") != "0":  # on your own desktop: snap them by keyboard, so they form a snap group (a few seconds, takes the focus; CLAWD_TRY_SNAP=0: not)
        keys = [("0x25", "0x26"), ("0x27", "0x26"), ("0x25", "0x28"), ("0x27", "0x28")]  # left/right, then up/down
        lines += ["if (-not [ClawdTry.T]::Snap($all[%d][0], %s, %s)) { 'not snapped: %s' }" % (i, k[0], k[1], ROOMS[i][0]) for i, k in enumerate(keys)]
        lines.append("[void][ClawdTry.T]::Focus($all[0][0])")
    script = "\n".join(lines)
    # through a file in Windows' temp folder (with the PATHs the script is too long for -EncodedCommand)
    tmp = subprocess.run(["cmd.exe", "/d", "/c", "echo %TEMP%"], capture_output=True, text=True, cwd="/mnt/c").stdout.strip()
    path = os.path.join(subprocess.run(["wslpath", "-u", tmp], capture_output=True, text=True).stdout.strip(), "clawd-room-try.ps1")
    with open(path, "w", encoding="utf-8-sig") as f:
        f.write(script)
    p = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", tmp + "\\clawd-room-try.ps1"],
                       capture_output=True, text=True)
    os.remove(path)
    if os.environ.get("CLAWD_TRY_TIMING"):  # (how long each window took, in ms since the script started)
        sys.stderr.write(p.stdout.replace("\r", ""))
    if p.returncode != 0:
        err = re.findall(r'<S S="Error">(.*?)</S>', p.stderr) or [l for l in p.stderr.replace("\r", "").splitlines() if l.strip()]
        err = [re.sub(r"_x000[DA]_", "", e).strip() for e in err]
        sys.stderr.write("clawd-room: --try: %s\n" % (err[0] if err else "powershell failed"))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

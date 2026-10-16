#!/usr/bin/env python3
"""Test helper (WSL): moves and resizes ONLY windows whose title starts with the given exact titles (your own test windows).

    python3 tools/place_windows.py "clawd-room alpha-123" 100 100 900 600 "clawd-room beta-456" 940 100 900 600
"""
import base64
import subprocess
import sys

PS = r'''
Add-Type -TypeDefinition @"
using System; using System.Text; using System.Runtime.InteropServices;
public class P {
  public delegate bool Proc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(Proc p, IntPtr l);
  [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  public static bool Place(string title, int x, int y, int w, int h) {
    SetProcessDPIAware(); IntPtr f = IntPtr.Zero;
    EnumWindows((hw, l) => { var t = new StringBuilder(256); GetWindowText(hw, t, 256); if (t.ToString() == title) { f = hw; return false; } return true; }, IntPtr.Zero);
    if (f == IntPtr.Zero) return false;
    return SetWindowPos(f, IntPtr.Zero, x, y, w, h, 0x0004);
  }
}
"@
'''
args = sys.argv[1:]
cmds = []
for i in range(0, len(args), 5):
    t, x, y, w, h = args[i:i + 5]
    cmds.append('Write-Output ("{0}: " + [P]::Place("{0}", {1}, {2}, {3}, {4}))'.format(t.replace('"', ''), int(x), int(y), int(w), int(h)))
script = PS + "\n".join(cmds)
enc = base64.b64encode(script.encode("utf-16-le")).decode()
print(subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", enc], capture_output=True, text=True).stdout)

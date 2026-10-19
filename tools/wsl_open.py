#!/usr/bin/env python3
"""Open one program in its own Windows Terminal window with the Clawd Room profile (WSL), the way the iTerm2 launcher does:
the window opens minimized, waits until the program has named it, is placed in the middle of the screen (W x H percent of the
work area, its visible frame) and then shown with the focus. The window you typed in stays as it is.

    python3 tools/wsl_open.py TITLE_PREFIX W_PERCENT H_PERCENT PROGRAM ARGS...

Used for clawd-room.sh --edit (the pixel editor names its window "Clawd Room pixel editor").
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from try_wsl import VD_CS, ps_quote, run_ps, ps_error  # noqa: E402  (the window helpers, shared with --try)


def main():
    prefix, wp, hp = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    prog = sys.argv[4:]
    distro = os.environ.get("WSL_DISTRO_NAME", "Ubuntu")
    cmd = ["wsl.exe", "-d", distro, "--cd", os.getcwd(), "-e", "env", "CLAWD_HERE=1", "PATH=" + os.environ.get("PATH", "")] + prog
    wt = ["-w", "new", "-p", "Clawd Room"] + cmd
    wt = ['"%s"' % a if " " in a else a for a in wt]  # (Start-Process joins the arguments with spaces)
    script = "\n".join([
        "$ErrorActionPreference = 'Stop'", "Add-Type -TypeDefinition " + ps_quote(VD_CS),
        "[ClawdTry.T]::Mark(); $wa = [ClawdTry.T]::WorkArea()",
        "$w = [int](($wa[2] - $wa[0]) * %d / 100); $h = [int](($wa[3] - $wa[1]) * %d / 100)" % (wp, hp),
        "$r = [int[]]@(($wa[0] + [int](($wa[2] - $wa[0] - $w) / 2)), ($wa[1] + [int](($wa[3] - $wa[1] - $h) / 2)), $w, $h)",
        "Start-Process wt.exe -WindowStyle Minimized -ArgumentList @(%s)" % ", ".join(ps_quote(a) for a in wt),
        "$hw = [IntPtr]::Zero; $t = [Diagnostics.Stopwatch]::StartNew()",
        "while ($hw -eq [IntPtr]::Zero -and $t.ElapsedMilliseconds -lt 15000) { Start-Sleep -Milliseconds 10; $hw = [ClawdTry.T]::Fresh() }",
        "if ($hw -eq [IntPtr]::Zero) { throw 'the window did not open' }",
        "while (-not [ClawdTry.T]::Title($hw).StartsWith(%s) -and $t.ElapsedMilliseconds -lt 15000) { Start-Sleep -Milliseconds 20 }" % ps_quote(prefix),
        "[ClawdTry.T]::Show($hw, $r); [void][ClawdTry.T]::Focus($hw)",
    ])
    p = run_ps(script, "clawd-room-open.ps1")
    if p.returncode != 0:
        sys.stderr.write("clawd-room: %s\n" % ps_error(p))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

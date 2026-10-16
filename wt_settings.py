#!/usr/bin/env python3
"""Print the Windows Terminal settings that hide the tab bar and the scroll bar (it changes nothing).

A program cannot hide Windows Terminal's tab bar or scroll bar from the inside (the terminal's own
"focus mode" can be toggled with keystrokes, but in tests it also dropped the window, so clawd-room
does not do that). These are plain settings, so add them yourself:  Windows Terminal -> Settings ->
Open JSON file.
"""
import subprocess

SNIPPET = '''
  // top level of settings.json: no tab bar while there is only one tab
  "alwaysShowTabs": false,
  "showTabsInTitlebar": false,

  // in "profiles": { "defaults": { ... } }  (or in the profile that opens WSL)
  "scrollbarState": "hidden",
'''


def settings_path():
    try:
        out = subprocess.run(
            ["powershell.exe", "-NoProfile", "-Command",
             r'$d = Get-ChildItem "$env:LOCALAPPDATA\Packages" -Filter "Microsoft.WindowsTerminal*" | Select -First 1;'
             r' Join-Path $d.FullName "LocalState\settings.json"'],
            capture_output=True, text=True, timeout=20).stdout.strip()
        return out or "(not found)"
    except (OSError, subprocess.SubprocessError):
        return "(powershell.exe not available)"


if __name__ == "__main__":
    print("Windows Terminal settings file:", settings_path())
    print("Add these:" + SNIPPET)

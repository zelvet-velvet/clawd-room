---
name: cmd-rollover
description: clawd-room progress command. Use when the user runs /cmd-rollover in the clawd-room repo (or says the day is over): moves Today to Yesterday in "Clawd Room Note/Review/Dev Status.md" with tools/dev_status.py.
---
# /cmd-rollover (clawd-room)

The script does the file work; do not read the LLM Guide or the whole Dev Status for this. From the repo root:

1. `python3 tools/dev_status.py show` prints Today's date and items. Empty → tell the user to run `/cmd-sync` first; stop.
2. `python3 tools/dev_status.py rollover`. It replaces Yesterday with Today (folded `[!abstract]-`), empties Today, dates it today and refreshes the counts.
    - Exit `2`: that day's note has no entry yet → run `/cmd-journal <date>` (skill `cmd-journal`), then run the script again.
    - Exit `1`: Today is empty; nothing changed.
3. Commit only Dev Status (and the day note if step 2 wrote one) on the current `docs/…` branch, `docs(vault): rollover <date>`; never on `main` (skill `room-git`). Not on a docs branch: ask which branch.
4. Report the script output in one or two lines. After this, it is a new day.

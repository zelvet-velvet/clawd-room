---
name: room-test-queue
description: Rules for the test queue in clawd-room's "Clawd Room Note/Review/Test Checklist.md": groups per branch, test IDs, who ticks, archiving. Use when adding tests that need someone's eyes, when the user reports a test result, or when a branch with a group is merged.
---
# Test queue (clawd-room)

Note: `Clawd Room Note/Review/Test Checklist.md`, section `## 🧪 Test queue`. Finished groups: `Review/Test Checklist Archive.md`.

- **A group per branch**: `> [!todo]- Group X · <platform>: <subject> (T63–T66)`, the next free letter. It lives **on the feature branch** with the code, so the tester ticks it there.
- **Test IDs** count on across the note, the archive and open branches, and never change: find the highest with `git grep -ho '\*\*T[0-9]*\*\*' $(git branch -r --format='%(refname:short)') -- 'Clawd Room Note/Review/' | sort -uV | tail -1` (open questions `**Q2**`). Each test is one box: `> - [ ] **T63** what to do → what should happen.`
- **Steps** in a folded `> > [!tip]- T63–T66 · how to run them` inside the group (branch to switch to, commands, a small picture to compare with).
- **Who ticks**: only a person. Never tick, untick or reword a person's result. After a check the person (or you, writing what they reported) adds `> ✅ 2026-11-02: what was seen (who).` Boxes follow `_Meta/Checkbox Style.md` (`[/]` partly, `[!]` fails, `[?]` unsure, `[-]` dropped, `[>]` later, `[i]` a note). Looks and art are ticked by Zaviel.
- **Callout type follows the boxes** in the same edit (LLM Guide, Conventions): open `[!todo]`, all `[x]` `[!done]`, any `[!]` `[!warning]`, any `[?]` `[!question]`.
- **Archive**: once the branch is merged and every box is `[x]` or `[-]`, move the group (`[!done]-`) to the end of `Test Checklist Archive.md`, IDs unchanged. Keep the `No open groups` line in the queue when it is empty.

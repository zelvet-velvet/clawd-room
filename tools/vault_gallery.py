#!/usr/bin/env python3
"""Write the vault's screenshot notes from the pictures and the Dev Log.

    python3 tools/vault_gallery.py

`Clawd Room Note/Snapshots/` is the source: one folder per subject holding the picture files, named
`YYYY-MM-DD subject.png`. For people to browse them this writes `Clawd Room Note/Gallery/`: a note per subject,
`SUBJECT gallery.md` (newest first: the picture, its caption, the Dev Log entry and notes that show it), and
`Gallery index.md`. (Notes named like their folder are make.md's folder notes: never written here.) Run it after
adding pictures (CMD_SYNC does). Only the notes in Gallery are rewritten.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
VAULT = os.path.join(ROOT, "Clawd Room Note")
SHOTS = os.path.join(VAULT, "Snapshots")
LOG = os.path.join(VAULT, "Gallery")
STATUS = os.path.join(VAULT, "_Meta", "Snapshot status.md")  # (picture | current / replaced / not picked | replaced by)
LOOK = {"current": ("success", "+", "✅ current"), "replaced": ("quote", "-", "♻️ replaced"), "not picked": ("failure", "-", "❌ not picked")}
PIC = (".png", ".gif", ".jpg", ".jpeg")
NAME = re.compile(r"^(\d{4}-\d{2}-\d{2}) (.+)\.(?:png|gif|jpe?g)$", re.I)
SUBJECT_ORDER = ["Office", "Coffee shop", "Park", "Gates (stairs, doors, wells)", "Interaction (keys, moving)", "Engine and tools",
                 "Platforms", "Style and mockups", "Old look"]
ABOUT = {
    "Office": "the office room",
    "Coffee shop": "the coffee shop",
    "Park": "the park plaza and its buildings",
    "Gates (stairs, doors, wells)": "the ways between rooms: doors, stairs, the half floor, wells",
    "Interaction (keys, moving)": "keys, hints, sending and moving Clawds",
    "Engine and tools": "the debug overlay, the placer, the pixel editor, the gallery",
    "Platforms": "Windows Terminal, iTerm2, `--try`",
    "Style and mockups": "mockups and options to choose from (the picked one is marked)",
    "Old look": "how it looked before style A",
}


def note_name(folder):
    return folder + " gallery"


def statuses():
    """{picture: (status, replaced by)} from _Meta/Snapshot status.md; a picture without a row is current."""
    out = {}
    if os.path.exists(STATUS):
        with open(STATUS, encoding="utf-8") as fh:
            for line in fh:
                cells = [c.strip() for c in line.strip().strip("|").split("|")]
                if len(cells) >= 2 and cells[1] in LOOK:
                    by = re.sub(r"^\[\[|\]\]$", "", cells[2]) if len(cells) > 2 else ""
                    out[cells[0]] = (cells[1], by)
    return out


def where_shown():
    """{picture name: [(note, heading)]} for every embed in the vault's notes (not the generated ones)."""
    shown = {}
    for root, dirs, files in os.walk(VAULT):
        dirs[:] = [d for d in dirs if not d.startswith(".")]
        for f in files:
            if not f.endswith(".md") or root.startswith(LOG):
                continue
            note = f[:-3]
            heading = ""
            with open(os.path.join(root, f), encoding="utf-8") as fh:
                for line in fh:
                    m = re.match(r"^#{1,6} (.+?)\s*$", line)
                    if m:
                        heading = m.group(1)
                    for emb in re.findall(r"!\[\[([^|\]]+)", line):
                        shown.setdefault(os.path.basename(emb), []).append((note, heading))
    return shown


def caption(subject):
    who = ""
    m = re.search(r"\s*\(([A-Z][a-z]+)\)$", subject)
    if m:
        who, subject = m.group(1), subject[:m.start()]
    picked = "(picked)" in subject
    subject = subject.replace(" (picked)", "")
    text = subject[:1].upper() + subject[1:]
    if picked:
        text += " · **picked**"
    if who:
        text += " · by " + who
    return text


def links(name, shown):
    out = []
    for note, heading in shown.get(name, []):
        target = "%s#%s" % (note, heading) if heading and note == "Dev Log" else note
        label = heading if note == "Dev Log" and heading else note
        link = "[[%s|%s]]" % (target, label)
        if link not in out:
            out.append(link)
    return ", ".join(out) if out else "not in a note yet"


def write_subject(folder, shown, status):
    path = os.path.join(SHOTS, folder)
    pics = sorted((f for f in os.listdir(path) if f.lower().endswith(PIC)), key=lambda f: (f[:10], f), reverse=True)
    lines = ["---", "title: Gallery · %s" % folder, "tags:", "  - clawd-room", "  - screenshots", "up: \"[[Gallery index]]\"",
             "---", "# 📷 %s" % folder, "",
             "Pictures of %s, newest first: ✅ current (open), ♻️ replaced and ❌ not picked (folded; the status is in "
             "[[Snapshot status]]). The files are the source in `Snapshots/%s/`; this note is written by "
             "`tools/vault_gallery.py` (do not edit it by hand: add a picture, then run the tool)." % (ABOUT.get(folder, folder.lower()), folder), ""]
    day = None
    for f in pics:
        m = NAME.match(f)
        date, subject = (m.group(1), m.group(2)) if m else ("undated", f.rsplit(".", 1)[0])
        if date != day:
            day = date
            lines += ["## %s" % date, ""]
        st, by = status.get(f, ("current", ""))
        kind, fold, label = LOOK[st]
        lines += ["> [!%s]%s %s · %s" % (kind, fold, label, caption(subject).replace("**", "")), "> ![[%s|420]]" % f]
        if by:
            lines.append("> Replaced by: [[%s]]" % by)
        lines += ["> Shown in: %s" % links(f, shown), ""]
    with open(os.path.join(LOG, note_name(folder) + ".md"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines).rstrip() + "\n")
    return pics


def main():
    os.makedirs(LOG, exist_ok=True)
    shown = where_shown()
    status = statuses()
    folders = [d for d in os.listdir(SHOTS) if os.path.isdir(os.path.join(SHOTS, d)) and not d.startswith(".")]
    folders.sort(key=lambda d: (SUBJECT_ORDER.index(d) if d in SUBJECT_ORDER else 99, d))
    loose = [f for f in os.listdir(SHOTS) if f.lower().endswith(PIC)]
    rows = []
    for d in folders:
        pics = write_subject(d, shown, status)
        count = {k: sum(1 for p in pics if status.get(p, ("current", ""))[0] == k) for k in LOOK}
        cur = [p for p in pics if status.get(p, ("current", ""))[0] == "current"]
        newest = cur[0] if cur else pics[0] if pics else None  # (the newest picture that is still current, if there is one)
        rows.append("| [[%s]] | %s | %d | %d | %d | %s | %s |" % (note_name(d), ABOUT.get(d, ""), count["current"], count["replaced"], count["not picked"], newest[:10] if newest else "", "![[%s\\|160]]" % newest if newest else ""))  # (a | in a table cell needs a backslash)
    index = ["---", "title: Gallery index", "tags:", "  - clawd-room", "  - screenshots", "---", "# 📷 Gallery", "",
             "The pictures of clawd-room (snapshots): what the room looked like, mockups that were picked from, the user's own screenshots. "
             "**`Snapshots/` is the source**: one folder per subject with the picture files (`YYYY-MM-DD subject.png`; a person's "
             "own: `... (Zaviel).png`; the picked mockup: `... (picked).png`). Browse them in the notes below (this folder, "
             "Gallery, written by `tools/vault_gallery.py`); the "
             "[[Dev Log]] shows them with what changed.", "",
             "> [!tip] Adding a picture",
             "> 1. Save it in the subject folder that fits, named `YYYY-MM-DD subject.png` (names stay unique across folders).",
             "> 2. Embed it in its [[Dev Log]] entry: `![[YYYY-MM-DD subject.png|300]]`.",
             "> 3. Give it a row in [[Snapshot status]] (and mark what it replaces as replaced).",
             "> 4. `python3 tools/vault_gallery.py` rewrites the notes in Gallery (CMD_SYNC runs it too).", "",
             "| Subject | What | ✅ current | ♻️ replaced | ❌ not picked | Newest current | |", "| --- | --- | --- | --- | --- | --- | --- |"] + rows
    if loose:
        index += ["", "> [!warning] Not in a subject folder yet", "> " + ", ".join(sorted(loose))]
    with open(os.path.join(LOG, "Gallery index.md"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(index) + "\n")
    print("wrote %d subject notes and Gallery index.md in Gallery (%d pictures)" % (len(folders), sum(sum(int(x) for x in r.split("|")[3:6]) for r in rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Write the vault's screenshot notes from the pictures and the Dev Log.

    python3 tools/vault_gallery.py

`Clawd Room Note/Assets/Snapshots/` is the source: one folder per subject holding the picture files, named
`YYYY-MM-DD subject.png`. For people to browse them this writes `Clawd Room Note/Gallery/`: a note per subject named
like it, in a folder per category (`Gallery/Rooms/Park.md`: newest first, the picture, its caption, the Dev Log entry and
notes that show it), and `Gallery/Index.md`. (Notes named like their folder are make.md's folder notes: never written here.)
Run it after adding pictures (CMD_SYNC does). Gallery is written only by this: notes in it that it did not write are removed.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
VAULT = os.path.join(ROOT, "Clawd Room Note")
SHOTS = os.path.join(VAULT, "Assets", "Snapshots")
LOG = os.path.join(VAULT, "Gallery")
STATUS = os.path.join(VAULT, "_Meta", "Snapshot status.md")  # (picture | current / replaced / not picked | replaced by)
LOOK = {"current": ("success", "+", "✅ current"), "replaced": ("quote", "-", "♻️ replaced"), "not picked": ("failure", "-", "❌ not picked")}
PIC = (".png", ".gif", ".jpg", ".jpeg")
NAME = re.compile(r"^(\d{4}-\d{2}-\d{2}) (.+)\.(?:png|gif|jpe?g)$", re.I)
SUBJECT_ORDER = ["Office", "Coffee shop", "Park", "Gates (stairs, doors, wells)", "Interaction (keys, moving)", "Engine and tools",
                 "Platforms", "Style and mockups", "Old look"]
ICON = {"Office": "briefcase", "Coffee shop": "coffee", "Park": "trees", "Gates (stairs, doors, wells)": "door-open",
        "Interaction (keys, moving)": "keyboard", "Engine and tools": "wrench", "Platforms": "monitor", "Style and mockups": "palette",
        "Old look": "history"}  # (each note's make.md sticker, a lucide icon)


def sticker(name):
    return "lucide//" + name


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


CATEGORY = {"Office": "Rooms", "Coffee shop": "Rooms", "Park": "Rooms", "Gates (stairs, doors, wells)": "Gates and moving",
            "Interaction (keys, moving)": "Gates and moving", "Engine and tools": "Tools and platforms", "Platforms": "Tools and platforms",
            "Style and mockups": "Style", "Old look": "Style"}  # (the Gallery folder each subject's note is in)
INDEX = "Index"


def note_name(folder):
    return folder


def note_path(folder):
    return os.path.join(LOG, CATEGORY.get(folder, "Other"), note_name(folder) + ".md")


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
        day = re.match(r"^\d{4}-\d{2}-\d{2}$", note)  # (a day note of the Dev Log: link to the entry)
        target = "%s#%s" % (note, heading) if heading and day else note
        label = "%s · %s" % (note, heading) if heading and day else note
        link = "[[%s|%s]]" % (target, label)
        if link not in out:
            out.append(link)
    return ", ".join(out) if out else "not in a note yet"


def write_subject(folder, shown, status):
    path = os.path.join(SHOTS, folder)
    pics = sorted((f for f in os.listdir(path) if f.lower().endswith(PIC)), key=lambda f: (f[:10], f), reverse=True)
    lines = ["---", "title: Gallery · %s" % folder, "tags:", "  - clawd-room", "  - screenshots", "up: \"[[%s]]\"" % INDEX,
             "sticker: " + sticker(ICON.get(folder, "image")), "---",
             "Pictures of %s, newest first: ✅ current (open), ♻️ replaced and ❌ not picked (folded; the status is in "
             "[[Snapshot status]]). The files are the source in `Assets/Snapshots/%s/`; this note is written by "
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
    os.makedirs(os.path.dirname(note_path(folder)), exist_ok=True)
    with open(note_path(folder), "w", encoding="utf-8") as fh:
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
        rows.append("| %s | [[%s]] | %s | %d | %d | %d | %s | %s |" % (CATEGORY.get(d, "Other"), note_name(d), ABOUT.get(d, ""), count["current"], count["replaced"], count["not picked"], newest[:10] if newest else "", "![[%s\\|160]]" % newest if newest else ""))  # (a | in a table cell needs a backslash)
    index = ["---", "title: Gallery · Index", "tags:", "  - clawd-room", "  - screenshots", "sticker: " + sticker("images"), "---",
             "The pictures of clawd-room (snapshots): what the room looked like, mockups that were picked from, the user's own screenshots. "
             "**`Assets/Snapshots/` is the source**: one folder per subject with the picture files (`YYYY-MM-DD subject.png`; a person's "
             "own: `... (Zaviel).png`; the picked mockup: `... (picked).png`). Browse them in the notes below (this folder, "
             "Gallery, written by `tools/vault_gallery.py`); the "
             "[[Dev Log]] shows them with what changed.", "",
             "> [!tip] Adding a picture",
             "> 1. Save it in the subject folder that fits, named `YYYY-MM-DD subject.png` (names stay unique across folders).",
             "> 2. Embed it in its Dev Log entry (the day's note in `Review/Log/`): `![[YYYY-MM-DD subject.png|300]]`.",
             "> 3. Give it a row in [[Snapshot status]] (and mark what it replaces as replaced).",
             "> 4. `python3 tools/vault_gallery.py` rewrites the notes in Gallery (CMD_SYNC runs it too).", "",
             "| Category | Subject | What | ✅ current | ♻️ replaced | ❌ not picked | Newest current | |", "| --- | --- | --- | --- | --- | --- | --- | --- |"] + rows
    if loose:
        index += ["", "> [!warning] Not in a subject folder yet", "> " + ", ".join(sorted(loose))]
    with open(os.path.join(LOG, INDEX + ".md"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(index) + "\n")
    keep = {os.path.join(LOG, INDEX + ".md")} | {note_path(d) for d in folders}
    for root, dirs, files in os.walk(LOG, topdown=False):  # (notes it no longer writes, e.g. the old `Park gallery.md`, and empty folders)
        for f in files:
            if f.endswith(".md") and os.path.join(root, f) not in keep:
                os.remove(os.path.join(root, f))
        if root != LOG and not os.listdir(root):
            os.rmdir(root)
    print("wrote %d subject notes and %s.md in Gallery (%d pictures)" % (len(folders), INDEX, sum(sum(int(x) for x in r.split("|")[4:7]) for r in rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

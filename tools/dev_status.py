"""Keep Review/Dev Status.md in shape, so the assistant never counts or moves sections by hand.

Usage (from the repo root):
    python3 tools/dev_status.py counts            # rewrite every callout title and type, print what changed
    python3 tools/dev_status.py counts --check    # print only, exit 1 when a title is stale
    python3 tools/dev_status.py show              # print the Today date and items
    python3 tools/dev_status.py rollover          # Today -> Yesterday, Today emptied and dated today

Callouts (one under each heading; the fold sign + / - is kept):
    Today        > [!abstract]+ N items       (results, no boxes, so no state)
    Yesterday    > [!abstract]- N items
    Ongoing ###  > [!todo]- N open            (type from the boxes, as in the LLM Guide's callout rule)
    Ideas        > [!tip]- N ideas
Rollover exit codes: 0 done, 1 Today empty (run /cmd-sync), 2 the day note has no entry (run /cmd-journal first).
Rules: .claude/skills/cmd-sync, cmd-rollover. No git here: vault changes go on a branch and a pull request.
"""
import argparse, datetime, re, sys
from pathlib import Path

VAULT = Path(__file__).resolve().parents[1] / 'Clawd Room Note'
STATUS = VAULT / 'Review' / 'Dev Status.md'
HEAD = re.compile(r'^> \[!(\w+)\]([+-]?) ?(.*)$')
ITEM = re.compile(r'^>\s*- ')
BOX = re.compile(r'^>\s*- \[(.)\]')
OPEN = set(' >/!?')
TODAY_RE = re.compile(r"^## 🚀 Today's Progress \((\d{4}-\d{2}-\d{2})\)\n", re.M)
YEST_RE = re.compile(r"^## 📅 Yesterday's Progress \((\d{4}-\d{2}-\d{2})\)\n", re.M)


def read(p):
    raw = p.read_bytes().decode('utf-8')
    return raw.replace('\r\n', '\n'), '\r\n' in raw


def write(p, text, crlf):
    p.write_bytes((text.replace('\n', '\r\n') if crlf else text).encode('utf-8'))


def state(boxes):
    """The callout type of a block from its boxes (LLM Guide, Conventions: Callout type)."""
    if not boxes:
        return 'note'
    if all(b in 'x-' for b in boxes):
        return 'failure' if all(b == '-' for b in boxes) else 'done'
    return 'warning' if '!' in boxes else 'question' if '?' in boxes else 'todo'


def title(section, body):
    """(type, title) of the callout of one section, from its body lines."""
    items = sum(1 for l in body if ITEM.match(l) and not l.startswith('>     '))
    if section == 'ideas':
        return 'tip', f'{items} ideas'
    if section in ('today', 'yesterday'):
        return 'abstract', f'{items} items'
    boxes = [m.group(1) for m in map(BOX.match, body) if m]
    return state(boxes), f'{sum(1 for b in boxes if b in OPEN)} open'


def counts(check):
    s, crlf = read(STATUS)
    lines, section, ongoing, changed, i = s.split('\n'), None, False, [], 0
    while i < len(lines):
        l = lines[i]
        if l.startswith('## '):
            ongoing = l.startswith('## ⏳ Ongoing')
            section = ('today' if l.startswith("## 🚀 Today's Progress") else
                       'yesterday' if l.startswith("## 📅 Yesterday's Progress") else
                       'ideas' if l.startswith('## 💡 Ideas') else None)
        elif l.startswith('### ') and ongoing:
            section = 'ongoing'
        m = HEAD.match(l)
        if m and section:
            j = i + 1
            while j < len(lines) and lines[j].startswith('>'):
                j += 1
            typ, txt = title(section, lines[i + 1:j])
            new = f'> [!{typ}]{m.group(2)} {txt}'
            if new != l:
                changed.append(f'{l}  ->  {new}')
                lines[i] = new
            i, section = j, None            # one callout per heading
            continue
        i += 1
    for c in changed:
        print(c)
    if check:
        sys.exit(1 if changed else 0)
    if changed:
        write(STATUS, '\n'.join(lines), crlf)
    else:
        print('Counts are current.')


def sections(s):
    t, y = TODAY_RE.search(s), YEST_RE.search(s)
    if not t or not y or y.start() < t.end():
        sys.exit("Dev Status: Today / Yesterday headings with (YYYY-MM-DD) not found")
    return t, y


def day_note(date):
    """Review/Log/YYYY/MM/Www/YYYY-MM-DD.md (the day's own month, its ISO week)."""
    d = datetime.date.fromisoformat(date)
    p = VAULT / 'Review' / 'Log' / f'{d:%Y}' / f'{d:%m}' / f'W{d.isocalendar()[1]:02d}' / f'{date}.md'
    if p.exists():
        return p
    hits = list((VAULT / 'Review' / 'Log').rglob(f'{date}.md'))
    return hits[0] if len(hits) == 1 else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['counts', 'show', 'rollover'])
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--today', default=datetime.date.today().isoformat())
    a = ap.parse_args()
    if a.cmd == 'counts':
        return counts(a.check)

    s, crlf = read(STATUS)
    t, y = sections(s)
    date, body = t.group(1), s[t.end():y.start()].strip('\n')
    items = [l for l in body.split('\n') if ITEM.match(l)]
    if a.cmd == 'show':
        print(f'Today ({date}):\n{body}' if items else f'Today ({date}) is empty')
        return
    if not items:
        print('Today has no items: run /cmd-sync first. Nothing changed.')
        sys.exit(1)
    note = day_note(date)
    if not note or not re.search(r'^## \S', read(note)[0], re.M):
        print(f'The day note {date} has no entry yet: run /cmd-journal {date} first. Nothing changed.')
        sys.exit(2)
    yest = re.sub(r'^> \[!\w+\][+-]?', '> [!abstract]-', body, count=1, flags=re.M)
    end = s.find('\n---', y.end())
    s = (s[:t.start()] + f"## 🚀 Today's Progress ({a.today})\n\n"
         + f"## 📅 Yesterday's Progress ({date})\n" + yest + '\n' + s[end:])
    write(STATUS, s, crlf)
    print(f'[Status] Yesterday = {date}, Today = {a.today}')
    counts(False)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Draw the first-parent line of a branch as a Mermaid gitGraph (standard library only).

  python3 tools/git_graph.py                 # print the graph (main, last 20 entries)
  python3 tools/git_graph.py --last 30       # more entries
  python3 tools/git_graph.py --write "Clawd Room Note/Review/Dev Log.md"
                                             # replace the note's gitGraph block (or what sits
                                             # between <!-- git-graph:start --> and <!-- git-graph:end -->)

A summary of the real shape: each merged pull request is a branch that leaves the
line where it really forked and joins it where it was really merged, so branches
that were open at the same time overlap, as in `git log --graph`. A branch shows
at most three of its commits (first, middle, last) and not the merges of main
into it; runs of direct commits on the line become one commit.
"""
import argparse
import re
import subprocess
import sys

START = "<!-- git-graph:start -->"
END = "<!-- git-graph:end -->"
PR = re.compile(r"^Merge pull request #(\d+) from [^/]+/(\S+)")
SEP = "\x1f"
MAIN, FORK, COMMIT = 0, 1, 2          # order of events that share a place on the line
SHOWN = 3                             # commits drawn per branch


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout


def is_ancestor(a, b):
    return subprocess.run(["git", "merge-base", "--is-ancestor", a, b], capture_output=True).returncode == 0


def clean(text, limit):
    """Label text: no quotes or characters Mermaid reads as syntax, shortened."""
    text = re.sub(r"^\w+(\([^)]*\))?!?:\s*", "", text)          # drop "type(scope): "
    text = re.sub(r'["=:;<>{}\[\]`#]', " ", text)
    text = " ".join(text.split())
    return text if len(text) <= limit else text[:limit - 1].rstrip() + "…"


def main_line(ref):
    """Entries of the first-parent line, oldest first, runs of direct commits joined:
    {"kind": "direct"|"pr", "sha", "date", "ts", "n", "number", "slug", "parents"}."""
    fmt = SEP.join(["%H", "%ad", "%ct", "%s", "%P"])
    out, run = [], None
    for l in git("log", "--first-parent", "--reverse", f"--format={fmt}", "--date=format:%m-%d", ref).splitlines():
        sha, date, ts, subject, parents = l.split(SEP)
        m = PR.match(subject)
        if not m:
            if run is None:
                run = {"kind": "direct", "n": 0}
                out.append(run)
            run.update(sha=sha, date=date, ts=int(ts))   # the run sits at its last commit
            run["n"] += 1
            continue
        run = None
        out.append({"kind": "pr", "sha": sha, "date": date, "ts": int(ts), "parents": parents.split(),
                    "number": m.group(1), "slug": m.group(2).split("/")[-1]})
    return out


def pick(items, k):
    """k items spread over the list, first and last always in."""
    if len(items) <= k:
        return items
    idx = sorted({round(i * (len(items) - 1) / (k - 1)) for i in range(k)})
    return [items[i] for i in idx]


def branch_info(e, line, i):
    """The index of the line entry the branch of pull request line[i] grew from
    (-1: before the line), and its own commits as (timestamp, subject)."""
    first, tip = e["parents"][0], e["parents"][1]
    # where it left the line: the parent of its oldest own commit (merging main
    # into the branch later moves the merge base, not this)
    own = git("log", "--reverse", "--format=%H", f"{first}..{tip}").split()
    base = own[0] + "^" if own else tip
    fork = -1
    for j in range(i - 1, -1, -1):
        if is_ancestor(line[j]["sha"], base):
            fork = j
            break
    commits = []
    for l in git("log", "--no-merges", "--reverse", f"--format=%ct{SEP}%s", f"{first}..{tip}").splitlines():
        t, s = l.split(SEP, 1)
        commits.append((int(t), s))
    if not commits:
        commits = [(e["ts"], git("log", "-1", "--format=%s", tip).strip())]
    return fork, commits


def graph(line, last):
    start = max(0, len(line) - last)
    events, used, labels = [], {"main"}, set()

    def label(text):
        base, k = text, 2
        while text in labels:
            text, k = f"{base} {k}", k + 1
        labels.add(text)
        return text

    for i in range(start, len(line)):
        e = line[i]
        if e["kind"] == "direct":
            text = f"{e['n']} commits ({e['date']})" if e["n"] > 1 else f"commit ({e['date']})"
            events.append(((i, MAIN, 0), ["    checkout main", f'    commit id: "{label(text)}"']))
            continue
        fork, commits = branch_info(e, line, i)
        fork = max(fork, start - 1)               # forked before the window: at its start
        name = re.sub(r"[^A-Za-z0-9_-]", "-", e["slug"])
        base, k = name, 2
        while name in used:
            name, k = f"{base}-{k}", k + 1
        used.add(name)
        events.append(((fork, FORK, i), ["    checkout main", f"    branch {name}"]))
        for t, s in pick(commits, SHOWN):
            # a branch commit sits after the last entry of the line that is older than it
            at = fork
            for j in range(max(fork, start), i):
                if line[j]["ts"] <= t:
                    at = j
            events.append(((at, COMMIT, t), [f"    checkout {name}", f'    commit id: "{label(clean(s, 40))}"']))
        events.append(((i, MAIN, 1), ["    checkout main",
                                      f'    merge {name} id: "{label("#" + e["number"] + " (" + e["date"] + ")")}"']))
    events.sort(key=lambda ev: ev[0])
    lines = ["gitGraph", '    commit id: "earlier history"']
    for _, chunk in events:
        lines += chunk
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default="main", help="the line to draw (default main)")
    ap.add_argument("--last", type=int, default=20, help="how many entries to draw (default 20)")
    ap.add_argument("--write", metavar="NOTE", help="replace the block between the markers in this note")
    a = ap.parse_args()
    line = main_line(a.ref)
    block = "```mermaid\n" + graph(line, a.last) + "\n```"
    if not a.write:
        print(block)
        return
    with open(a.write, encoding="utf-8") as f:
        text = f.read()
    if START in text and END in text:
        head, rest = text.split(START, 1)
        _, tail = rest.split(END, 1)
        text = f"{head}{START}\n{block}\n{END}{tail}"
    else:                                       # no markers: the first gitGraph block
        m = re.search(r"```mermaid\ngitGraph\n.*?\n```", text, re.S)
        if not m:
            sys.exit(f"{a.write}: no gitGraph block and no {START} / {END} lines to write into")
        text = text[:m.start()] + block + text[m.end():]
    with open(a.write, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"{a.write}: graph written ({min(a.last, len(line))} entries)")


if __name__ == "__main__":
    main()

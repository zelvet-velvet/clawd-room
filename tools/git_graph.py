#!/usr/bin/env python3
"""Draw the first-parent line of a branch as a Mermaid gitGraph (standard library only).

  python3 tools/git_graph.py                 # print the graph (main, last 20 entries)
  python3 tools/git_graph.py --last 30       # more entries
  python3 tools/git_graph.py --write "Clawd Room Note/Review/Dev Log.md"
                                             # replace what sits between the two markers
                                             #   <!-- git-graph:start --> ... <!-- git-graph:end -->

A summary, not the real graph: gitGraph can only replay branch / commit / merge.
Each merged pull request becomes one branch with one commit (the tip's subject),
merged back on the line; runs of direct commits on the line become one commit.
"""
import argparse
import re
import subprocess
import sys

START = "<!-- git-graph:start -->"
END = "<!-- git-graph:end -->"
PR = re.compile(r"^Merge pull request #(\d+) from [^/]+/(\S+)")
SEP = "\x1f"


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout


def clean(text, limit):
    """Label text: no quotes or characters Mermaid reads as syntax, shortened."""
    text = re.sub(r"^\w+(\([^)]*\))?!?:\s*", "", text)          # drop "type(scope): "
    text = re.sub(r'["=:;<>{}\[\]`#]', " ", text)
    text = " ".join(text.split())
    return text if len(text) <= limit else text[:limit - 1].rstrip() + "…"


def entries(ref):
    """Oldest first: ("direct", n, date) or ("pr", number, slug, date, the pull request title)."""
    out, run = [], None
    fmt = SEP.join(["%H", "%ad", "%s", "%P"])
    for line in git("log", "--first-parent", "--reverse", f"--format={fmt}", "--date=format:%m-%d", ref).splitlines():
        sha, date, subject, parents = line.split(SEP)
        m = PR.match(subject)
        if not m:
            if run is None:
                run = ["direct", 0, date]
                out.append(run)
            run[1] += 1
            continue
        run = None
        # GitHub puts the pull request's title in the merge commit's body
        tip = git("log", "-1", "--format=%b", sha).strip().splitlines()
        tip = tip[0] if tip else git("log", "-1", "--format=%s", parents.split()[-1]).strip()
        out.append(("pr", m.group(1), m.group(2).split("/")[-1], date, tip))
    return [tuple(e) for e in out]


def graph(items, ref):
    lines = ["gitGraph"]
    used = {"main"}
    if items and items[0][0] == "pr":       # a branch needs a commit to leave from
        lines.append('    commit id: "earlier history"')
    for e in items:
        if e[0] == "direct":
            n, date = e[1], e[2]
            label = f"{n} commits ({date})" if n > 1 else f"commit ({date})"
            lines.append(f'    commit id: "{label}"')
            continue
        _, number, slug, date, tip = e
        name = re.sub(r"[^A-Za-z0-9_-]", "-", slug)
        base, k = name, 2
        while name in used:
            name, k = f"{base}-{k}", k + 1
        used.add(name)
        lines += [f"    branch {name}",
                  f'    commit id: "{clean(tip, 44)}"',
                  "    checkout main",
                  f'    merge {name} id: "#{number} ({date})"']
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default="main", help="the line to draw (default main)")
    ap.add_argument("--last", type=int, default=20, help="how many entries to draw (default 20)")
    ap.add_argument("--write", metavar="NOTE", help="replace the block between the markers in this note")
    a = ap.parse_args()
    items = entries(a.ref)
    skipped = max(0, len(items) - a.last)
    block = "```mermaid\n" + graph(items[-a.last:], a.ref) + "\n```"
    if skipped:
        block = f"_The last {a.last} of {len(items)} entries on the line of `{a.ref}`._\n\n" + block
    if not a.write:
        print(block)
        return
    with open(a.write, encoding="utf-8") as f:
        text = f.read()
    if START not in text or END not in text:
        sys.exit(f"{a.write}: add the lines {START} and {END} where the graph goes")
    head, rest = text.split(START, 1)
    _, tail = rest.split(END, 1)
    with open(a.write, "w", encoding="utf-8") as f:
        f.write(f"{head}{START}\n{block}\n{END}{tail}")
    print(f"{a.write}: graph written ({len(items[-a.last:])} entries)")


if __name__ == "__main__":
    main()

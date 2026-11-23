#!/usr/bin/env python3
"""Builds every tag of pixelforge, times the same filters on the same image,
and draws the history as a chart.

    tools/bench_tags.py [--image images/cat-4k.jpg] [--runs 30] [--rounds 3]
                        [--tags v0 v1 ...] [--out docs/history]

What it does, step by step:
  1. `git archive TAG` into its own directory under --work, so the tag is built
     exactly as it was tagged and the working tree is not touched.
  2. Release build of one program per tag: pixelforge_c_bench (C) for v0,
     pixelforge_bench (C++) for the later tags. Both print the same lines
     ("grayscale  median 6.211 ms ...") for the same two filters.
  3. ROUNDS rounds, each running every tag once (tag after tag, then again), so
     that a busy machine in one minute does not fall on one tag only. Each run
     is the program's own median of RUNS runs.
  4. Per tag and filter: the middle of the rounds' medians, and the lowest and
     highest of them. Written to history.csv, with history.svg as the chart.

Only the Python standard library is needed (no matplotlib).
"""
import argparse
import csv
import datetime
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LINE = re.compile(r"^(\w+)\s+median\s+([\d.]+) ms\s+fastest\s+([\d.]+) ms\s+([\d.]+) GB/s")


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, text=True, capture_output=True, **kw)


def list_tags():
    out = run(["git", "tag", "--list", "v*"], cwd=ROOT).stdout.split()
    return sorted(out, key=lambda t: int(t[1:]))


def build(tag, work):
    src = work / tag / "src"
    bld = work / tag / "build"
    exe_name = "pixelforge_c_bench" if tag == "v0" else "pixelforge_bench"
    exe = next(bld.rglob(exe_name), None) if bld.exists() else None
    if exe and exe.is_file():
        return exe
    shutil.rmtree(work / tag, ignore_errors=True)
    src.mkdir(parents=True)
    archive = subprocess.Popen(["git", "archive", tag], cwd=ROOT, stdout=subprocess.PIPE)
    subprocess.run(["tar", "-x", "-C", str(src)], stdin=archive.stdout, check=True)
    archive.wait()
    run(["cmake", "-S", str(src), "-B", str(bld), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
         "-DPIXELFORGE_BUILD_TESTS=OFF", "-DPIXELFORGE_BUILD_BENCHMARKS=OFF"])
    run(["cmake", "--build", str(bld), "--target", exe_name])
    return next(bld.rglob(exe_name))


def measure(exe, image, runs):
    out = run([str(exe), str(image), str(runs)]).stdout
    return {m[1]: (float(m[2]), float(m[4])) for m in map(LINE.match, out.splitlines()) if m}


def middle(values):
    return sorted(values)[len(values) // 2]


def busiest():
    out = run(["ps", "-Aro", "%cpu,comm"]).stdout.splitlines()[1:4]
    return "; ".join(" ".join(line.split(None, 1)[0:1] + [os.path.basename(line.split(None, 1)[1])[:30]])
                     for line in out)


def chart(rows, tags, filters, path, subtitle):
    """A line chart: one line per filter, the tags along x, milliseconds up."""
    w, h, left, right, top, bottom = 760, 420, 70, 30, 70, 60
    peak = max(r["median"] for r in rows) * 1.15
    colors = {"grayscale": "#2a6fdb", "brightness": "#d9822b"}

    def x(i):
        return left + (w - left - right) * i / max(len(tags) - 1, 1)

    def y(v):
        return top + (h - top - bottom) * (1 - v / peak)

    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" '
             f'font-family="system-ui, sans-serif" font-size="12">',
             "<style>:root{--fg:#222;--bg:#fff;--grid:#ddd}"
             "@media (prefers-color-scheme: dark){:root{--fg:#ddd;--bg:#1b1b1b;--grid:#444}}"
             "text{fill:var(--fg)}</style>",
             f'<rect width="{w}" height="{h}" fill="var(--bg)"/>',
             f'<text x="{left}" y="26" font-size="16" font-weight="600">'
             f"Time of two filters at each tag (3840 x 2160, lower is faster)</text>",
             f'<text x="{left}" y="46" fill-opacity="0.7">{subtitle}</text>']
    for k in range(5):
        v = peak * k / 4
        parts.append(f'<line x1="{left}" x2="{w - right}" y1="{y(v):.1f}" y2="{y(v):.1f}" '
                     f'stroke="var(--grid)"/>')
        parts.append(f'<text x="{left - 8}" y="{y(v) + 4:.1f}" text-anchor="end">{v:.1f} ms</text>')
    for i, tag in enumerate(tags):
        parts.append(f'<text x="{x(i):.1f}" y="{h - bottom + 20}" text-anchor="middle">{tag}</text>')
    for n, name in enumerate(filters):
        pts = [(i, r) for i, tag in enumerate(tags) for r in rows
               if r["tag"] == tag and r["filter"] == name]
        color = colors.get(name, "#777")
        parts.append('<polyline fill="none" stroke-width="2" stroke="%s" points="%s"/>' % (
            color, " ".join(f"{x(i):.1f},{y(r['median']):.1f}" for i, r in pts)))
        for i, r in pts:
            parts.append(f'<line x1="{x(i):.1f}" x2="{x(i):.1f}" y1="{y(r["low"]):.1f}" '
                         f'y2="{y(r["high"]):.1f}" stroke="{color}" stroke-opacity="0.5"/>')
            parts.append(f'<circle cx="{x(i):.1f}" cy="{y(r["median"]):.1f}" r="3.5" fill="{color}"/>')
        lx = left + 150 * n
        parts.append(f'<rect x="{lx}" y="{h - 22}" width="12" height="12" fill="{color}"/>')
        parts.append(f'<text x="{lx + 18}" y="{h - 12}">{name}</text>')
    parts.append(f'<text x="{w - right}" y="{h - 12}" text-anchor="end" fill-opacity="0.7">'
                 f"dot = middle of the rounds, bar = lowest to highest round</text>")
    parts.append("</svg>")
    path.write_text("\n".join(parts) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--image", default=str(ROOT / "images" / "cat-4k.jpg"))
    ap.add_argument("--runs", type=int, default=30)
    ap.add_argument("--rounds", type=int, default=3)
    ap.add_argument("--tags", nargs="*")
    ap.add_argument("--work", default=str(Path.home() / ".cache" / "pixelforge-tags"))
    ap.add_argument("--out", default=str(ROOT / "docs" / "history"))
    args = ap.parse_args()

    tags = args.tags or list_tags()
    work = Path(args.work)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    exes = {}
    for tag in tags:
        print(f"building {tag} ...", flush=True)
        exes[tag] = build(tag, work)

    samples = {}  # (tag, filter) -> [(median ms, GB/s) per round]
    snapshots = []
    for rnd in range(args.rounds):
        snapshots.append(busiest())
        for tag in tags:
            for name, value in measure(exes[tag], args.image, args.runs).items():
                samples.setdefault((tag, name), []).append(value)
            print(f"round {rnd + 1}/{args.rounds} {tag}", flush=True)

    filters = sorted({f for _, f in samples})
    rows = []
    for tag in tags:
        for name in filters:
            got = samples.get((tag, name))
            if not got:
                continue
            times = [t for t, _ in got]
            mid = middle(times)
            rows.append({"tag": tag, "filter": name, "median": mid, "low": min(times),
                         "high": max(times), "gbs": next(g for t, g in got if t == mid)})
    with open(out / "history.csv", "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["tag", "filter", "ms_middle_round", "ms_lowest_round",
                         "ms_highest_round", "gb_per_s"])
        for r in rows:
            writer.writerow([r["tag"], r["filter"], f"{r['median']:.3f}", f"{r['low']:.3f}",
                             f"{r['high']:.3f}", f"{r['gbs']:.1f}"])
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    chart(rows, tags, filters, out / "history.svg",
          f"{stamp}, {args.rounds} rounds of {args.runs} runs, one machine")
    (out / "history.txt").write_text(
        f"measured {stamp}; {args.rounds} rounds x {args.runs} runs of each filter per tag\n"
        f"image: {Path(args.image).name}\n"
        "busiest processes before each round (%cpu, name):\n  " + "\n  ".join(snapshots) + "\n")
    print(f"wrote {out}/history.csv, history.svg, history.txt")


if __name__ == "__main__":
    sys.exit(main())

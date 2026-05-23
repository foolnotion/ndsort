#!/usr/bin/env python3
"""Regenerate BENCHMARKS.md from nanobench CSV output.

Typical workflow
----------------
Build the bench target first, then:

    python3 scripts/generate_benchmarks.py --binary build/bench --data test/data

Save the raw CSV to skip re-running the binary on the next render:

    python3 scripts/generate_benchmarks.py --binary build/bench --save-csv results.csv

Re-render from a saved CSV:

    python3 scripts/generate_benchmarks.py --from-csv results.csv

Output defaults to BENCHMARKS.md in the repo root.
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

SORTERS = [
    "deductive",
    "rank_intersect",
    "merge",
    "hierarchical",
    "best_order",
    "efficient_binary",
    "efficient_sequential",
]

# Markdown display names (escaped underscores for table headers).
DISPLAY = {
    "deductive":            "deductive",
    "rank_intersect":       r"rank\_intersect",
    "merge":                "merge",
    "hierarchical":         "hierarchical",
    "best_order":           r"best\_order",
    "efficient_binary":     r"eff\_binary",
    "efficient_sequential": r"eff\_seq",
}

# Plain names for Mermaid chart x-axis labels.
DISPLAY_PLAIN = {k: v.replace("\\", "") for k, v in DISPLAY.items()}

SYNTHETIC_DISTS = ["random", "linear_front", "sphere_front", "striated"]

DIST_TITLE = {
    "random":        "Random (uniform [0,1]^m)",
    "linear_front":  "Linear front (simplex, DTLZ1-style)",
    "sphere_front":  "Sphere front (unit sphere, DTLZ2-style)",
    "striated":      "Striated (n distinct fronts, adversarial)",
}

LIT_TITLE = {
    "dtlz1": "DTLZ1 — linear Pareto front (hyperplane Σfᵢ = 0.5)",
    "dtlz2": "DTLZ2 — spherical Pareto front",
    "zdt1":  "ZDT1 — convex 2-D front",
    "zdt2":  "ZDT2 — non-convex 2-D front",
}

SORTER_TABLE = """\
| Short name | Algorithm | Complexity (best / worst) | Reference |
|:---|:---|:---|:---|
| `deductive` | Deductive sort | O(MN²) expected / Θ(MN³) | [Mishra & Buzdalov, GECCO 2020](https://doi.org/10.1145/3377930.3390246) |
| `rank_intersect` | Rank-intersect NDS — packed triangular bitsets, SIMD intersection, rank propagation | O(MN log N) avg / O(MN²) | [Burlacu, arXiv 2022](https://arxiv.org/abs/2203.13654) |
| `merge` | Merge NDS (MNDS) | O(N log N) best / O(MN²) | [Moreno et al., IEEE TCYB 2020](https://doi.org/10.1109/TCYB.2020.2968301) |
| `hierarchical` | Hierarchical NDS (HNDS) | O(MN√N) best / O(MN²) | [Bao et al., J. Comput. Sci. 2017](https://doi.org/10.1016/j.jocs.2017.09.015) |
| `best_order` | Best Order Sort (BOS) | O(MN log N) best / O(MN²) | [Roy et al., GECCO 2016](https://doi.org/10.1145/2908961.2931684) |
| `eff_binary` | ENS-BS — efficient NDS, binary search (requires lex-sorted input) | O(MN log N) best / O(MN²) | [Zhang et al., IEEE TEC 2015](https://doi.org/10.1109/TEVC.2014.2308305) |
| `eff_seq` | ENS-SS — efficient NDS, sequential search (requires lex-sorted input) | O(MN√N) best / O(MN²) | [Zhang et al., IEEE TEC 2015](https://doi.org/10.1109/TEVC.2014.2308305) |"""

# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

# "random n=100 m=2  deductive"
_SYNTH_RE = re.compile(
    r"^(random|linear_front|sphere_front|striated)\s+n=(\d+)\s+m=(\d+)\s+(\S+)$"
)
# "csv/dtlz1_n500_m2  deductive"
_CSV_RE = re.compile(r"^csv/([a-z]+\d+)_n(\d+)_m(\d+)\s+(\S+)$")


def parse_csv(text: str) -> dict:
    """Parse nanobench CSV output.

    Returns: {dist: {n: {m: {sorter: {"us": float, "err": float}}}}}
    """
    results: dict = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))

    reader = csv.reader(io.StringIO(text), delimiter=";")
    header = next(reader, None)
    if header is None:
        return results

    try:
        name_col    = header.index("name")
        elapsed_col = header.index("elapsed")   # seconds per sort
        error_col   = header.index("error %")
    except ValueError as exc:
        sys.exit(f"Unexpected CSV header — {exc}\nHeader: {header}")

    for row in reader:
        if not row:
            continue
        name    = row[name_col].strip()
        us      = float(row[elapsed_col]) * 1e6   # s → µs
        err_pct = float(row[error_col])

        m = _SYNTH_RE.match(name)
        if m:
            dist, n, obj, sorter = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4)
            results[dist][n][obj][sorter] = {"us": us, "err": err_pct}
            continue

        m = _CSV_RE.match(name)
        if m:
            dist, n, obj, sorter = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4)
            results[dist][n][obj][sorter] = {"us": us, "err": err_pct}

    return results


# ---------------------------------------------------------------------------
# Table generation
# ---------------------------------------------------------------------------

def make_table(dist_data: dict) -> str:
    """Markdown table for one distribution. All times in µs, winner per row bold."""
    header_cells = ["n", "m"] + [DISPLAY[s] for s in SORTERS]
    sep_cells    = ["--:", "--:"] + ["--:" for _ in SORTERS]

    lines = [
        "| " + " | ".join(header_cells) + " |",
        "| " + " | ".join(sep_cells)    + " |",
    ]

    for n in sorted(dist_data):
        for m in sorted(dist_data[n]):
            row = dist_data[n][m]
            times = {s: row[s]["us"] for s in SORTERS if s in row}
            if not times:
                continue
            best = min(times, key=times.__getitem__)

            cells = [str(n), str(m)]
            for s in SORTERS:
                if s not in times:
                    cells.append("—")
                    continue
                val = f"{times[s]:.1f}"
                if row[s]["err"] > 2.0:
                    val += "\\*"   # unstable measurement
                cells.append(f"**{val}**" if s == best else val)

            lines.append("| " + " | ".join(cells) + " |")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Chart generation
# ---------------------------------------------------------------------------

def _pick_scenario(dist_data: dict) -> tuple[int, int] | None:
    """Highest n; among its m values prefer 5, else the middle one."""
    if not dist_data:
        return None
    best_n = max(dist_data)
    ms = sorted(dist_data[best_n])
    return best_n, (5 if 5 in ms else ms[len(ms) // 2])


def make_chart(dist_data: dict, title: str) -> str:
    """Mermaid speedup-vs-deductive bar chart for the representative scenario."""
    nm = _pick_scenario(dist_data)
    if nm is None:
        return ""
    n, m = nm
    row = dist_data[n][m]
    if "deductive" not in row:
        return ""

    baseline = row["deductive"]["us"]
    labels, speedups = [], []
    for s in SORTERS:
        if s == "deductive" or s not in row:
            continue
        labels.append(f'"{DISPLAY_PLAIN[s]}"')
        speedups.append(round(baseline / row[s]["us"], 1))

    if not speedups:
        return ""

    y_max    = math.ceil(max(speedups) * 1.1) or 2
    x_axis   = ", ".join(labels)
    bar_vals = ", ".join(str(v) for v in speedups)

    return (
        "```mermaid\n"
        "xychart-beta\n"
        f'    title "Speedup vs deductive — {title}, n={n}, m={m}"\n'
        f"    x-axis [{x_axis}]\n"
        f'    y-axis "Speedup (×)" 0 --> {y_max}\n'
        f"    bar [{bar_vals}]\n"
        "```"
    )


# ---------------------------------------------------------------------------
# Markdown assembly
# ---------------------------------------------------------------------------

def build_markdown(results: dict) -> str:
    parts: list[str] = ["""\
# Benchmarks

Times are wall-clock **µs/sort** (one full population ranking).
The fastest sorter per row is **bold**. Entries with >2 % MAPE are marked \\*.

## Environment

<!-- Fill in before committing: CPU, compiler, flags, any relevant notes. -->

## Sorters

""" + SORTER_TABLE + """

## Synthetic benchmarks

Four distributions covering the main cases from the literature
(Jensen 2003; Fortin & Parizeau 2013; Buzdalov & Shalyto 2014):

- **random** — uniform [0,1]^m; typical EA population.
- **linear\\_front** — all points on the (m−1)-simplex (Σfᵢ = 1); DTLZ1-style converged front.
- **sphere\\_front** — all points on the positive unit sphere; DTLZ2-style converged front.
- **striated** — individual i has fⱼ = i for all j; n distinct fronts; \
worst case for O(n·|F₀|) inner loops."""]

    for dist in SYNTHETIC_DISTS:
        if dist not in results:
            continue
        title = DIST_TITLE[dist]
        table = make_table(results[dist])
        chart = make_chart(results[dist], title)
        parts.append(
            f"\n### {title}\n\n*All times in µs.*\n\n{table}"
            + (f"\n\n{chart}" if chart else "")
        )

    lit_keys = sorted(k for k in results if k not in SYNTHETIC_DISTS)
    if lit_keys:
        parts.append(
            "\n## Literature instances (from `test/data/`)\n\n"
            "Generated by `test/data/generate.py` using the standard DTLZ1, DTLZ2, ZDT1, ZDT2\n"
            "formulations (Deb et al. 2002; Zitzler et al. 2000). Each instance uses uniformly\n"
            "sampled decision variables, producing a realistic mix of dominated and non-dominated solutions."
        )
        for dist in lit_keys:
            title = LIT_TITLE.get(dist, dist.upper())
            table = make_table(results[dist])
            chart = make_chart(results[dist], title)
            parts.append(
                f"\n### {title}\n\n*All times in µs.*\n\n{table}"
                + (f"\n\n{chart}" if chart else "")
            )

    return "\n".join(parts) + "\n"


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--binary",   metavar="PATH", help="Path to compiled bench binary")
    src.add_argument("--from-csv", metavar="PATH", help="Load a previously saved CSV")

    ap.add_argument("--data",     metavar="PATH", default="test/data",
                    help="Data directory passed to the binary (default: test/data)")
    ap.add_argument("--save-csv", metavar="PATH",
                    help="Save the raw CSV output for later reuse")
    ap.add_argument("--out",      metavar="PATH", default="BENCHMARKS.md",
                    help="Output file (default: BENCHMARKS.md)")
    args = ap.parse_args()

    if args.from_csv:
        csv_text = Path(args.from_csv).read_text()
    else:
        binary = Path(args.binary)
        if not binary.exists():
            sys.exit(f"Binary not found: {binary}")
        print(f"Running {binary} --csv ...", file=sys.stderr)
        proc = subprocess.run(
            [str(binary), "--csv", args.data],
            capture_output=True, text=True,
        )
        if proc.returncode != 0:
            sys.exit(f"Benchmark failed:\n{proc.stderr}")
        csv_text = proc.stdout
        if args.save_csv:
            Path(args.save_csv).write_text(csv_text)
            print(f"Saved CSV → {args.save_csv}", file=sys.stderr)

    results = parse_csv(csv_text)
    if not results:
        sys.exit("No results parsed — check binary output or CSV file")

    Path(args.out).write_text(build_markdown(results))
    print(f"Written {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()

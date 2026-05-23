#!/usr/bin/env python3
"""Generate NDS benchmark CSV instances.

Problems:
  dtlz1  — linear Pareto front (hyperplane sum fi = 0.5); Deb et al. (2002).
  dtlz2  — spherical Pareto front (unit sphere in positive orthant).
  zdt1   — convex 2-D front; f2 = g*(1 - sqrt(f1/g)).
  zdt2   — non-convex 2-D front; f2 = g*(1 - (f1/g)^2).

Each file: one row per individual, comma-separated objective values, no header.
"""

import csv
import math
import random
from pathlib import Path


# ---- DTLZ1 ------------------------------------------------------------------
# k=5 distance variables; g = 100*(k + sum((xi-0.5)^2 - cos(20π(xi-0.5))))
# Pareto-optimal when g=0 (all distance vars = 0.5); front satisfies Σfi = 0.5.

def dtlz1(n: int, m: int, seed: int = 42) -> list[list[float]]:
    rng = random.Random(seed)
    k = 5
    pop = []
    for _ in range(n):
        xp = [rng.random() for _ in range(m - 1)]   # position vars
        xd = [rng.random() for _ in range(k)]        # distance vars
        g = 100.0 * (k + sum(
            (xi - 0.5) ** 2 - math.cos(20.0 * math.pi * (xi - 0.5))
            for xi in xd
        ))
        f = []
        for i in range(m):
            fi = 0.5 * (1.0 + g)
            upper = m - 1 - i
            for j in range(upper):
                fi *= xp[j]
            if i > 0:
                fi *= (1.0 - xp[upper])
            f.append(fi)
        pop.append(f)
    return pop


# ---- DTLZ2 ------------------------------------------------------------------
# k=10 distance variables; g = sum((xi-0.5)^2)
# Pareto-optimal when g=0; front lies on unit sphere.

def dtlz2(n: int, m: int, seed: int = 42) -> list[list[float]]:
    rng = random.Random(seed)
    k = 10
    pop = []
    for _ in range(n):
        xp = [rng.random() for _ in range(m - 1)]
        xd = [rng.random() for _ in range(k)]
        g = sum((xi - 0.5) ** 2 for xi in xd)
        f = []
        for i in range(m):
            fi = 1.0 + g
            upper = m - 1 - i
            for j in range(upper):
                fi *= math.cos(math.pi / 2.0 * xp[j])
            if i > 0:
                fi *= math.sin(math.pi / 2.0 * xp[upper])
            f.append(fi)
        pop.append(f)
    return pop


# ---- ZDT1 -------------------------------------------------------------------
# n_var=30 decision variables; f1 = x0, g = 1 + 9*sum(x[1:])/29
# f2 = g*(1 - sqrt(f1/g)). Convex Pareto front.

def zdt1(n: int, seed: int = 42) -> list[list[float]]:
    rng = random.Random(seed)
    n_var = 30
    pop = []
    for _ in range(n):
        x = [rng.random() for _ in range(n_var)]
        f1 = x[0]
        g = 1.0 + 9.0 * sum(x[1:]) / (n_var - 1)
        f2 = g * (1.0 - math.sqrt(f1 / g))
        pop.append([f1, f2])
    return pop


# ---- ZDT2 -------------------------------------------------------------------
# Same as ZDT1 but non-convex: f2 = g*(1 - (f1/g)^2).

def zdt2(n: int, seed: int = 42) -> list[list[float]]:
    rng = random.Random(seed)
    n_var = 30
    pop = []
    for _ in range(n):
        x = [rng.random() for _ in range(n_var)]
        f1 = x[0]
        g = 1.0 + 9.0 * sum(x[1:]) / (n_var - 1)
        f2 = g * (1.0 - (f1 / g) ** 2)
        pop.append([f1, f2])
    return pop


# ---- Writer -----------------------------------------------------------------

def write_csv(path: Path, pop: list[list[float]]) -> None:
    with path.open("w", newline="") as fh:
        writer = csv.writer(fh)
        for ind in pop:
            writer.writerow([f"{v:.10g}" for v in ind])


# ---- Main -------------------------------------------------------------------

def main() -> None:
    out = Path(__file__).parent
    sizes = [500, 2000]

    # DTLZ1: m=2,3,5,10
    for n in sizes:
        for m in [2, 3, 5, 10]:
            write_csv(out / f"dtlz1_n{n}_m{m}.csv", dtlz1(n, m))
            print(f"  dtlz1_n{n}_m{m}.csv")

    # DTLZ2: m=2,3,5,10
    for n in sizes:
        for m in [2, 3, 5, 10]:
            write_csv(out / f"dtlz2_n{n}_m{m}.csv", dtlz2(n, m))
            print(f"  dtlz2_n{n}_m{m}.csv")

    # ZDT1/ZDT2: 2-objective only
    for n in sizes:
        write_csv(out / f"zdt1_n{n}_m2.csv", zdt1(n))
        write_csv(out / f"zdt2_n{n}_m2.csv", zdt2(n))
        print(f"  zdt1_n{n}_m2.csv  zdt2_n{n}_m2.csv")


if __name__ == "__main__":
    main()

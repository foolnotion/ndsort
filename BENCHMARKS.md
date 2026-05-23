# Benchmarks

Times are wall-clock **ns/sort** (one full population ranking).
The fastest sorter per row is **bold**.
Entries marked unstable by nanobench (>2% error) are included with their measured value.

## Environment

- **CPU**: AMD Ryzen 9 5950X 16-Core Processor
- **Compiler**: clang (llvmPackages\_21), `-march=x86-64-v3 -O3`
- **Measurement**: [nanobench](https://github.com/martinus/nanobench) — 3 warmup iterations, ≥5 measurement iterations per epoch
- **Note**: CPU frequency scaling was active during this run; relative rankings are reliable but absolute times may vary ±10%.

## Sorters

| Short name | Algorithm |
|:---|:---|
| `deductive` | Deductive sort (baseline O(MN²)) |
| `rank_intersect` | Rank-intersection with SIMD bitset sweeping |
| `merge` | Merge sort–based NDS |
| `hierarchical` | Hierarchical sort (divide-and-conquer) |
| `best_order` | Best-order sort |
| `eff_binary` | ENS-BS — efficient NDS with binary search (requires lex-sorted input) |
| `eff_seq` | ENS-SS — efficient NDS with sequential search (requires lex-sorted input) |

## Synthetic benchmarks

Four distributions covering the main cases from the literature
(Jensen 2003; Fortin & Parizeau 2013; Buzdalov & Shalyto 2014):

- **random** — uniform [0,1]^m; typical EA population.
- **linear\_front** — all points on the (m−1)-simplex (Σfᵢ = 1); DTLZ1-style converged front.
- **sphere\_front** — all points on the positive unit sphere; DTLZ2-style converged front.
- **striated** — individual i has fⱼ = i for all j; produces n distinct fronts; worst case for O(n·|F₀|) inner loops.

### Random (uniform [0,1]^m)

| scenario | deductive | rank\_intersect | merge | hierarchical | best\_order | eff\_binary | eff\_seq |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| n=100 m=2 | 10.8 µs | 8.5 µs | 14.8 µs | 13.1 µs | 13.9 µs | 5.6 µs | **5.5 µs** |
| n=500 m=2 | 399.3 µs | **28.1 µs** | 128.3 µs | 287.9 µs | 98.6 µs | 35.6 µs | 37.9 µs |
| n=1000 m=2 | 1.35 ms | **86.8 µs** | 408.4 µs | 926.0 µs | 340.0 µs | 111.3 µs | 88.6 µs |
| n=100 m=5 | 20.5 µs | 22.0 µs | 23.5 µs | 8.7 µs | 25.8 µs | 9.3 µs | **7.1 µs** |
| n=500 m=5 | 365.1 µs | **65.9 µs** | 94.4 µs | 322.1 µs | 177.7 µs | 206.6 µs | 166.7 µs |
| n=1000 m=5 | 1.43 ms | **159.2 µs** | 253.3 µs | 1.20 ms | 566.7 µs | 742.9 µs | 633.8 µs |
| n=100 m=10 | 40.5 µs | 42.9 µs | 45.2 µs | 14.6 µs | 47.5 µs | 11.9 µs | **10.8 µs** |
| n=500 m=10 | 937.4 µs | **132.6 µs** | 166.5 µs | 751.8 µs | 310.7 µs | 542.0 µs | 535.1 µs |
| n=1000 m=10 | 3.60 ms | **348.0 µs** | 363.6 µs | 2.67 ms | 924.4 µs | 2.15 ms | 2.12 ms |

### Linear front (simplex, DTLZ1-style)

| scenario | deductive | rank\_intersect | merge | hierarchical | best\_order | eff\_binary | eff\_seq |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| n=100 m=2 | 15.0 µs | **6.5 µs** | 9.2 µs | 13.1 µs | 12.0 µs | 8.7 µs | 8.4 µs |
| n=500 m=2 | 353.6 µs | **18.5 µs** | 32.6 µs | 288.4 µs | 93.8 µs | 127.8 µs | 128.0 µs |
| n=1000 m=2 | 1.41 ms | **39.7 µs** | 65.4 µs | 1.15 ms | 326.4 µs | 472.3 µs | 450.7 µs |
| n=100 m=5 | 30.6 µs | 22.9 µs | 25.0 µs | 14.8 µs | 27.0 µs | 11.1 µs | **10.1 µs** |
| n=500 m=5 | 730.7 µs | **66.0 µs** | 87.2 µs | 643.3 µs | 264.8 µs | 389.0 µs | 381.6 µs |
| n=1000 m=5 | 2.90 ms | **135.3 µs** | 200.3 µs | 2.61 ms | 1.04 ms | 1.72 ms | 1.74 ms |
| n=100 m=10 | 42.4 µs | 46.4 µs | 44.5 µs | 14.5 µs | 50.1 µs | 11.8 µs | **11.4 µs** |
| n=500 m=10 | 1.04 ms | **130.3 µs** | 169.4 µs | 770.9 µs | 291.2 µs | 531.5 µs | 535.4 µs |
| n=1000 m=10 | 4.23 ms | **303.2 µs** | 365.2 µs | 3.12 ms | 951.0 µs | 2.30 ms | 2.42 ms |

### Sphere front (unit sphere, DTLZ2-style)

| scenario | deductive | rank\_intersect | merge | hierarchical | best\_order | eff\_binary | eff\_seq |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| n=100 m=2 | 15.4 µs | **5.7 µs** | 8.9 µs | 13.0 µs | 11.8 µs | 8.6 µs | 8.5 µs |
| n=500 m=2 | 351.2 µs | **19.7 µs** | 34.1 µs | 288.5 µs | 94.6 µs | 128.2 µs | 127.8 µs |
| n=1000 m=2 | 1.45 ms | **41.9 µs** | 66.4 µs | 1.16 ms | 335.5 µs | 487.0 µs | 482.6 µs |
| n=100 m=5 | 30.0 µs | 21.1 µs | 23.2 µs | 14.3 µs | 24.8 µs | 10.5 µs | **9.9 µs** |
| n=500 m=5 | 745.3 µs | **63.9 µs** | 82.1 µs | 632.5 µs | 279.9 µs | 388.4 µs | 372.3 µs |
| n=1000 m=5 | 2.91 ms | **120.8 µs** | 191.3 µs | 2.59 ms | 1.07 ms | 1.75 ms | 1.78 ms |
| n=100 m=10 | 41.9 µs | 44.3 µs | 43.9 µs | 14.8 µs | 50.4 µs | 11.6 µs | **10.7 µs** |
| n=500 m=10 | 1.05 ms | **134.2 µs** | 166.4 µs | 767.8 µs | 291.5 µs | 528.0 µs | 535.0 µs |
| n=1000 m=10 | 4.20 ms | **293.0 µs** | 372.3 µs | 3.11 ms | 950.8 µs | 2.26 ms | 2.37 ms |

### Striated (n fronts, adversarial)

| scenario | deductive | rank\_intersect | merge | hierarchical | best\_order | eff\_binary | eff\_seq |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| n=100 m=2 | 34.5 µs | 13.4 µs | 20.2 µs | 50.9 µs | 23.0 µs | **6.8 µs** | 10.7 µs |
| n=500 m=2 | 521.9 µs | 119.5 µs | 259.1 µs | 1.31 ms | 253.1 µs | **35.9 µs** | 141.4 µs |
| n=1000 m=2 | 2.06 ms | 393.8 µs | 936.5 µs | 5.41 ms | 918.7 µs | **74.7 µs** | 526.9 µs |
| n=100 m=5 | 40.2 µs | 28.5 µs | 20.6 µs | 55.1 µs | 45.8 µs | **6.6 µs** | 14.4 µs |
| n=500 m=5 | 913.2 µs | 154.8 µs | 261.7 µs | 1.43 ms | 330.1 µs | **39.1 µs** | 274.4 µs |
| n=1000 m=5 | 3.59 ms | 449.0 µs | 943.7 µs | 5.71 ms | 1.10 ms | **83.7 µs** | 1.05 ms |
| n=100 m=10 | 52.6 µs | 53.1 µs | 21.0 µs | 63.1 µs | 84.1 µs | **7.9 µs** | 22.2 µs |
| n=500 m=10 | 1.22 ms | 214.8 µs | 266.3 µs | 1.65 ms | 459.9 µs | **47.3 µs** | 487.1 µs |
| n=1000 m=10 | 4.85 ms | 559.1 µs | 971.6 µs | 6.67 ms | 1.33 ms | **98.7 µs** | 2.01 ms |

## Literature instances (from `test/data/`)

Generated by `test/data/generate.py` using the standard DTLZ1, DTLZ2, ZDT1, ZDT2
formulations (Deb et al. 2002; Zitzler et al. 2000). Each instance uses uniformly
sampled decision variables, producing a realistic mix of dominated and non-dominated solutions.

### DTLZ1 — linear Pareto front (hyperplane Σfᵢ = 0.5)

| scenario | deductive | rank\_intersect | merge | hierarchical | best\_order | eff\_binary | eff\_seq |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| n=500 m=2 | 387.3 µs | **28.8 µs** | 90.4 µs | 241.1 µs | 102.8 µs | 41.6 µs | 35.2 µs |
| n=2000 m=2 | 4.62 ms | 301.6 µs | 810.9 µs | 3.14 ms | 1.22 ms | 328.3 µs | **241.6 µs** |
| n=500 m=3 | 322.6 µs | **39.9 µs** | 72.9 µs | 261.9 µs | 121.7 µs | 95.0 µs | 66.6 µs |
| n=2000 m=3 | 3.78 ms | **290.8 µs** | 701.3 µs | 3.25 ms | 1.55 ms | 1.03 ms | 771.2 µs |
| n=500 m=5 | 416.9 µs | **72.2 µs** | 102.0 µs | 335.4 µs | 243.8 µs | 293.0 µs | 238.7 µs |
| n=2000 m=5 | 5.12 ms | **411.0 µs** | 662.6 µs | 4.18 ms | 2.47 ms | 3.70 ms | 2.90 ms |
| n=500 m=10 | 736.8 µs | **148.3 µs** | 195.5 µs | 565.3 µs | 397.8 µs | 669.2 µs | 580.3 µs |
| n=2000 m=10 | 9.58 ms | **766.7 µs** | 958.3 µs | 6.53 ms | 3.40 ms | 7.40 ms | 6.31 ms |

### DTLZ2 — spherical Pareto front

| scenario | deductive | rank\_intersect | merge | hierarchical | best\_order | eff\_binary | eff\_seq |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| n=500 m=2 | 404.1 µs | **24.8 µs** | 72.0 µs | 223.6 µs | 88.5 µs | 44.5 µs | 34.6 µs |
| n=2000 m=2 | 4.59 ms | 269.4 µs | 673.5 µs | 3.13 ms | 971.1 µs | 356.5 µs | **247.2 µs** |
| n=500 m=3 | 334.9 µs | **36.8 µs** | 61.0 µs | 211.1 µs | 118.8 µs | 104.2 µs | 68.2 µs |
| n=2000 m=3 | 3.73 ms | **288.4 µs** | 540.3 µs | 2.67 ms | 1.38 ms | 1.17 ms | 788.8 µs |
| n=500 m=5 | 424.5 µs | **69.3 µs** | 92.2 µs | 291.9 µs | 222.5 µs | 312.2 µs | 224.8 µs |
| n=2000 m=5 | 5.11 ms | **400.7 µs** | 577.0 µs | 3.53 ms | 2.50 ms | 4.47 ms | 2.88 ms |
| n=500 m=10 | 588.1 µs | **140.0 µs** | 192.1 µs | 448.1 µs | 394.4 µs | 675.3 µs | 532.5 µs |
| n=2000 m=10 | 7.71 ms | **784.1 µs** | 917.3 µs | 5.58 ms | 3.96 ms | 8.01 ms | 6.46 ms |

### ZDT1 — convex 2-D front

| scenario | deductive | rank\_intersect | merge | hierarchical | best\_order | eff\_binary | eff\_seq |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| n=500 m=2 | 378.2 µs | **25.2 µs** | 72.3 µs | 244.6 µs | 96.6 µs | 44.1 µs | 34.2 µs |
| n=2000 m=2 | 4.40 ms | 269.3 µs | 658.3 µs | 3.10 ms | 1.21 ms | 353.6 µs | **246.7 µs** |

### ZDT2 — non-convex 2-D front

| scenario | deductive | rank\_intersect | merge | hierarchical | best\_order | eff\_binary | eff\_seq |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| n=500 m=2 | 396.4 µs | **27.8 µs** | 117.2 µs | 270.7 µs | 104.2 µs | 36.0 µs | 36.9 µs |
| n=2000 m=2 | 4.52 ms | 332.0 µs | 1.27 ms | 3.14 ms | 1.18 ms | 300.7 µs | **257.0 µs** |

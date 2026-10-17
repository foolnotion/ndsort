"""Post-build smoke test run by cibuildwheel against the installed wheel.

Not collected by pytest (the name does not match ``test_*.py``).  It runs
outside the source tree, so ``import ndsort`` resolves to the installed wheel.
"""

import numpy as np

import ndsort


def fronts_as_lists(fronts):
    return [sorted(int(i) for i in front) for front in fronts]


def main():
    # (population, expected fronts as sorted index lists)
    cases = [
        (np.array([[0.0, 0.0], [1.0, 1.0]]), [[0], [1]]),
        (np.array([[0.0, 1.0], [1.0, 0.0], [1.0, 1.0]]), [[0, 1], [2]]),
    ]
    # "rank_intersect" is the default method; "rank_ordinal" uses the AVX2 path.
    for method in ("rank_intersect", "rank_ordinal"):
        for population, expected in cases:
            actual = fronts_as_lists(ndsort.sort(population, method=method))
            assert actual == expected, f"{method}: expected {expected}, got {actual}"
    print(f"ndsort smoke test passed ({ndsort.__file__})")


if __name__ == "__main__":
    main()

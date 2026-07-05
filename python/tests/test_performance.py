"""Performance tests for the ndsort Python bindings.

Two goals:
  1. Regression benchmarks — track throughput over time with pytest-benchmark.
  2. Overhead bounds — assert that binding plumbing does not dominate sort time.

Overhead methodology
--------------------
We cannot call the C++ library directly without going through the binding, so we
approximate the binding overhead as follows:

  * A population of n=1 exercises the full call path (Python dispatch, ndarray
    cast, lazy-view construction, flatten, sorting a single point, numpy output)
    but the *algorithm* itself does O(1) work.  The measured time is therefore
    dominated by binding plumbing (T_bind).

  * A population of n=N_LARGE exercises the same plumbing *plus* O(f(N)) sorting
    work.  The total time T_total ≈ T_bind + T_algo.

  * We assert: T_bind / T_total < OVERHEAD_FRACTION
    i.e. the binding plumbing is at most OVERHEAD_FRACTION of the total call time
    for populations of N_LARGE individuals.

We also assert that the numpy zero-copy path is never slower than the
list-of-lists path (which forces an extra n×m copy).

Run the benchmark suite:
    pytest tests/test_performance.py -v --benchmark-sort=name

Run only the overhead assertions (no pytest-benchmark required):
    pytest tests/test_performance.py -m "not benchmark" -v
"""

from __future__ import annotations

import timeit

import numpy as np
import pytest

import ndsort

from conftest import (
    ALL_METHODS,
    make_linear_front,
    make_random,
    make_striated,
)

# ── Tunable constants ──────────────────────────────────────────────────────────

# For populations this large the algorithm time must dominate the binding overhead.
N_LARGE = 2_000

# Binding overhead must be less than this fraction of total sort time for N_LARGE.
# 5 % is a very conservative bound given the zero-copy design.
OVERHEAD_FRACTION = 0.05

# Numpy path must not be slower than list path by more than this margin.
# 5 % headroom absorbs timer noise on heavily loaded CI machines.
NUMPY_VS_LIST_MARGIN = 0.05

# Number of timing repetitions for the non-benchmark overhead tests.
_TIMEIT_REPS = 7
_TIMEIT_NUMBER = 10


def _measure(fn, *, number: int = _TIMEIT_NUMBER, reps: int = _TIMEIT_REPS) -> float:
    """Return the minimum per-call time (seconds) over `reps` repetitions."""
    return min(timeit.repeat(fn, number=number, repeat=reps)) / number


# ── pytest-benchmark throughput tests ─────────────────────────────────────────
# These tests require pytest-benchmark (`pip install pytest-benchmark`).
# They produce no pass/fail assertion; their purpose is regression tracking.


@pytest.fixture(params=ALL_METHODS)
def method(request):
    return request.param


@pytest.fixture(
    params=[
        pytest.param(("random",      2), id="random-m2"),
        pytest.param(("random",      5), id="random-m5"),
        pytest.param(("random",     10), id="random-m10"),
        pytest.param(("linear",      5), id="linear-front-m5"),
        pytest.param(("sphere",      5), id="sphere-front-m5"),
        pytest.param(("striated",    5), id="striated-m5"),
    ]
)
def population_2000(request):
    kind, m = request.param
    n = 2_000
    if kind == "random":
        return make_random(n, m)
    if kind == "linear":
        return make_linear_front(n, m)
    if kind == "sphere":
        from conftest import make_sphere_front
        return make_sphere_front(n, m)
    # striated
    return make_striated(n, m)


@pytest.mark.benchmark(group="throughput")
def test_throughput(benchmark, method, population_2000):
    """Measure sort throughput for each algorithm × population type.

    No assertion — results are captured by pytest-benchmark for regression
    tracking (use --benchmark-compare across commits).
    """
    result = benchmark(ndsort.sort, population_2000, method=method)
    # Sanity-check: result must be a non-empty partition.
    assert len(result) >= 1
    total = sum(len(f) for f in result)
    assert total == len(population_2000)


@pytest.mark.benchmark(group="numpy-vs-list")
@pytest.mark.parametrize("input_type", ["numpy", "list"])
def test_benchmark_input_type(benchmark, input_type):
    """Benchmark numpy array vs list-of-lists input for the same population.

    Run with --benchmark-compare to see the speedup from the zero-copy path.
    """
    pop_np = make_random(N_LARGE, 5)
    pop = pop_np if input_type == "numpy" else pop_np.tolist()
    benchmark(ndsort.sort, pop, method="rank_intersect")


# ── Overhead bound (no pytest-benchmark required) ─────────────────────────────

@pytest.mark.parametrize("method", ALL_METHODS)
def test_binding_overhead_fraction(method):
    """Binding plumbing must account for less than OVERHEAD_FRACTION of total time.

    T_bind is approximated by timing a single-individual sort (algorithm is O(1)).
    T_total is timed for N_LARGE individuals.
    """
    single = np.array([[0.5, 0.3, 0.7, 0.2, 0.9]])
    large  = make_random(N_LARGE, 5, seed=42)

    # Warm up (JIT, branch predictor, TLB).
    for _ in range(3):
        ndsort.sort(single, method=method)
        ndsort.sort(large,  method=method)

    t_bind  = _measure(lambda: ndsort.sort(single, method=method))
    t_total = _measure(lambda: ndsort.sort(large,  method=method))

    overhead = t_bind / t_total
    assert overhead < OVERHEAD_FRACTION, (
        f"[{method}] binding overhead {overhead*100:.1f}% "
        f"(T_bind={t_bind*1e6:.0f}µs, T_total={t_total*1e3:.2f}ms) "
        f"exceeds {OVERHEAD_FRACTION*100:.0f}% threshold"
    )


# ── Numpy faster than list ─────────────────────────────────────────────────────

@pytest.mark.parametrize("method", ALL_METHODS)
def test_numpy_input_not_slower_than_list(method):
    """The zero-copy numpy path must not be slower than the list-of-lists path.

    For N_LARGE the extra copy (n×m doubles, ~160 KB for n=2000, m=10) is
    measurable.  We allow NUMPY_VS_LIST_MARGIN to absorb timer noise.
    """
    pop_np   = make_random(N_LARGE, 10, seed=7)
    pop_list = pop_np.tolist()

    # Warm up.
    ndsort.sort(pop_np,   method=method)
    ndsort.sort(pop_list, method=method)

    t_numpy = _measure(lambda: ndsort.sort(pop_np,   method=method))
    t_list  = _measure(lambda: ndsort.sort(pop_list, method=method))

    assert t_numpy <= t_list * (1 + NUMPY_VS_LIST_MARGIN), (
        f"[{method}] numpy ({t_numpy*1e3:.2f}ms) is more than "
        f"{NUMPY_VS_LIST_MARGIN*100:.0f}% slower than list ({t_list*1e3:.2f}ms); "
        "the zero-copy input path may be broken"
    )


# ── Presorted / sorted_unique hints are faster ────────────────────────────────

@pytest.mark.parametrize("method", ALL_METHODS)
def test_presorted_hint_not_slower_than_default(method):
    """The presorted hint skips an internal lex-sort and must not be slower."""
    pop = make_random(N_LARGE, 5, seed=99)
    pop_sorted = pop[np.lexsort(pop[:, ::-1].T)]

    # Warm up.
    ndsort.sort(pop_sorted, method=method)
    ndsort.sort(pop_sorted, method=method, hint=ndsort.SortHint.presorted)

    t_default  = _measure(lambda: ndsort.sort(pop_sorted, method=method))
    t_presorted = _measure(lambda: ndsort.sort(pop_sorted, method=method,
                                               hint=ndsort.SortHint.presorted))

    # Allow 5 % margin; the internal sort is not the dominant cost for all algorithms.
    assert t_presorted <= t_default * 1.05, (
        f"[{method}] presorted hint ({t_presorted*1e3:.2f}ms) is slower than "
        f"default ({t_default*1e3:.2f}ms)"
    )


@pytest.mark.parametrize("method", ALL_METHODS)
def test_sorted_unique_hint_not_slower_than_presorted(method):
    """sorted_unique additionally skips eps-dedup and must not be slower than presorted."""
    pop = make_random(N_LARGE, 5, seed=99)
    pop_sorted = pop[np.lexsort(pop[:, ::-1].T)]

    ndsort.sort(pop_sorted, method=method, hint=ndsort.SortHint.presorted)
    ndsort.sort(pop_sorted, method=method, hint=ndsort.SortHint.sorted_unique)

    t_presorted   = _measure(lambda: ndsort.sort(pop_sorted, method=method,
                                                 hint=ndsort.SortHint.presorted))
    t_sorted_unique = _measure(lambda: ndsort.sort(pop_sorted, method=method,
                                                   hint=ndsort.SortHint.sorted_unique))

    assert t_sorted_unique <= t_presorted * 1.05, (
        f"[{method}] sorted_unique ({t_sorted_unique*1e3:.2f}ms) is slower than "
        f"presorted ({t_presorted*1e3:.2f}ms)"
    )


# ── Scaling: algorithm-dominated, not overhead-dominated ──────────────────────

@pytest.mark.parametrize("method", ["rank_intersect", "deductive"])
def test_sort_time_scales_with_n_not_overhead(method):
    """For large N the sort time must grow with N (algorithm dominates overhead).

    If binding overhead dominated, time(2N) / time(N) would approach 1.
    We require the ratio to be > 1.5 — any realistic sorting algorithm on random
    data scales at least linearly, so 1.5× is a conservative lower bound.
    """
    n_small = 500
    n_large = 2_000

    pop_small = make_random(n_small, 5, seed=0)
    pop_large = make_random(n_large, 5, seed=0)

    ndsort.sort(pop_small, method=method)
    ndsort.sort(pop_large, method=method)

    t_small = _measure(lambda: ndsort.sort(pop_small, method=method))
    t_large = _measure(lambda: ndsort.sort(pop_large, method=method))

    ratio = t_large / t_small
    assert ratio > 1.5, (
        f"[{method}] time ratio t({n_large})/t({n_small}) = {ratio:.2f} < 1.5; "
        "binding overhead appears to dominate algorithm time"
    )

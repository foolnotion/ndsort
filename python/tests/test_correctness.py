"""Correctness tests for the ndsort Python bindings.

These tests mirror the C++ Catch2 suite (test/source/ndsort_test.cpp) and add
Python-specific checks (numpy-vs-list consistency, return types, hint modes).
"""

from __future__ import annotations

import csv
from pathlib import Path

import numpy as np
import pytest

import ndsort

from conftest import (
    ALL_METHODS,
    DATA_DIR,
    indices_in_fronts,
    make_linear_front,
    make_random,
    make_striated,
    normalize_fronts,
)


# ── Helpers ────────────────────────────────────────────────────────────────────

def reference_sort(pop: np.ndarray, eps: float = 0.0) -> list[list[int]]:
    """Deductive sort is the canonical reference (exact, correct by construction)."""
    return normalize_fronts(ndsort.sort(pop, method="deductive", eps=eps))


def load_csv(path: Path) -> np.ndarray:
    with path.open() as fh:
        rows = [[float(v) for v in row] for row in csv.reader(fh) if row]
    return np.array(rows)


# ── Trivial cases (mirror C++ "deductive sorter - trivial cases") ──────────────

def test_trivial_single_individual():
    pop = np.array([[1.0, 2.0]])
    fronts = ndsort.sort(pop)
    assert len(fronts) == 1
    assert list(fronts[0]) == [0]


def test_trivial_two_nondominating():
    pop = np.array([[0.0, 1.0], [1.0, 0.0]])
    fronts = ndsort.sort(pop)
    assert len(fronts) == 1
    assert len(fronts[0]) == 2


def test_trivial_one_dominates():
    pop = np.array([[0.0, 0.0], [1.0, 1.0]])
    fronts = ndsort.sort(pop)
    assert len(fronts) == 2
    assert list(fronts[0]) == [0]
    assert list(fronts[1]) == [1]


# ── Edge cases ────────────────────────────────────────────────────────────────

def test_edge_empty_population():
    pop = np.empty((0, 3), dtype=np.float64)
    fronts = ndsort.sort(pop)
    assert fronts == []


def test_edge_single_objective():
    """m=1: strict total order, each individual in its own front."""
    pop = np.array([[3.0], [1.0], [2.0]])
    fronts = ndsort.sort(pop)
    assert len(fronts) == 3
    for f in fronts:
        assert len(f) == 1


def test_edge_large_m_fallback():
    """m=9 exceeds dispatch_on_m's compile-time cases; exercises the runtime path."""
    pop = make_random(100, 9, seed=42)
    ref = reference_sort(pop)
    for method in ALL_METHODS:
        assert normalize_fronts(ndsort.sort(pop, method=method)) == ref, method


# ── Partition property ─────────────────────────────────────────────────────────

@pytest.mark.parametrize("n,m,seed", [
    (100, 2, 7),
    (200, 5, 8),
    (50,  8, 9),
])
def test_partition_every_index_exactly_once(n, m, seed):
    """Every individual must appear in exactly one front."""
    pop = make_random(n, m, seed)
    for method in ALL_METHODS:
        fronts = ndsort.sort(pop, method=method)
        assert indices_in_fronts(fronts) == list(range(n)), method


# ── All methods agree with deductive reference ────────────────────────────────

@pytest.mark.parametrize("n,m,seed", [
    (100, 2, 42),
    (500, 2,  1),
    (200, 3,  1),
    (200, 5,  2),
    (500, 3,  3),
    (500, 5,  4),
])
@pytest.mark.parametrize("method", [m for m in ALL_METHODS if m != "deductive"])
def test_all_methods_agree_with_deductive(method, n, m, seed):
    pop = make_random(n, m, seed)
    ref = reference_sort(pop)
    assert normalize_fronts(ndsort.sort(pop, method=method)) == ref


# ── Eps-deduplication ─────────────────────────────────────────────────────────

@pytest.mark.parametrize("method", ALL_METHODS)
def test_eps_dedup_near_duplicates_same_front(method):
    """{0,0} and {0.005,0.005} are eps-close (eps=0.01); both should dominate {1,1}."""
    pop = np.array([[0.0, 0.0], [0.005, 0.005], [1.0, 1.0]])
    fronts = ndsort.sort(pop, method=method, eps=0.01)
    assert len(fronts) == 2, method
    assert len(fronts[0]) == 2, method


# ── Numpy array input == list-of-lists input ──────────────────────────────────

@pytest.mark.parametrize("n,m", [(50, 2), (200, 5), (100, 10)])
@pytest.mark.parametrize("method", ALL_METHODS)
def test_numpy_and_list_give_identical_results(method, n, m):
    """Zero-copy numpy path must produce exactly the same fronts as list-of-lists."""
    pop_np = make_random(n, m, seed=123)
    pop_list = pop_np.tolist()

    fronts_np   = normalize_fronts(ndsort.sort(pop_np,   method=method))
    fronts_list = normalize_fronts(ndsort.sort(pop_list, method=method))
    assert fronts_np == fronts_list, method


# ── Hint: presorted ───────────────────────────────────────────────────────────

@pytest.mark.parametrize("method", ALL_METHODS)
def test_hint_presorted_gives_same_result(method):
    """presorted hint must produce the same fronts as the default for lex-sorted input."""
    pop = make_random(200, 3, seed=55)
    pop_sorted = pop[np.lexsort(pop[:, ::-1].T)]  # lexicographic row sort

    ref    = normalize_fronts(ndsort.sort(pop_sorted, method=method))
    hinted = normalize_fronts(ndsort.sort(pop_sorted, method=method,
                                          hint=ndsort.SortHint.presorted))
    assert hinted == ref, method


@pytest.mark.parametrize("method", ALL_METHODS)
def test_hint_presorted_with_duplicates(method):
    """presorted: exact duplicates must land in the same front."""
    pop = np.array([[0.0, 0.0], [0.0, 0.0], [1.0, 1.0]])
    fronts = ndsort.sort(pop, method=method, hint=ndsort.SortHint.presorted)
    assert len(fronts) == 2, method
    assert len(fronts[0]) == 2, method
    assert len(fronts[1]) == 1, method


# ── Hint: sorted_unique ───────────────────────────────────────────────────────

@pytest.mark.parametrize("method", ALL_METHODS)
def test_hint_sorted_unique_gives_same_result(method):
    """sorted_unique hint must produce the same fronts as the default."""
    pop = make_random(200, 3, seed=55)
    pop_sorted = pop[np.lexsort(pop[:, ::-1].T)]

    ref    = normalize_fronts(ndsort.sort(pop_sorted, method=method))
    hinted = normalize_fronts(ndsort.sort(pop_sorted, method=method,
                                          hint=ndsort.SortHint.sorted_unique))
    assert hinted == ref, method


def test_hint_sorted_unique_skips_dedup():
    """sorted_unique skips eps-dedup: near-duplicates are treated as distinct individuals."""
    # With default sort + eps=0.01: {0,0} and {0.005,0.005} are eps-merged → 2 fronts.
    # With sorted_unique  + eps=0.01: dedup is skipped; neither eps-dominates the other
    #   (|0.005 - 0| = 0.005 < eps on both objectives) but they ARE separate individuals,
    #   so they both dominate {1,1}.  Result: 2 fronts, front 0 has both.
    pop = np.array([[0.0, 0.0], [0.005, 0.005], [1.0, 1.0]])

    f_su = normalize_fronts(
        ndsort.sort(pop, method="deductive", eps=0.01,
                    hint=ndsort.SortHint.sorted_unique)
    )
    assert len(f_su) == 2
    assert len(f_su[0]) == 2


# ── pareto_compare ────────────────────────────────────────────────────────────

def test_pareto_compare_equal():
    a = np.array([1.0, 2.0])
    assert ndsort.pareto_compare(a, a) == ndsort.Dominance.equal


def test_pareto_compare_left_dominates():
    a = np.array([0.0, 0.0])
    b = np.array([1.0, 1.0])
    assert ndsort.pareto_compare(a, b) == ndsort.Dominance.left


def test_pareto_compare_right_dominates():
    a = np.array([1.0, 1.0])
    b = np.array([0.0, 0.0])
    assert ndsort.pareto_compare(a, b) == ndsort.Dominance.right


def test_pareto_compare_none():
    a = np.array([0.0, 1.0])
    b = np.array([1.0, 0.0])
    assert ndsort.pareto_compare(a, b) == ndsort.Dominance.none


def test_pareto_compare_eps():
    a = np.array([0.0, 0.0])
    b = np.array([0.005, 0.005])
    # Without eps: a dominates b.
    assert ndsort.pareto_compare(a, b) == ndsort.Dominance.left
    # With eps=0.01: differences are within eps on all objectives → equal.
    assert ndsort.pareto_compare(a, b, eps=0.01) == ndsort.Dominance.equal


def test_pareto_compare_numpy_and_list_agree():
    """Numpy overload (zero-copy) and list overload must return the same result."""
    a_np = np.array([0.5, 0.3])
    b_np = np.array([0.4, 0.6])
    assert ndsort.pareto_compare(a_np, b_np) == ndsort.pareto_compare(
        a_np.tolist(), b_np.tolist()
    )


# ── Return type ───────────────────────────────────────────────────────────────

def test_return_type_is_list_of_numpy_arrays():
    """sort() must return list[np.ndarray], not list[list[int]]."""
    pop = make_random(100, 3)
    fronts = ndsort.sort(pop)
    assert isinstance(fronts, list)
    for f in fronts:
        assert isinstance(f, np.ndarray), type(f)
        assert f.dtype == np.dtype("uint64") or np.issubdtype(f.dtype, np.unsignedinteger)


def test_front_indices_are_valid_references():
    """All indices in the fronts must be valid row indices into the population."""
    n, m = 300, 4
    pop = make_random(n, m)
    fronts = ndsort.sort(pop)
    all_indices = [int(i) for f in fronts for i in f]
    assert all(0 <= i < n for i in all_indices)


# ── CSV consistency ───────────────────────────────────────────────────────────

@pytest.mark.slow
@pytest.mark.parametrize("csv_path", sorted(DATA_DIR.glob("*.csv")),
                          ids=lambda p: p.stem)
def test_all_methods_agree_on_csv_data(csv_path):
    """All algorithms must agree with the deductive reference on the shared CSV populations."""
    pop = load_csv(csv_path)
    ref = reference_sort(pop)
    for method in [m for m in ALL_METHODS if m != "deductive"]:
        result = normalize_fronts(ndsort.sort(pop, method=method))
        assert result == ref, f"{method} disagrees on {csv_path.name}"

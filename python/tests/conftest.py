"""Shared fixtures and helpers for the ndsort Python test suite."""

from __future__ import annotations

import math
from pathlib import Path

import numpy as np
import pytest

try:
    import ndsort
except ImportError:
    pytest.skip("ndsort extension not built — run cmake -DNDSORT_PYTHON=ON first",
                allow_module_level=True)

# ── Constants ──────────────────────────────────────────────────────────────────

# Path to the shared CSV populations used by the C++ benchmark / tests.
DATA_DIR = Path(__file__).parents[2] / "test" / "data"

ALL_METHODS: list[str] = list(ndsort._methods.keys())


# ── Population generators ──────────────────────────────────────────────────────

def make_random(n: int, m: int, seed: int = 42) -> np.ndarray:
    """Uniform random population in [0, 1]^m."""
    return np.random.default_rng(seed).random((n, m))


def make_linear_front(n: int, m: int, seed: int = 42) -> np.ndarray:
    """Points on the (m-1)-simplex (DTLZ1-style linear Pareto front).

    All individuals are mutually non-dominating — a stress test for algorithms
    whose inner loop scales with the front size.
    """
    rng = np.random.default_rng(seed)
    # Sample from Dirichlet(1, …, 1) via normalised exponentials.
    raw = rng.exponential(scale=1.0, size=(n, m))
    return raw / raw.sum(axis=1, keepdims=True)


def make_sphere_front(n: int, m: int, seed: int = 42) -> np.ndarray:
    """Points on the positive unit sphere (DTLZ2-style)."""
    rng = np.random.default_rng(seed)
    raw = np.abs(rng.standard_normal((n, m)))
    return raw / np.linalg.norm(raw, axis=1, keepdims=True)


def make_striated(n: int, m: int) -> np.ndarray:
    """Individual i has all objectives equal to i.

    Individual 0 strictly dominates all others — worst case for algorithms with
    O(n × |F0|) inner loops.
    """
    idx = np.arange(n, dtype=np.float64)
    return np.tile(idx[:, None], (1, m))


# ── Result helpers ─────────────────────────────────────────────────────────────

def normalize_fronts(fronts: list[np.ndarray]) -> list[list[int]]:
    """Sort each front's indices so fronts can be compared for equality."""
    return [sorted(int(i) for i in f) for f in fronts]


def indices_in_fronts(fronts: list[np.ndarray]) -> list[int]:
    """Flatten all front indices into a single sorted list."""
    return sorted(int(i) for f in fronts for i in f)

from ndsort._ext import (
    rank_intersect_sort,
    merge_sort,
    deductive_sort,
    hierarchical_sort,
    best_order_sort,
    dominance_degree_sort,
    rank_ordinal_sort,
    efficient_binary_sort,
    efficient_sequential_sort,
    Dominance,
    SortHint,
    pareto_compare,
)

_methods = {
    "rank_intersect":      rank_intersect_sort,
    "merge":               merge_sort,
    "deductive":           deductive_sort,
    "hierarchical":        hierarchical_sort,
    "best_order":          best_order_sort,
    "dominance_degree":    dominance_degree_sort,
    "rank_ordinal":        rank_ordinal_sort,
    "efficient_binary":    efficient_binary_sort,
    "efficient_sequential": efficient_sequential_sort,
}


def sort(population, *, method="rank_intersect", eps=0.0, hint=SortHint.none):
    """Non-dominated sort a population.

    Parameters
    ----------
    population:
        Either a 2-D numpy array of shape (n_individuals, n_objectives) or a
        list of lists.  A numpy array is preferred: it is read without copying;
        only the internal column-major transposition inside flatten() touches
        the data.
    method:
        Which algorithm to use.  One of: "rank_intersect", "merge",
        "deductive", "hierarchical", "best_order", "dominance_degree",
        "rank_ordinal", "efficient_binary", "efficient_sequential".
    eps:
        Epsilon tolerance for dominance comparisons.
    hint:
        ``SortHint.none``          – sort and deduplicate internally (default).
        ``SortHint.presorted``     – skip lex-sort; caller guarantees the input
                                     is already in lexicographic fitness order.
        ``SortHint.sorted_unique`` – skip sort *and* dedup; caller guarantees
                                     sorted order with no duplicate fitnesses.

    Returns
    -------
    list[numpy.ndarray]
        One 1-D uint64 array per front, containing the indices of the
        individuals assigned to that front (Pareto-rank 0 first).
    """
    return _methods[method](population, eps, hint)

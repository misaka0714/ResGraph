# Experiment Protocol

This protocol fixes the measurement discipline behind the paper numbers. It
applies to every dataset in the README workload table. The runnable form of
this protocol lives in the config templates and commands of this directory;
tools and host integration are described in
[../docs/vsag-integration.md](../docs/vsag-integration.md).

## Shared configuration

All comparisons share one graph shape and one metric family:

| Parameter | Value |
| --- | --- |
| graph | NSW bottom graph (`graph_type: nsw`), no upper-graph reuse across variants |
| `max_degree` | 64 |
| `ef_construction` | 400 |
| base storage | FP32 |
| metric | cosine |
| build threads | 16 |

Search sweeps always use:

| Parameter | Value |
| --- | --- |
| `topk` | 10 |
| `search_query_count` | 1000 |
| search threads | 1 |
| `ef_search` sweep | fine sweep; headline comparisons quote EF ∈ {50, 100, 200, 400} |

`search_query_count` must be set explicitly. Without it the evaluator defaults
to 100,000 queries, and the resulting QPS/latency are **not comparable** with
the 1000-query protocol used across all paper tables.

## Timing accounting: three clocks, never mixed

Every run produces one of three timing stamps. They answer different questions
and must never be added together:

1. **Baseline build** — old-space index construction. Authoritative line:
   `Build finished. Cost: X s`.
2. **Migration refine** — ResGraph-Local / ResGraph-Search graph repair.
   Authoritative line: `RefineGraph finished. Cost: X s`. When the paper quotes
   a migration cost, it is this number and nothing else.
3. **Search evaluation** — the recall/QPS sweep over an already-built index.
   Eval time is never part of migration cost.

Before re-running a refine on a dataset, delete the previous refined index and
its logs. Reused stale artifacts are the classic source of timing pollution
across experiment rounds.

## Result artifacts

Per dataset, one flat directory triple keeps everything auditable:

```text
configs/<dataset>/   generated YAML per stage
indexes/<dataset>/   baseline index, migrated index, chained variants
results/<dataset>/   stage logs and *_search.json summaries
```

Timing is parsed from logs by two regexes: `Build finished\. Cost:\s*([\d.]+)\s*s`
and `RefineGraph finished\. Cost:\s*([\d.]+)\s*s` — reviewers can reproduce the
numbers with `grep -E "Cost:" results/<dataset>/*.log`.

## Fair-comparison constraints

Two variants can be compared only when all of the following hold:

- identical HDF5 inputs (same snapshot, same query rows);
- `search_query_count = 1000`, `topk = 10`, single search thread;
- the same `ef_search` values were swept for both;
- refine was executed under comparable machine load (no co-resident jobs on
  the same cores).

## Interpretation discipline

- A smaller average candidate distance does **not** imply higher recall:
  alpha-pruned diversification needs some long-range and direction-diverse
  edges.
- A higher final average degree tends to raise low-EF reachability while
  lowering QPS; report degree next to every recall/QPS pair.
- The upper graph is not neutral ground: the ResGraph-Local path rebuilds
  it, the ResGraph-Search paths relink and reverse-repair it. Attribute
  candidate-source differences only after equalizing (or at least reporting)
  degree, distance-call counts, and refine wall time.
- Index file size is insensitive to pruned degree because `max_degree`
  pre-allocates edge slots; do not use bytes as a quality proxy.

## Input data format

Build inputs are HDF5 with four datasets:

| dataset | shape | meaning |
| --- | --- | --- |
| `train` | N x d, float32 | base vectors (row-major, object-ID order) |
| `test` | Q x d, float32 | new-space query vectors |
| `neighbors` | Q x k, int | new-space ground-truth IDs |
| `distances` | Q x k, float32 | matching distances |

With file attributes `distance: angular` and `type: dense`. For the search
stage, cap queries at the protocol's 1000 rows rather than submitting the
full query file. The migration refine itself only needs `train`.

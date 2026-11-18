# vsag Host Integration

The paper measurements run ResGraph inside a host vector-search system rather
than against this standalone library. This page explains what that host is,
where the migration logic plugs into it, and how the two refine modes map back
to the algorithms of this repository.

## What vsag is

[vsag](https://github.com/antgroup/vsag) is an open-source C++ vector-search
library maintained by Ant Group. The experiments use its hierarchical graph
index (`hgraph`): a navigable bottom graph searched first with a small upper
layer for entry-point selection. The index is responsible for serialization,
query serving, and layer orchestration — everything that this repository
deliberately leaves out — so it is a realistic host for measuring migrated
graphs end to end.

## Where ResGraph plugs in

ResGraph enters the host as an index-level **refine operation** with two
coordination points in the vsag tree:

- `src/algorithm/hgraph.cpp` — the candidate-discovery and selection logic of
  the migration, guarded by `VSAG_REFINE_*` environment variables.
- `tools/eval/refine_index.cpp` — the experiment entry binary. It reads a YAML
  config, translates the `refine_*` keys into the environment variables above,
  loads an existing index plus the new-space HDF5 matrix, and rewrites the
  adjacency lists.

Because the knobs are environment-backed, every experiment is reproducible
from its YAML file alone; the driver scripts only orchestrate file naming.

## Refine modes

### `drift_sign` — the ResGraph-Local pipeline

Instantiates the ResGraph-Local candidate pipeline of this repository
(`resgraph::RunResGraphLocal`): reuse old-graph k-hop structure for candidate
discovery, screen candidates with packed sign signatures, keep every inherited
one-hop neighbor, and order the union with exact new-space distances plus
alpha-diversified pruning.

```yaml
refine_mode: drift_sign
refine_drift_hop: "2"
refine_drift_sign_only: "1"
refine_drift_candidate_multiplier: "4"   # C = multiplier * max_degree (64)
refine_drift_sign_exact_alpha_prune: "1"
refine_drift_sign_alpha: "1.1"
```

The candidate union is: current one-hop survivors (never screened away) +
two-hop nodes excluded from one-hop + persisted construction candidates kept
by the index at build time (forward-rejected hints) + their one-hop graph
neighbors. The sign screen truncates this union to a global Top-C before
exact distances are computed. `CandidateMultiplier` 2/4 holds the coarse cap
C at 128/256 for `max_degree=64`. `Alpha=1.0` under-prunes the diversity rule
and collapses average degree (observed ~12 vs. ~23–29 at 1.1); it is kept only
as an ablation knob, not a default.

### `two_stage` — the search-side candidate source (NSW baseline)

Instantiates the search-oriented candidate collection on the old graph, the
form ResGraph-Search operates on after vector replacement: stage 1 locates a
seed near the node with `ef=1`, stage 2 traverses the old bottom graph with the
node's **new** vector at a bounded beam, and the returned candidates are
pruned with exact distances at alpha 1.1.

```yaml
refine_mode: two_stage
refine_two_stage_stage1_ef: "1"
refine_two_stage_stage2_ef: "200"
refine_two_stage_budget: "200"
refine_two_stage_alpha: "1.1"
```

`refine_two_stage_budget: "0"` disables the stage-2 distance-call cap; the
unbudgeted variant costs roughly two orders of magnitude more wall time than
`drift_sign` on the same data, so treat it as a diagnostic upper bound rather
than a deployable configuration.

### Upper-layer semantics (fairness note)

The two paths do not treat the upper graph identically: `two_stage` relinks
the upper layer and repairs reverse edges, while `drift_sign` rebuilds the
upper graph from scratch. Recall/QPS deltas between the two modes therefore
must not be attributed to candidate discovery alone; report final average
degree, distance-call counts, and refine wall time alongside every comparison
(see [../experiments/PROTOCOL.md](../experiments/PROTOCOL.md)).

## Eval toolchain

All binaries live under `tools/eval` in the vsag tree and speak HDF5 in,
JSON out:

| Tool | Role |
| --- | --- |
| `build_index` | builds the old-space baseline index (NSW, `max_degree=64`, `ef_construction=400`, FP32 cosine) |
| `refine_index` | applies the migration/refine to a saved index with a new-space matrix |
| `eval_performance` | recall/QPS/latency sweep (`topk=10`, single thread, EF ∈ {50, 100, 200, 400}) |

The runnable commands covering the full pipeline are collected in
[../experiments/README.md](../experiments/README.md).

## Relation to this repository

This repository provides the algorithm reference implementation: a small,
dependency-free, in-memory API (`resgraph::Graph`, `resgraph::EmbeddingSpace`,
`RunResGraphLocal`, `RunResGraphSearch`) intended for reading and reuse. The
vsag integration is the paper-scale manifestation of the same two algorithms
under a real host system: same candidate philosophy, same exact-distance
validation discipline, different storage and serving arithmetic. When porting
results between the two, trust the protocol in
[../experiments/PROTOCOL.md](../experiments/PROTOCOL.md) rather than any single
timing number, and keep in mind that the host owns entry points and layer
structure.

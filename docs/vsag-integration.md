# vsag Host Integration

The paper measurements run ResGraph inside a host vector-search system rather
than against this standalone library. This page explains what that host is,
where the migration logic plugs into it, and how the two migration modes map
back to the algorithms of this repository.

## What vsag is

[vsag](https://github.com/antgroup/vsag) is an open-source C++ vector-search
library maintained by Ant Group. The experiments use its hierarchical graph
index (`hgraph`): a navigable bottom graph searched via a small upper layer
for entry-point selection. The index owns serialization, query serving, and
layer orchestration — everything this repository deliberately leaves out — so
it is a realistic host for measuring migrated graphs end to end.

## Where ResGraph plugs in

ResGraph enters the host as an index-level **refine operation** on an existing
`hgraph` index. The `refine_index` tool in the vsag evaluation tree reads a
YAML config naming the saved index, the replacement embedding matrix (HDF5),
and one of the two migration modes below; it rewrites the bottom-graph
adjacency lists accordingly and writes the migrated index alongside the input.
No query-time path is modified: migration is an offline index maintenance
step, so search evaluation stays identical across variants.

## The two migration modes

### `resgraph_local` — the ResGraph-Local pipeline

Instantiates the ResGraph-Local candidate pipeline of this repository
(`resgraph::RunResGraphLocal`): reuse old-graph two-hop structure for
candidate discovery, screen candidates with packed sign signatures, keep every
inherited one-hop neighbor, and order the surviving union with exact
new-space distances plus alpha-diversified pruning.

```yaml
refine_mode: resgraph_local
refine_hop: "2"                        # two-hop candidate pool
refine_candidate_cap_multiplier: "2"   # C = multiplier x max_degree
refine_alpha: "1.1"
refine_output_degree: "32"
```

The candidate union before screening is the inherited one-hop survivors plus
the two-hop pool. The sign screen truncates the union to a global Top-C
(C = multiplier x max_degree, 128 at the default) before exact distances are
computed. `Alpha = 1.0` under-prunes the diversity rule and collapses average
degree (observed ~12 vs. ~23-29 at the default 1.1), so it serves only as an
ablation knob.

### `resgraph_search` — the search-side refinement

Instantiates the beam-traversal refinement of
`resgraph::RunResGraphSearch` on top of a ResGraph-Local index (or directly on
a vector-replacement graph): per node, walk the current graph with the node's
new vector in a bounded beam, merge the returned candidates with the node's
current adjacency list, and prune with exact distances.

```yaml
refine_mode: resgraph_search
refine_search_ef: "64"     # beam width; typical chains run 64 then 100
refine_search_turns: "1"
refine_alpha: "1.0"
```

### Upper-layer semantics (fairness note)

The two modes do not treat the upper graph identically: ResGraph-Local
rebuilds the upper graph from scratch after bottom-list repair, while
ResGraph-Search relinks the upper layer and repairs reverse edges in place.
Recall/QPS deltas between the two modes therefore must not be attributed to
candidate discovery alone; report final average degree, distance-call
counts, and refine wall time alongside every comparison (see
[../experiments/PROTOCOL.md](../experiments/PROTOCOL.md)).

## Eval toolchain

All binaries live under `tools/eval` in the vsag tree and speak HDF5 in,
JSON out:

| Tool | Role |
| --- | --- |
| `build_index` | builds the old-space baseline index (NSW, `max_degree=64`, `ef_construction=400`, FP32 cosine) |
| `refine_index` | applies one migration mode to a saved index with a new-space matrix |
| `eval_performance` | recall/QPS/latency sweep (`topk=10`, single thread, EF in {50, 100, 200, 400}) |

The runnable stage commands covering the full pipeline are collected in
[../experiments/README.md](../experiments/README.md), with config templates
under [../experiments/configs/](../experiments/configs/).

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

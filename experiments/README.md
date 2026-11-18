# Experiments — Runbook

Runnable form of the paper measurement pipeline on the vsag host system.
Read this together with [PROTOCOL.md](PROTOCOL.md) for the measurement
discipline, [../docs/vsag-integration.md](../docs/vsag-integration.md) for
what the tools do, and [../docs/environment.md](../docs/environment.md) for
the required toolchain.

## Prerequisites

1. A vsag source tree containing the ResGraph refine integration, built in
   Release mode. The stage binaries live under
   `<vsag-tree>/build-release/tools/eval/`:
   `eval_performance`, `refine_index`.
2. Two HDF5 files from [../README.md](../README.md) data preparation, old and
   new encoder, sharing object-ID order (`train` plus, for search, capped
   `test`/`neighbors`/`distances`).
3. Nothing else on the machine competing for the measured cores during build
   and refine stages.

## Layout convention

Per dataset:

```text
work/<dataset>/
  configs/   instantiated YAML per stage (from ../configs templates)
  indexes/   baseline_index, resgraph_local_index[_refined], ...
  results/   stage logs and *_search.json
```

## Stage commands

The templates in [configs/](configs/) use `<PLACEHOLDER>` tokens. Instantiate
them for one dataset:

```bash
export BIN=<vsag-tree>/build-release/tools/eval
export DATASET_DIR=work/yelp_650k_768
mkdir -p $DATASET_DIR/{configs,indexes,results}

for f in 01_build_baseline 03_refine_resgraph_local 04_search_sweep 05_refine_resgraph_search; do
  sed -e "s#<DATASET_DIR>#$PWD/$DATASET_DIR#g" \
      -e "s#<DIM>#768#g" \
      -e "s#<OLD_SPACE_H5>#$PWD/data/yelp_distilroberta.h5#g" \
      -e "s#<NEW_SPACE_H5>#$PWD/data/yelp_bge.h5#g" \
      configs/$f.yaml > $DATASET_DIR/configs/$f.yaml
done
```

### 1. Baseline build (old space)

```bash
$BIN/eval_performance $DATASET_DIR/configs/01_build_baseline.yaml \
  > $DATASET_DIR/results/baseline_build.log 2>&1
```

Authoritative cost: the `Build finished. Cost: ...` line.

### 2. ResGraph-Local migration

Refine runs on a copy of the baseline index and writes `..._refined`:

```bash
cp $DATASET_DIR/indexes/baseline_index $DATASET_DIR/indexes/resgraph_local_index
$BIN/refine_index $DATASET_DIR/configs/03_refine_resgraph_local.yaml \
  > $DATASET_DIR/results/resgraph_local_refine.log 2>&1
```

Authoritative migration cost: the `RefineGraph finished. Cost: ...` line.
Re-runs must delete both the copied index and the `_refined` output first —
never refine on top of a previous run's leftovers.

### 3. ResGraph-Search chain (optional)

Chains a second refine on top of the ResGraph-Local output (typical chain
values `refine_search_ef: "64"`, then `"100"`):

```bash
cp $DATASET_DIR/indexes/resgraph_local_index_refined \
   $DATASET_DIR/indexes/resgraph_search_index
$BIN/refine_index $DATASET_DIR/configs/05_refine_resgraph_search.yaml \
  > $DATASET_DIR/results/resgraph_search_refine.log 2>&1
```

### 4. Search sweep per variant

Instantiate the sweep once per variant (baseline vector replacement,
ResGraph-Local, ResGraph-Search, full rebuild — whichever the comparison
needs) by substituting `<INDEX_TO_EVALUATE>`, `<VARIANT>`, and the capped
query file `<NEW_SPACE_SEARCH_H5>`:

```bash
sed -e "s#<INDEX_TO_EVALUATE>#resgraph_local_index_refined#g" \
    -e "s#<VARIANT>#resgraph_local#g" \
    -e "s#<NEW_SPACE_SEARCH_H5>#$PWD/data/yelp_bge_q1000.h5#g" \
    configs/04_search_sweep.yaml > $DATASET_DIR/configs/04_search_resgraph_local.yaml

$BIN/eval_performance $DATASET_DIR/configs/04_search_resgraph_local.yaml \
  > $DATASET_DIR/results/resgraph_local_search.log 2>&1
```

Search time is evaluation only — it never mixes into migration cost (see
[PROTOCOL.md](PROTOCOL.md)).

## Checking a run

```bash
# the three clocks
grep -E "Cost:" $DATASET_DIR/results/*.log

# spot-check the protocol invariants in one result file
python - <<'EOF'
import json
data = json.load(open("search_resgraph_local.json"))
entry = next(iter(data.values()))
print(entry["dataset_info"]["query_count"])   # must be 1000
EOF
```

Summaries must keep the protocol fields together: recall, QPS, mean/P95
latency per EF, plus the refine log's final average degree and distance-call
count for the variant.

## Scope-control rules

The stage commands above are intentionally manual. Two discipline rules keep
reruns clean:

- *Refine-only rerun*: run stages 1-2 and skip stage 4 when re-measuring
  migration cost.
- *Force-clean refine rerun*: delete `indexes/resgraph_local_index*` and the
  old refine logs before re-running stage 2 so no cached artifact survives.

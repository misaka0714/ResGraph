# Environment and Dependencies

ResGraph separates two stacks: the algorithm library in this repository, and
the host-system experiment stack used for the paper measurements. This page
lists what each stack requires. Dataset and checkpoint sources are listed in
the main [README](../README.md); nothing here redistributes model weights.

## Algorithm library (this repository)

| Component | Requirement | Notes |
| --- | --- | --- |
| CMake | >= 3.16 | `cmake -S . -B build` |
| C++ compiler | C++17 | GCC >= 9 or Clang >= 10 |
| Threads | POSIX threads (`pthreads`) | found automatically by `find_package(Threads)` |

The library has **no third-party runtime dependency**. Beside the C++17
standard library it links only pthreads, which every supported platform
provides. An optimized build with warnings enabled is:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Host experiment stack (vsag)

The paper experiments embed ResGraph inside the [vsag](https://github.com/antgroup/vsag)
vector-search library. Building that host tree additionally requires:

| Component | Requirement | Notes |
| --- | --- | --- |
| CMake | >= 3.18 | vsag top-level `CMakeLists.txt` requirement |
| C++ compiler | C++17 | same toolchain class as above |
| HDF5 | C library + C++ bindings (`H5Cpp.h`) | experiment datasets/matrices are passed around as HDF5 files |
| yaml-cpp | any recent release | stage configs are plain YAML files |
| nlohmann/json | any recent release | machine-readable result summaries |

See [vsag-integration.md](vsag-integration.md) for how these pieces connect,
and [../experiments/README.md](../experiments/README.md) for runnable commands.

## Data preparation (Python)

Encoding the corpora with the old/new checkpoints and writing the HDF5 inputs
is done in Python. The packages below cover every workload listed in the
README table:

| Package | Used for |
| --- | --- |
| `torch`, `torchvision` | image checkpoints (`resnet18`/`resnet34`), `torchvision.datasets` loaders, GPU/CPU encoding batches |
| `transformers` | text and LLM encoders (`NV-Embed-v2`, `SFR-Embedding-Mistral`, Qwen2/Qwen2.5, `e5-base-v2`, `gte-base`, `metaclip`, `bge-base-en-v1.5`, `all-distilroberta-v1`, MiniLM models) |
| `datasets` | Hugging Face dataset download (`load_dataset`), incl. `fancyzhx/ag_news`, `Yelp/yelp_review_full`, `HuggingFaceFW/fineweb-edu`, `allenai/c4` |
| `h5py` | writing row-major `float32` matrices (base vectors, queries, optional new-space ground truth) |
| `numpy` | ID bookkeeping, shuffling, and matrix layout |
| `matplotlib` | recall-QPS curves and paper figures |

Minimal install for the preparation side:

```bash
python -m pip install torch torchvision transformers datasets h5py numpy matplotlib
```

Some checkpoints (for example `nvidia/NV-Embed-v2`) require accepting gated
terms and an authenticated `HF_TOKEN` before `load_dataset` or model download
succeeds.

## Practical notes

- All timing-sensitive steps should run without co-resident GPU jobs; see the
  timing-accounting rules in [../experiments/PROTOCOL.md](../experiments/PROTOCOL.md).
- CPU-side knowledge distills to one rule: fix the dataset revision and the
  model revision (commit hash) in every artifact name; binaries in the eval
  stack read only what the YAML config points to.
- HDF5 files should be written once with `compression=None`; the eval tools
  memory-map them and compressed datasets slow both refine and search.

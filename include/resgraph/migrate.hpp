// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>

#include "resgraph/embedding_space.hpp"
#include "resgraph/graph.hpp"
#include "resgraph/types.hpp"

namespace resgraph {

struct LocalConfig {
    std::size_t max_degree{64};
    std::size_t shortlist_size{256};
    float diversity_factor{1.1F};
    std::size_t calibration_sample_size{4096};
    std::uint64_t random_seed{20250311};
    std::size_t thread_count{0};
    std::size_t work_block_size{64};
};

struct SearchConfig {
    std::size_t max_degree{64};
    std::size_t beam_width{256};
    std::size_t max_hops{4};
    float diversity_factor{1.1F};
    NodeId entry_point{0};
    std::size_t thread_count{0};
    std::size_t work_block_size{8};
};

struct MigrationResult {
    Graph graph;
    MigrationStats stats;
};

// Runs the immutable two-hop migration pass described as ResGraph-Local.
[[nodiscard]] MigrationResult RunResGraphLocal(const Graph& old_graph,
                                          const EmbeddingSpace& new_space,
                                          const LocalConfig& config = {});

// Runs one asynchronous hop-bounded beam-refinement pass described as
// ResGraph-Search. The input may be a vector-replacement graph or a ResGraph-Local graph.
[[nodiscard]] MigrationResult RunResGraphSearch(const Graph& starting_graph,
                                           const EmbeddingSpace& new_space,
                                           const SearchConfig& config = {});

}  // namespace resgraph

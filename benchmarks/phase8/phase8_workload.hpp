#pragma once

// M5 Phase-8 PR-B shared deterministic candidate workloads.
//
// The standard cells deliberately reuse the Phase-6 M2 workload identities
// and generators.  The one mixed-stream cell is Phase-8-specific because the
// Phase-6 M2 registry has no single insert/update/delete mixed identity.
// Workload construction is setup only; no workload object is used as a
// production or installed API.

#include "phase8_model.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bmd_projection::m5::phase8 {

enum class Phase8Operation : std::uint8_t {
    apply_level,
    apply_updates,
    replace_all,
    top_levels,
};

struct Phase8Workload final {
    std::string id;
    std::string workload_spec_text;
    std::string workload_spec_sha256;
    std::string generated_workload_sha256;
    std::string generator_schema;
    Phase8Operation operation{};
    std::size_t depth{};
    std::size_t batch{};
    std::size_t query_limit{};
    std::vector<core::BookLevel> initial_bids;
    std::vector<core::BookLevel> initial_asks;
    std::vector<core::BookLevel> bids;
    std::vector<core::BookLevel> asks;
    std::vector<core::LevelUpdate> updates;
    std::vector<std::vector<core::LevelUpdate>> update_batches;
};

[[nodiscard]] const std::vector<Phase8Workload>& phase8_workloads();
[[nodiscard]] std::string_view phase8_operation_name(Phase8Operation operation) noexcept;
template <Phase8ModelConcept Model> [[nodiscard]] std::string phase8_digest(const Model& model) {
    std::string digest;
    digest.reserve(model.level_count(core::BookSide::Bid) * 32U +
                   model.level_count(core::BookSide::Ask) * 32U + 64U);
    for (const auto side : {core::BookSide::Bid, core::BookSide::Ask}) {
        digest += side == core::BookSide::Bid ? 'B' : 'A';
        for (const auto& level : model.all_levels(side)) {
            digest += std::to_string(level.price.value());
            digest += ':';
            digest += std::to_string(level.quantity.value());
            digest += ';';
        }
        digest += '|';
    }
    return digest;
}

} // namespace bmd_projection::m5::phase8

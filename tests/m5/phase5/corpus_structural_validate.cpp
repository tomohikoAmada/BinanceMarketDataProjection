#include "replay_fuzz_decoder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace decoder = bmd_projection::m5::replay::fuzz_decoder;
namespace replay = bmd_projection::m5::replay;

namespace {

using Check = bool (*)(const decoder::FuzzCase&);

struct RequiredSeed final {
    std::string_view name;
    std::string_view contract;
    Check check;
};

[[nodiscard]] std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

template <typename OperationType>
[[nodiscard]] const OperationType* operation(const decoder::FuzzCase& value, std::size_t index) {
    if (index >= value.operations.size()) {
        return nullptr;
    }
    return std::get_if<OperationType>(&value.operations[index]);
}

[[nodiscard]] bool event_level_bounds_hold(const decoder::FuzzCase& value) {
    for (const auto& candidate : value.operations) {
        if (const auto* install = std::get_if<replay::InstallBaselineOp>(&candidate);
            install != nullptr &&
            install->bids.size() + install->asks.size() > decoder::kMaxLevelsPerEvent) {
            return false;
        }
        if (const auto* rebaseline = std::get_if<replay::RebaselineOp>(&candidate);
            rebaseline != nullptr &&
            rebaseline->bids.size() + rebaseline->asks.size() > decoder::kMaxLevelsPerEvent) {
            return false;
        }
        if (const auto* update = std::get_if<replay::DepthUpdateOp>(&candidate);
            update != nullptr && update->levels.size() > decoder::kMaxLevelsPerEvent) {
            return false;
        }
        if (const auto* snapshot = std::get_if<replay::SnapshotRequestOp>(&candidate);
            snapshot != nullptr && snapshot->host_quality_facts.size() > 6U) {
            return false;
        }
        if (const auto* metadata = std::get_if<replay::AdapterMetadataOp>(&candidate);
            metadata != nullptr && metadata->observed_quality.size() > 8U) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_bridge(const replay::InstallBaselineOp& install,
                             const replay::DepthUpdateOp& update) {
    return update.first_update_id <= install.last_update_id + 1U &&
           install.last_update_id + 1U <= update.final_update_id;
}

[[nodiscard]] bool is_locked(const replay::InstallBaselineOp& install) {
    return std::ranges::any_of(install.bids, [&install](const replay::LevelInput& bid) {
        return std::ranges::any_of(
            install.asks, [&bid](const replay::LevelInput& ask) { return bid.price == ask.price; });
    });
}

[[nodiscard]] bool check_spot_synchronized(const decoder::FuzzCase& value) {
    const auto* install = operation<replay::InstallBaselineOp>(value, 0);
    const auto* bridge = operation<replay::DepthUpdateOp>(value, 1);
    const auto* continuation = operation<replay::DepthUpdateOp>(value, 2);
    return value.mode == decoder::DecodedMode::CoreOnly && value.market == replay::Market::Spot &&
           value.sequence_policy == replay::SequencePolicy::Spot && value.operations.size() == 3U &&
           install != nullptr && bridge != nullptr && continuation != nullptr &&
           is_bridge(*install, *bridge) &&
           continuation->first_update_id == bridge->final_update_id + 1U &&
           continuation->final_update_id == continuation->first_update_id;
}

[[nodiscard]] bool check_usdm_synchronized(const decoder::FuzzCase& value) {
    const auto* install = operation<replay::InstallBaselineOp>(value, 0);
    const auto* bridge = operation<replay::DepthUpdateOp>(value, 1);
    const auto* continuation = operation<replay::DepthUpdateOp>(value, 2);
    return value.mode == decoder::DecodedMode::CoreOnly &&
           value.market == replay::Market::UsdMPerpetual &&
           value.sequence_policy == replay::SequencePolicy::UsdMPerpetual &&
           value.operations.size() == 3U && install != nullptr && bridge != nullptr &&
           continuation != nullptr && is_bridge(*install, *bridge) &&
           bridge->first_update_id <= install->last_update_id &&
           bridge->previous_final == install->last_update_id &&
           continuation->first_update_id == bridge->final_update_id + 1U &&
           continuation->previous_final == bridge->final_update_id;
}

[[nodiscard]] bool check_bridge_transition(const decoder::FuzzCase& value) {
    const auto* install = operation<replay::InstallBaselineOp>(value, 0);
    const auto* bridge = operation<replay::DepthUpdateOp>(value, 1);
    return value.operations.size() == 2U && install != nullptr && bridge != nullptr &&
           is_bridge(*install, *bridge);
}

[[nodiscard]] bool check_gap(const decoder::FuzzCase& value) {
    const auto* install = operation<replay::InstallBaselineOp>(value, 0);
    const auto* gap = operation<replay::DepthUpdateOp>(value, 1);
    return value.operations.size() == 2U && install != nullptr && gap != nullptr &&
           gap->first_update_id > install->last_update_id + 1U;
}

[[nodiscard]] bool check_recovery(const decoder::FuzzCase& value) {
    const auto* install = operation<replay::InstallBaselineOp>(value, 0);
    const auto* gap = operation<replay::DepthUpdateOp>(value, 1);
    const auto* reset = operation<replay::ResetOp>(value, 2);
    const auto* rebaseline = operation<replay::RebaselineOp>(value, 3);
    const auto* bridge = operation<replay::DepthUpdateOp>(value, 4);
    return value.operations.size() == 5U && install != nullptr && gap != nullptr &&
           reset != nullptr && rebaseline != nullptr && bridge != nullptr &&
           gap->first_update_id > install->last_update_id + 1U &&
           bridge->first_update_id <= rebaseline->last_update_id + 1U &&
           rebaseline->last_update_id + 1U <= bridge->final_update_id;
}

[[nodiscard]] bool check_duplicate_stale(const decoder::FuzzCase& value) {
    const auto* install = operation<replay::InstallBaselineOp>(value, 0);
    const auto* bridge = operation<replay::DepthUpdateOp>(value, 1);
    const auto* duplicate = operation<replay::DepthUpdateOp>(value, 2);
    const auto* stale = operation<replay::DepthUpdateOp>(value, 3);
    return value.operations.size() == 4U && install != nullptr && bridge != nullptr &&
           duplicate != nullptr && stale != nullptr && is_bridge(*install, *bridge) &&
           duplicate->final_update_id == bridge->final_update_id &&
           stale->final_update_id < bridge->final_update_id;
}

[[nodiscard]] bool check_locked_crossed(const decoder::FuzzCase& value) {
    const auto* install = operation<replay::InstallBaselineOp>(value, 0);
    return value.operations.size() == 1U && install != nullptr && is_locked(*install);
}

[[nodiscard]] bool check_decimal_boundaries(const decoder::FuzzCase& value) {
    const auto* install = operation<replay::InstallBaselineOp>(value, 0);
    if (value.operations.size() != 1U || install == nullptr) {
        return false;
    }
    std::vector<std::string> tokens;
    for (const auto& level : install->bids) {
        tokens.push_back(level.price);
        tokens.push_back(level.quantity);
    }
    for (const auto& level : install->asks) {
        tokens.push_back(level.price);
        tokens.push_back(level.quantity);
    }
    static const std::vector<std::string> kExpected{
        "", "0", "1.23", "00", "-42", "12a34", "9999999999999999999", "9"};
    return tokens == kExpected;
}

[[nodiscard]] bool check_depth_limit_snapshot(const decoder::FuzzCase& value) {
    const auto* install = operation<replay::InstallBaselineOp>(value, 0);
    const auto* bridge = operation<replay::DepthUpdateOp>(value, 1);
    const auto* snapshot = operation<replay::SnapshotRequestOp>(value, 2);
    return value.mode == decoder::DecodedMode::AdapterEnabled && value.operations.size() == 3U &&
           install != nullptr && bridge != nullptr && snapshot != nullptr &&
           install->bids.size() >= 3U && install->asks.size() >= 3U &&
           is_bridge(*install, *bridge) && snapshot->depth_limit == 1U;
}

[[nodiscard]] bool check_quality_combinations(const decoder::FuzzCase& value) {
    const auto* metadata = operation<replay::AdapterMetadataOp>(value, 0);
    const auto* install = operation<replay::InstallBaselineOp>(value, 1);
    const auto* bridge = operation<replay::DepthUpdateOp>(value, 2);
    const auto* snapshot = operation<replay::SnapshotRequestOp>(value, 3);
    const std::vector<replay::HostQualityFact> expected_inbound{
        replay::HostQualityFact::OutOfOrder, replay::HostQualityFact::ProducerRestart};
    const std::vector<replay::HostQualityFact> expected_host{
        replay::HostQualityFact::Duplicate, replay::HostQualityFact::SnapshotTooOld};
    return value.mode == decoder::DecodedMode::AdapterEnabled && value.operations.size() == 4U &&
           metadata != nullptr && install != nullptr && bridge != nullptr && snapshot != nullptr &&
           metadata->observed_quality == expected_inbound &&
           snapshot->host_quality_facts == expected_host && is_locked(*install) &&
           is_bridge(*install, *bridge);
}

constexpr std::array<RequiredSeed, 10> kRequiredSeeds{{
    {"spot_synchronized_stream.bin", "exact Spot synchronized ID geometry",
     check_spot_synchronized},
    {"usdm_synchronized_stream.bin", "exact USD-M synchronized ID/pu geometry",
     check_usdm_synchronized},
    {"bridge_transition.bin", "successor-covering bridge geometry", check_bridge_transition},
    {"gap.bin", "forward-separated gap geometry", check_gap},
    {"recovery.bin", "gap, reset, rebaseline, and post-rebaseline bridge", check_recovery},
    {"duplicate_stale.bin", "established current ID followed by duplicate and stale ranges",
     check_duplicate_stale},
    {"locked_crossed.bin", "actual locked bid/ask price geometry", check_locked_crossed},
    {"decimal_boundaries.bin", "exact eight-token decimal category set", check_decimal_boundaries},
    {"depth_limit_snapshot.bin", "AdapterEnabled synchronized depth-one snapshot over deep sides",
     check_depth_limit_snapshot},
    {"quality_combinations.bin", "AdapterEnabled inbound, host, and derived crossed geometry",
     check_quality_combinations},
}};

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: bmd_projection_m5_replay_corpus_structural_validate <corpus_dir>\n";
        return 1;
    }

    const std::span<char*> arguments{argv, static_cast<std::size_t>(argc)};
    const std::string directory = arguments[1];
    std::cerr << "Structural corpus validator: " << directory << '\n';

    int failures = 0;
    for (const auto& required : kRequiredSeeds) {
        const std::string path = directory + "/" + std::string(required.name);
        const auto data = read_file(path);
        if (data.empty()) {
            std::cerr << "  FAIL " << required.name << ": missing or empty\n";
            ++failures;
            continue;
        }
        const auto decoded = decoder::decode(data.data(), data.size());
        if (!decoded.has_value()) {
            std::cerr << "  FAIL " << required.name << ": decoder rejected seed\n";
            ++failures;
            continue;
        }
        const bool valid = event_level_bounds_hold(*decoded) && required.check(*decoded);
        std::cerr << "  " << (valid ? "PASS " : "FAIL ") << required.name << ": "
                  << required.contract << " | ops=" << decoded->operations.size() << '\n';
        if (!valid) {
            ++failures;
        }
    }

    std::cerr << "Results: " << kRequiredSeeds.size() << " mandatory seeds, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}

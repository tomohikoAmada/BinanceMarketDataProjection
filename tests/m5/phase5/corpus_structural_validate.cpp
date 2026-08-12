#include "replay_fuzz_decoder.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace decoder = bmd_projection::m5::replay::fuzz_decoder;
namespace replay = bmd_projection::m5::replay;

[[nodiscard]] std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return {};
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}

[[nodiscard]] bool has_install(const decoder::FuzzCase& c) {
    for (const auto& op : c.operations) {
        if (std::holds_alternative<replay::InstallBaselineOp>(op))
            return true;
    }
    return false;
}

[[nodiscard]] bool has_update(const decoder::FuzzCase& c) {
    for (const auto& op : c.operations) {
        if (std::holds_alternative<replay::DepthUpdateOp>(op))
            return true;
    }
    return false;
}

[[nodiscard]] bool has_reset(const decoder::FuzzCase& c) {
    for (const auto& op : c.operations) {
        if (std::holds_alternative<replay::ResetOp>(op))
            return true;
    }
    return false;
}

[[nodiscard]] bool has_rebaseline(const decoder::FuzzCase& c) {
    for (const auto& op : c.operations) {
        if (std::holds_alternative<replay::RebaselineOp>(op))
            return true;
    }
    return false;
}

[[nodiscard]] bool has_snapshot(const decoder::FuzzCase& c) {
    for (const auto& op : c.operations) {
        if (std::holds_alternative<replay::SnapshotRequestOp>(op))
            return true;
    }
    return false;
}

[[nodiscard]] bool has_metadata(const decoder::FuzzCase& c) {
    for (const auto& op : c.operations) {
        if (std::holds_alternative<replay::AdapterMetadataOp>(op))
            return true;
    }
    return false;
}

[[nodiscard]] bool has_baseline_continuation(const decoder::FuzzCase& c) {
    return has_install(c) && has_update(c);
}

[[nodiscard]] int count_updates(const decoder::FuzzCase& c) {
    int du = 0;
    for (const auto& op : c.operations) {
        if (std::holds_alternative<replay::DepthUpdateOp>(op))
            ++du;
    }
    return du;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: bmd_projection_m5_corpus_structural_validate <corpus_dir>\n";
        return 1;
    }

    const std::string dir = argv[1];
    std::cerr << "Structural corpus validator: " << dir << "\n";

    int failures = 0;
    int total = 0;

    auto validate = [&](const std::string& name, const std::string& expected_desc,
                        bool (*check_fn)(const decoder::FuzzCase&)) {
        ++total;
        const auto path = dir + "/" + name;
        const auto data = read_file(path);
        if (data.empty()) {
            std::cerr << "  FAIL " << name << ": cannot read file\n";
            ++failures;
            return;
        }
        auto result = decoder::decode(data.data(), data.size());
        if (!result.has_value()) {
            std::cerr << "  FAIL " << name << ": decode returned nullopt\n";
            ++failures;
            return;
        }
        auto ok = check_fn(*result);
        if (ok) {
            std::cerr << "  PASS " << name << ": " << expected_desc
                      << " | ops=" << result->operations.size() << "\n";
        } else {
            std::cerr << "  FAIL " << name << ": " << expected_desc
                      << " ops=" << result->operations.size()
                      << " market=" << static_cast<int>(result->market) << "\n";
            ++failures;
        }
    };

    static auto check_spot = [](const decoder::FuzzCase& c) {
        return c.market == replay::Market::Spot && has_baseline_continuation(c);
    };
    static auto check_usdm = [](const decoder::FuzzCase& c) {
        return c.market == replay::Market::UsdMPerpetual && has_baseline_continuation(c);
    };
    static auto check_bridge = [](const decoder::FuzzCase& c) {
        return has_baseline_continuation(c) && c.operations.size() >= 2;
    };
    static auto check_gap = [](const decoder::FuzzCase& c) {
        return has_install(c) && has_update(c);
    };
    static auto check_recovery = [](const decoder::FuzzCase& c) {
        return has_reset(c) || has_rebaseline(c);
    };
    static auto check_duplicate = [](const decoder::FuzzCase& c) { return count_updates(c) >= 2; };
    static auto check_locked = [](const decoder::FuzzCase& c) { return has_install(c); };
    static auto check_decimal = [](const decoder::FuzzCase& c) { return c.operations.size() >= 1; };
    static auto check_snapshot = [](const decoder::FuzzCase& c) { return has_snapshot(c); };
    static auto check_quality = [](const decoder::FuzzCase& c) {
        return has_metadata(c) || has_snapshot(c);
    };

    validate("spot_synchronized_stream.bin", "Spot market, baseline+continuation", check_spot);
    validate("usdm_synchronized_stream.bin", "USD-M market, baseline+continuation", check_usdm);
    validate("bridge_transition.bin", "baseline + successor-covering update", check_bridge);
    validate("gap.bin", "forward-separated update pattern", check_gap);
    validate("recovery.bin", "reset/rebaseline/recovery-shaped ops", check_recovery);
    validate("duplicate_stale.bin", "repeated/stale-looking ID ranges", check_duplicate);
    validate("locked_crossed.bin", "locked/crossed price geometry", check_locked);
    validate("decimal_boundaries.bin", "parser/rescale boundary tokens", check_decimal);
    validate("depth_limit_snapshot.bin", "SnapshotRequest with depth limit variants",
             check_snapshot);
    validate("quality_combinations.bin", "HostQualityFact + adapter quality coverage",
             check_quality);

    std::cerr << "\nResults: " << total << " seeds, " << failures << " failures\n";
    return failures > 0 ? 1 : 0;
}

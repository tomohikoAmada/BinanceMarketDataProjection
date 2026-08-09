#include "small_workload.hpp"

#include "canonical_text.hpp"
#include "replay_fixture.hpp"

#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::phase3 {
namespace {

struct GeneratedWorkload final {
    std::string fixture_id;
    std::string market;
    std::string policy;
    std::uint32_t price_scale{};
    std::uint32_t quantity_scale{};
    std::vector<std::string> events;
};

[[nodiscard]] std::string snapshot_line(std::size_t ordinal, bool gap) {
    const auto timestamp = 1'000'000U + ordinal;
    std::string line = "SNAPSHOT_REQUEST 20 ";
    line += gap ? "OrderBookResync " : "- ";
    line += "snapshot-" + std::to_string(ordinal);
    line += " phase3 1.0 RecorderReplay " + std::to_string(timestamp) + " - ";
    line += gap ? std::to_string(timestamp - 1U) + ",ResyncRequired" : "-";
    return line;
}

void append_spot_transition(std::vector<std::string>& events, std::uint64_t& last_update_id,
                            std::size_t& snapshot_ordinal) {
    events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id - 2U) + " " +
                     std::to_string(last_update_id - 1U) + " pu=- B:98.00,1.000");
    events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id) + " " +
                     std::to_string(last_update_id) + " pu=- A:102.00,2.000");
    ++last_update_id;
    events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id) + " " +
                     std::to_string(last_update_id) + " pu=- A:100.00,1.000");
    events.push_back(snapshot_line(snapshot_ordinal++, false));
    ++last_update_id;
    events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id) + " " +
                     std::to_string(last_update_id) + " pu=- A:99.50,1.250");
    events.push_back(snapshot_line(snapshot_ordinal++, false));
    events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id + 2U) + " " +
                     std::to_string(last_update_id + 2U) + " pu=- B:97.00,3.000");
    events.push_back(snapshot_line(snapshot_ordinal++, true));
    events.emplace_back("RESET");
    const auto baseline = last_update_id + 100U;
    events.push_back("REBASELINE " + std::to_string(baseline) +
                     " B:100.00,1.000|B:99.00,2.000 A:101.00,1.500|A:102.00,2.500");
    events.push_back("DEPTH_UPDATE " + std::to_string(baseline - 1U) + " " +
                     std::to_string(baseline + 1U) + " pu=- B:100.00,1.125");
    last_update_id = baseline + 1U;
}

[[nodiscard]] GeneratedWorkload generate_spot() {
    GeneratedWorkload workload{"m5-small-spot-v1", "Spot", "Spot", 2, 3, {}};
    auto& events = workload.events;
    events.reserve(kSmallWorkloadEventCount);
    events.emplace_back("ADAPTER_METADATA Duplicate,Overlap");
    events.emplace_back("INSTALL_BASELINE 100000 B:100.00,1.000|B:99.00,2.000 "
                        "A:101.00,1.500|A:102.00,2.500");
    events.emplace_back("DEPTH_UPDATE 99999 100001 pu=- B:100.00,1.125");
    events.push_back(snapshot_line(0, false));

    std::uint64_t last_update_id = 100'001;
    std::size_t snapshot_ordinal = 1;
    std::size_t normal_updates = 0;
    while (events.size() < kSmallWorkloadEventCount) {
        if (normal_updates != 0U && normal_updates % 350U == 0U &&
            events.size() + 11U <= kSmallWorkloadEventCount) {
            append_spot_transition(events, last_update_id, snapshot_ordinal);
            ++normal_updates;
            continue;
        }
        if (normal_updates % 113U == 0U && events.size() + 2U <= kSmallWorkloadEventCount) {
            events.emplace_back("ADAPTER_METADATA OutOfOrder,Duplicate");
        }
        if (events.size() >= kSmallWorkloadEventCount) {
            break;
        }
        ++last_update_id;
        const auto price_minor = 9'500U + normal_updates % 401U;
        const auto quantity_minor = 1U + normal_updates % 997U;
        const char side = normal_updates % 2U == 0U ? 'B' : 'A';
        const auto price = std::to_string(price_minor / 100U) + "." +
                           (price_minor % 100U < 10U ? "0" : "") +
                           std::to_string(price_minor % 100U);
        const auto quantity = std::to_string(quantity_minor / 1000U) + "." +
                              (quantity_minor % 1000U < 100U ? "0" : "") +
                              (quantity_minor % 1000U < 10U ? "0" : "") +
                              std::to_string(quantity_minor % 1000U);
        events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id) + " " +
                         std::to_string(last_update_id) + " pu=- " + side + ":" + price + "," +
                         quantity);
        ++normal_updates;
    }
    return workload;
}

void append_usdm_transition(std::vector<std::string>& events, std::uint64_t& last_update_id,
                            std::size_t& snapshot_ordinal) {
    events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id - 2U) + " " +
                     std::to_string(last_update_id - 1U) + " pu=" + std::to_string(last_update_id) +
                     " B:98.00,1.000");
    events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id) + " " +
                     std::to_string(last_update_id) + " pu=" + std::to_string(last_update_id) +
                     " A:102.00,2.000");
    const auto previous = last_update_id;
    ++last_update_id;
    events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id) + " " +
                     std::to_string(last_update_id) + " pu=" + std::to_string(previous) +
                     " A:100.00,1.000");
    events.push_back(snapshot_line(snapshot_ordinal++, false));
    events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id + 1U) + " " +
                     std::to_string(last_update_id + 1U) +
                     " pu=" + std::to_string(last_update_id - 1U) + " B:97.00,3.000");
    events.push_back(snapshot_line(snapshot_ordinal++, true));
    events.emplace_back("RESET");
    const auto baseline = last_update_id + 100U;
    events.push_back("REBASELINE " + std::to_string(baseline) +
                     " B:100.00,1.000|B:99.00,2.000 A:101.00,1.500|A:102.00,2.500");
    events.push_back("DEPTH_UPDATE " + std::to_string(baseline - 1U) + " " +
                     std::to_string(baseline + 1U) + " pu=" + std::to_string(baseline) +
                     " B:100.00,1.125");
    last_update_id = baseline + 1U;
}

[[nodiscard]] GeneratedWorkload generate_usdm() {
    GeneratedWorkload workload{"m5-small-usdm-v1", "UsdMPerpetual", "UsdMPerpetual", 2, 3, {}};
    auto& events = workload.events;
    events.reserve(kSmallWorkloadEventCount);
    events.emplace_back("ADAPTER_METADATA RecoveredTail");
    events.emplace_back("INSTALL_BASELINE 500000 B:100.00,1.000|B:99.00,2.000 "
                        "A:101.00,1.500|A:102.00,2.500");
    events.emplace_back("DEPTH_UPDATE 499999 500001 pu=500000 B:100.00,1.125");
    events.push_back(snapshot_line(0, false));

    std::uint64_t last_update_id = 500'001;
    std::size_t snapshot_ordinal = 1;
    std::size_t normal_updates = 0;
    while (events.size() < kSmallWorkloadEventCount) {
        if (normal_updates != 0U && normal_updates % 350U == 0U &&
            events.size() + 9U <= kSmallWorkloadEventCount) {
            append_usdm_transition(events, last_update_id, snapshot_ordinal);
            ++normal_updates;
            continue;
        }
        if (normal_updates % 127U == 0U && events.size() + 2U <= kSmallWorkloadEventCount) {
            events.emplace_back("ADAPTER_METADATA RecoveredTail,Overlap");
        }
        if (events.size() >= kSmallWorkloadEventCount) {
            break;
        }
        const auto previous = last_update_id;
        ++last_update_id;
        const auto price_minor = 9'500U + normal_updates % 401U;
        const auto quantity_minor = 1U + normal_updates % 997U;
        const char side = normal_updates % 2U == 0U ? 'B' : 'A';
        const auto price = std::to_string(price_minor / 100U) + "." +
                           (price_minor % 100U < 10U ? "0" : "") +
                           std::to_string(price_minor % 100U);
        const auto quantity = std::to_string(quantity_minor / 1000U) + "." +
                              (quantity_minor % 1000U < 100U ? "0" : "") +
                              (quantity_minor % 1000U < 10U ? "0" : "") +
                              std::to_string(quantity_minor % 1000U);
        events.push_back("DEPTH_UPDATE " + std::to_string(last_update_id) + " " +
                         std::to_string(last_update_id) + " pu=" + std::to_string(previous) + " " +
                         side + ":" + price + "," + quantity);
        ++normal_updates;
    }
    return workload;
}

[[nodiscard]] replay::ReplayFixture materialize(const GeneratedWorkload& workload) {
    std::ostringstream log;
    log << "REPLAY_V1 market=" << workload.market
        << " symbol=BTCUSDT price_scale=" << workload.price_scale
        << " quantity_scale=" << workload.quantity_scale << " policy=" << workload.policy
        << " fixture_id=" << workload.fixture_id
        << " provenance_generator=" << kSmallGeneratorVersion
        << " provenance_seed=548746690337 provenance_source=synthetic\n";
    for (const auto& event : workload.events) {
        log << event << '\n';
    }
    const auto replay_log = log.str();
    const auto hash_result = replay::sha256_hex(replay_log);
    if (!std::holds_alternative<std::string>(hash_result)) {
        std::abort();
    }

    std::ostringstream manifest;
    manifest << "MANIFEST_V1\n"
             << "fixture_id=" << workload.fixture_id << '\n'
             << "schema_version=REPLAY_V1\n"
             << "log_sha256=" << std::get<std::string>(hash_result) << '\n'
             << "market=" << workload.market << '\n'
             << "symbol=BTCUSDT\n"
             << "price_scale=" << workload.price_scale << '\n'
             << "quantity_scale=" << workload.quantity_scale << '\n'
             << "policy=" << workload.policy << '\n'
             << "event_count=" << workload.events.size() << '\n'
             << "provenance_generator=" << kSmallGeneratorVersion << '\n'
             << "provenance_seed=548746690337\n"
             << "provenance_source=synthetic\n";
    const auto loaded = replay::load_fixture(replay_log, manifest.str());
    if (!std::holds_alternative<replay::ReplayFixture>(loaded)) {
        std::abort();
    }
    return std::get<replay::ReplayFixture>(loaded);
}

} // namespace

replay::ReplayFixture make_spot_small_workload() { return materialize(generate_spot()); }

replay::ReplayFixture make_usdm_small_workload() { return materialize(generate_usdm()); }

} // namespace bmd_projection::m5::phase3

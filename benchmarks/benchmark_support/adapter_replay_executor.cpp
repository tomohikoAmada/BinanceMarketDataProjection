#include "adapter_replay_executor.hpp"

#include "core_replay_executor.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>
#include <binance_market_data/projection/v1/snapshots.pb.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

namespace adapter = binance_market_data::projection_adapter::v1;
namespace core = binance_market_data::projection::v1;

constexpr std::uint64_t kNoUpdateIdMarker = 0xFFFF'FFFF'FFFF'FFFFULL;

[[nodiscard]] core::DecimalScale required_scale(std::uint32_t value) {
    const auto scale = core::DecimalScale::create(value);
    if (!scale.has_value()) {
        std::abort();
    }
    return *scale;
}

[[nodiscard]] core::SequencePolicyKind core_policy(replay::SequencePolicy policy) noexcept {
    return policy == replay::SequencePolicy::Spot ? core::SequencePolicyKind::Spot
                                                  : core::SequencePolicyKind::UsdMPerpetual;
}

[[nodiscard]] std::uint64_t fold_apply(std::uint64_t checksum, const core::ApplyResult& result) {
    checksum = replay_checksum_append(
        checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.disposition)));
    checksum = replay_checksum_append(
        checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after)));
    return replay_checksum_append(checksum, result.last_update_id_after.has_value()
                                                ? result.last_update_id_after->value()
                                                : kNoUpdateIdMarker);
}

[[nodiscard]] std::uint64_t fold_install(std::uint64_t checksum,
                                         const core::InstallResult& result) {
    checksum = replay_checksum_append(
        checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.disposition)));
    checksum = replay_checksum_append(
        checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after)));
    return replay_checksum_append(checksum, result.last_update_id_after.has_value()
                                                ? result.last_update_id_after->value()
                                                : kNoUpdateIdMarker);
}

} // namespace

AdapterReplayExecutor::AdapterReplayExecutor(
    const replay::ReplayFixture& fixture, std::vector<adapter_support::PreconstructedEntry> entries)
    : fixture_{&fixture}, entries_{std::move(entries)},
      numeric_spec_{required_scale(fixture.identity.numeric_spec.price_scale),
                    required_scale(fixture.identity.numeric_spec.quantity_scale)},
      policy_{core_policy(fixture.identity.sequence_policy)} {}

std::uint64_t AdapterReplayExecutor::execute_event(std::size_t event_index,
                                                   core::BookProjection& projection,
                                                   std::uint64_t checksum) const {
    const auto& entry = entries_.at(event_index);
    switch (entry.kind) {
    case adapter_support::PreconstructedKind::Baseline: {
        const auto adapted = adapter::adapt_exchange_depth_snapshot(
            entry.baseline_wire, entry.conversion_spec, entry.expected);
        checksum = replay_checksum_append(checksum, adapted.index());
        if (const auto* failure = std::get_if<adapter::AdapterError>(&adapted)) {
            return replay_checksum_append(
                checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(failure->code)));
        }
        const auto& owner = std::get<adapter::AdaptedBookBaseline>(adapted);
        checksum = replay_checksum_append(checksum, owner.metadata().observed_quality.size());
        const auto installed = owner.install_into(projection);
        if (const auto* failure = std::get_if<adapter::AdapterError>(&installed)) {
            return replay_checksum_append(
                checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(failure->code)));
        }
        return fold_install(checksum, std::get<core::InstallResult>(installed));
    }
    case adapter_support::PreconstructedKind::Update: {
        const auto adapted =
            adapter::adapt_depth_update(entry.update_wire, entry.conversion_spec, entry.expected);
        checksum = replay_checksum_append(checksum, adapted.index());
        if (const auto* failure = std::get_if<adapter::AdapterError>(&adapted)) {
            return replay_checksum_append(
                checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(failure->code)));
        }
        const auto& owner = std::get<adapter::AdaptedDepthBatch>(adapted);
        checksum = replay_checksum_append(checksum, owner.metadata().observed_quality.size());
        const auto applied = owner.apply_to(projection);
        if (const auto* failure = std::get_if<adapter::AdapterError>(&applied)) {
            return replay_checksum_append(
                checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(failure->code)));
        }
        return fold_apply(checksum, std::get<core::ApplyResult>(applied));
    }
    case adapter_support::PreconstructedKind::Rebaseline: {
        const auto adapted = adapter::adapt_exchange_depth_snapshot(
            entry.baseline_wire, entry.conversion_spec, entry.expected);
        checksum = replay_checksum_append(checksum, adapted.index());
        if (const auto* failure = std::get_if<adapter::AdapterError>(&adapted)) {
            return replay_checksum_append(
                checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(failure->code)));
        }
        const auto& owner = std::get<adapter::AdaptedBookBaseline>(adapted);
        checksum = replay_checksum_append(checksum, owner.metadata().observed_quality.size());
        const auto installed = owner.install_into(projection);
        if (const auto* failure = std::get_if<adapter::AdapterError>(&installed)) {
            return replay_checksum_append(
                checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(failure->code)));
        }
        return fold_install(checksum, std::get<core::InstallResult>(installed));
    }
    case adapter_support::PreconstructedKind::Reset:
        projection.reset();
        return replay_checksum_append(
            checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(projection.status())));
    case adapter_support::PreconstructedKind::Snapshot: {
        const auto produced = adapter::make_local_order_book_snapshot(
            projection, entry.snapshot_context, entry.snapshot_options);
        checksum = replay_checksum_append(checksum, produced.index());
        if (const auto* failure = std::get_if<adapter::AdapterError>(&produced)) {
            return replay_checksum_append(
                checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(failure->code)));
        }
        const auto& snapshot = std::get<core::LocalOrderBookSnapshot>(produced);
        checksum =
            replay_checksum_append(checksum, static_cast<std::uint64_t>(snapshot.bids_size()));
        checksum =
            replay_checksum_append(checksum, static_cast<std::uint64_t>(snapshot.asks_size()));
        checksum = replay_checksum_append(checksum, snapshot.last_update_id());
        return replay_checksum_append(checksum, snapshot.synchronized() ? 1 : 0);
    }
    case adapter_support::PreconstructedKind::Metadata:
        return replay_checksum_append(
            checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(projection.status())));
    case adapter_support::PreconstructedKind::MalformedRange: {
        const auto range = core::UpdateRange::try_create(core::UpdateId{entry.malformed_first},
                                                         core::UpdateId{entry.malformed_final});
        return replay_checksum_append(checksum, range.has_value() ? 1 : 0);
    }
    }
    return checksum;
}

std::uint64_t AdapterReplayExecutor::finalize_checksum(const core::BookProjection& projection,
                                                       std::uint64_t checksum) {
    return finalize_projection_checksum(projection, checksum);
}

std::uint64_t AdapterReplayExecutor::run(core::BookProjection& projection) const {
    std::uint64_t checksum = kReplayChecksumSeed;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        checksum = execute_event(index, projection, checksum);
    }
    return finalize_checksum(projection, checksum);
}

} // namespace bmd_projection::m5::benchmark

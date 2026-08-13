#include "core_replay_executor.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/order_book/book_side.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

namespace core = binance_market_data::projection::v1;

constexpr std::uint64_t kKindInstall = 1;
constexpr std::uint64_t kKindRebaseline = 2;
constexpr std::uint64_t kKindDepthUpdate = 3;
constexpr std::uint64_t kKindReset = 4;
constexpr std::uint64_t kKindSnapshotRequest = 5;
constexpr std::uint64_t kKindMetadata = 6;
constexpr std::uint64_t kKindMalformedRange = 7;

constexpr std::uint64_t kParseErrorDisposition = 0xFFEE'DDCC'BBAA'9988ULL;
constexpr std::uint64_t kNoUpdateIdMarker = 0xFFFF'FFFF'FFFF'FFFFULL;
constexpr std::uint64_t kNoDisposition = 0xFFFF'FFFF'FFFF'FFFEULL;

[[nodiscard]] std::uint64_t operation_kind_code(const replay::Operation& operation) noexcept {
    if (std::holds_alternative<replay::InstallBaselineOp>(operation)) {
        return kKindInstall;
    }
    if (std::holds_alternative<replay::RebaselineOp>(operation)) {
        return kKindRebaseline;
    }
    if (std::holds_alternative<replay::DepthUpdateOp>(operation)) {
        return kKindDepthUpdate;
    }
    if (std::holds_alternative<replay::ResetOp>(operation)) {
        return kKindReset;
    }
    if (std::holds_alternative<replay::SnapshotRequestOp>(operation)) {
        return kKindSnapshotRequest;
    }
    if (std::holds_alternative<replay::AdapterMetadataOp>(operation)) {
        return kKindMetadata;
    }
    return kKindMalformedRange;
}

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

[[nodiscard]] core::BookSide core_side(replay::Side side) noexcept {
    return side == replay::Side::Bid ? core::BookSide::Bid : core::BookSide::Ask;
}

} // namespace

std::uint64_t fold_evidence(std::uint64_t checksum, const EventEvidence& evidence) {
    checksum = replay_checksum_append(checksum, evidence.kind);
    checksum = replay_checksum_append(checksum, evidence.disposition);
    checksum = replay_checksum_append(checksum, evidence.status);
    return replay_checksum_append(checksum, evidence.update_id);
}

CoreReplayExecutor::CoreReplayExecutor(const replay::ReplayFixture& fixture)
    : fixture_{&fixture},
      numeric_spec_{required_scale(fixture.identity.numeric_spec.price_scale),
                    required_scale(fixture.identity.numeric_spec.quantity_scale)},
      policy_{core_policy(fixture.identity.sequence_policy)} {
    std::size_t max_book_levels = 0;
    std::size_t max_updates = 0;
    for (const auto& operation : fixture_->replay.operations) {
        if (const auto* install = std::get_if<replay::InstallBaselineOp>(&operation)) {
            max_book_levels =
                std::max(max_book_levels, install->bids.size() + install->asks.size());
        } else if (const auto* rebaseline = std::get_if<replay::RebaselineOp>(&operation)) {
            max_book_levels =
                std::max(max_book_levels, rebaseline->bids.size() + rebaseline->asks.size());
        } else if (const auto* update = std::get_if<replay::DepthUpdateOp>(&operation)) {
            max_updates = std::max(max_updates, update->levels.size());
        }
    }
    scratch_book_levels_.resize(max_book_levels,
                                core::BookLevel{core::PriceUnits::create(1).value(),
                                                core::QuantityUnits::create(1).value()});
    scratch_updates_.resize(max_updates, core::LevelUpdate{core::BookSide::Bid,
                                                           core::PriceUnits::create(1).value(),
                                                           core::QuantityUnits::create(1).value()});
}

bool CoreReplayExecutor::parse_levels(const std::vector<replay::LevelInput>& levels,
                                      std::size_t side_offset) const {
    std::size_t position = 0;
    for (const auto& level : levels) {
        const auto price = core::parse_price(level.price, numeric_spec_.price_scale);
        const auto quantity = core::parse_quantity(level.quantity, numeric_spec_.quantity_scale);
        if (!std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(price) ||
            !std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(quantity)) {
            return false;
        }
        scratch_book_levels_[side_offset + position] = {
            std::get<core::ParsedDecimal<core::PriceUnits>>(price).value,
            std::get<core::ParsedDecimal<core::QuantityUnits>>(quantity).value};
        ++position;
    }
    return true;
}

EventEvidence
CoreReplayExecutor::execute_install(core::BookProjection& projection,
                                    const replay::InstallBaselineOp& operation) const {
    const auto kind = kKindInstall;
    if (!parse_levels(operation.bids, 0) || !parse_levels(operation.asks, operation.bids.size())) {
        return {kind, kParseErrorDisposition, kNoDisposition, kNoUpdateIdMarker};
    }
    const std::span<const core::BookLevel> book_levels{scratch_book_levels_};
    const auto result = projection.install_baseline(
        {core::UpdateId{operation.last_update_id}, book_levels.first(operation.bids.size()),
         book_levels.subspan(operation.bids.size(), operation.asks.size())});
    std::uint64_t update_id_after = kNoUpdateIdMarker;
    if (result.last_update_id_after.has_value()) {
        update_id_after = result.last_update_id_after->value();
    }
    return {kind, static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.disposition)),
            static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after)),
            update_id_after};
}

EventEvidence
CoreReplayExecutor::execute_depth_update(core::BookProjection& projection,
                                         const replay::DepthUpdateOp& operation) const {
    const auto kind = kKindDepthUpdate;
    const auto count = operation.levels.size();
    for (std::size_t index = 0; index < count; ++index) {
        const auto& level = operation.levels[index];
        const auto price = core::parse_price(level.price, numeric_spec_.price_scale);
        const auto quantity = core::parse_quantity(level.quantity, numeric_spec_.quantity_scale);
        if (!std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(price) ||
            !std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(quantity)) {
            return {kind, kParseErrorDisposition, kNoDisposition, kNoUpdateIdMarker};
        }
        scratch_updates_[index] = {
            core_side(level.side), std::get<core::ParsedDecimal<core::PriceUnits>>(price).value,
            std::get<core::ParsedDecimal<core::QuantityUnits>>(quantity).value};
    }
    const auto range = core::UpdateRange::try_create(core::UpdateId{operation.first_update_id},
                                                     core::UpdateId{operation.final_update_id});
    if (!range.has_value()) {
        return {kind, kParseErrorDisposition, kNoDisposition, kNoUpdateIdMarker};
    }
    std::optional<core::UpdateId> previous;
    if (operation.previous_final.has_value()) {
        previous = core::UpdateId{*operation.previous_final};
    }
    const auto result = projection.apply(
        {*range, previous, std::span<const core::LevelUpdate>{scratch_updates_.data(), count}});
    std::uint64_t update_id_after = kNoUpdateIdMarker;
    if (result.last_update_id_after.has_value()) {
        update_id_after = result.last_update_id_after->value();
    }
    return {kind, static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.disposition)),
            static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after)),
            update_id_after};
}

EventEvidence CoreReplayExecutor::execute_event(core::BookProjection& projection,
                                                std::size_t event_index) const {
    const auto& operation = fixture_->replay.operations.at(event_index);
    const auto kind = operation_kind_code(operation);
    if (const auto* install = std::get_if<replay::InstallBaselineOp>(&operation)) {
        return execute_install(projection, *install);
    }
    if (const auto* rebaseline = std::get_if<replay::RebaselineOp>(&operation)) {
        const auto kind_rebaseline = kKindRebaseline;
        if (!parse_levels(rebaseline->bids, 0) ||
            !parse_levels(rebaseline->asks, rebaseline->bids.size())) {
            return {kind_rebaseline, kParseErrorDisposition, kNoDisposition, kNoUpdateIdMarker};
        }
        const std::span<const core::BookLevel> book_levels{scratch_book_levels_};
        const auto result = projection.install_baseline(
            {core::UpdateId{rebaseline->last_update_id}, book_levels.first(rebaseline->bids.size()),
             book_levels.subspan(rebaseline->bids.size(), rebaseline->asks.size())});
        std::uint64_t update_id_after = kNoUpdateIdMarker;
        if (result.last_update_id_after.has_value()) {
            update_id_after = result.last_update_id_after->value();
        }
        return {kind_rebaseline,
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.disposition)),
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after)),
                update_id_after};
    }
    if (const auto* update = std::get_if<replay::DepthUpdateOp>(&operation)) {
        return execute_depth_update(projection, *update);
    }
    if (std::holds_alternative<replay::ResetOp>(operation)) {
        projection.reset();
        return {kKindReset, kNoDisposition,
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(projection.status())),
                kNoUpdateIdMarker};
    }
    if (std::holds_alternative<replay::SnapshotRequestOp>(operation)) {
        // Core-only mode: snapshot production is the M4 adapter boundary and
        // is absent from this executable path.
        return {kKindSnapshotRequest, kNoDisposition,
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(projection.status())),
                kNoUpdateIdMarker};
    }
    if (std::holds_alternative<replay::AdapterMetadataOp>(operation)) {
        return {kKindMetadata, kNoDisposition,
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(projection.status())),
                kNoUpdateIdMarker};
    }
    const auto& malformed = std::get<replay::MalformedRangeOp>(operation);
    const auto range = core::UpdateRange::try_create(core::UpdateId{malformed.first_update_id},
                                                     core::UpdateId{malformed.final_update_id});
    return {kind, range.has_value() ? 1ULL : 0ULL, kNoDisposition, kNoUpdateIdMarker};
}

std::uint64_t finalize_projection_checksum(const core::BookProjection& projection,
                                           std::uint64_t checksum) {
    checksum = replay_checksum_append(
        checksum, static_cast<std::uint64_t>(static_cast<std::uint8_t>(projection.status())));
    const auto last_update_id = projection.last_update_id();
    checksum = replay_checksum_append(checksum, last_update_id.has_value() ? last_update_id->value()
                                                                           : kNoUpdateIdMarker);
    const auto& book = projection.diagnostic_book();
    checksum = replay_checksum_append(checksum, book.level_count(core::BookSide::Bid));
    checksum = replay_checksum_append(checksum, book.level_count(core::BookSide::Ask));
    const auto best_bid = book.best_bid();
    std::uint64_t best_bid_price = kNoUpdateIdMarker;
    if (best_bid.has_value()) {
        best_bid_price = static_cast<std::uint64_t>(best_bid->price.value());
    }
    checksum = replay_checksum_append(checksum, best_bid_price);
    const auto best_ask = book.best_ask();
    std::uint64_t best_ask_price = kNoUpdateIdMarker;
    if (best_ask.has_value()) {
        best_ask_price = static_cast<std::uint64_t>(best_ask->price.value());
    }
    checksum = replay_checksum_append(checksum, best_ask_price);
    checksum = replay_checksum_append(checksum, projection.synchronized_book().has_value() ? 1 : 0);
    return checksum;
}

std::uint64_t CoreReplayExecutor::finalize_checksum(const core::BookProjection& projection,
                                                    std::uint64_t checksum) {
    return finalize_projection_checksum(projection, checksum);
}

std::uint64_t CoreReplayExecutor::run(core::BookProjection& projection) const {
    std::uint64_t checksum = kReplayChecksumSeed;
    for (std::size_t index = 0; index < fixture_->replay.operations.size(); ++index) {
        checksum = fold_evidence(checksum, execute_event(projection, index));
    }
    return finalize_checksum(projection, checksum);
}

std::size_t CoreReplayExecutor::event_count() const noexcept {
    return fixture_->replay.operations.size();
}

std::string replay_fixture_identity(const replay::ReplayFixture& fixture) {
    std::string text;
    const auto append = [&text](std::string_view key, std::string_view value) {
        text += key;
        text += '=';
        text += value;
        text += '\n';
    };
    append("workload_id", fixture.identity.fixture_id);
    append("market", fixture.identity.market == replay::Market::Spot ? "Spot" : "UsdMPerpetual");
    append("symbol", fixture.identity.symbol);
    append("price_scale", std::to_string(fixture.identity.numeric_spec.price_scale));
    append("quantity_scale", std::to_string(fixture.identity.numeric_spec.quantity_scale));
    append("policy", fixture.identity.sequence_policy == replay::SequencePolicy::Spot
                         ? "Spot"
                         : "UsdMPerpetual");
    append("event_count", std::to_string(fixture.replay.operations.size()));
    append("canonical_log_sha256", fixture.canonical_log_sha256);
    for (const auto& [key, value] : fixture.manifest.provenance) {
        append("provenance_" + key, value);
    }
    return text;
}

} // namespace bmd_projection::m5::benchmark

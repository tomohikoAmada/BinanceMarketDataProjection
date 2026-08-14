#include "core_replay_executor.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/order_book/book_side.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <algorithm>
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

[[nodiscard]] core::PriceUnits placeholder_price() {
    const auto value = core::PriceUnits::create(1);
    if (!value.has_value()) {
        std::abort();
    }
    return *value;
}

[[nodiscard]] core::QuantityUnits placeholder_quantity() {
    const auto value = core::QuantityUnits::create(1);
    if (!value.has_value()) {
        std::abort();
    }
    return *value;
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
    initialize_scratch();
    prepare_latency_events();
}

void CoreReplayExecutor::initialize_scratch() {
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
                                core::BookLevel{placeholder_price(), placeholder_quantity()});
    scratch_updates_.resize(max_updates, core::LevelUpdate{core::BookSide::Bid, placeholder_price(),
                                                           placeholder_quantity()});
}

void CoreReplayExecutor::prepare_latency_events() {
    prepared_events_.reserve(fixture_->replay.operations.size());
    for (const auto& operation : fixture_->replay.operations) {
        prepared_events_.push_back(std::visit(
            [this](const auto& concrete) { return this->prepare_event(concrete); }, operation));
    }
}

bool CoreReplayExecutor::prepare_book_levels(const std::vector<replay::LevelInput>& input,
                                             std::vector<core::BookLevel>& output) const {
    output.reserve(input.size());
    for (const auto& level : input) {
        const auto price = core::parse_price(level.price, numeric_spec_.price_scale);
        const auto quantity = core::parse_quantity(level.quantity, numeric_spec_.quantity_scale);
        if (!std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(price) ||
            !std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(quantity)) {
            return false;
        }
        output.push_back({std::get<core::ParsedDecimal<core::PriceUnits>>(price).value,
                          std::get<core::ParsedDecimal<core::QuantityUnits>>(quantity).value});
    }
    return true;
}

CoreReplayExecutor::PreparedEvent
CoreReplayExecutor::prepare_event(const replay::InstallBaselineOp& operation) {
    PreparedEvent prepared;
    prepared.kind = PreparedKind::Install;
    prepared.final_id = operation.last_update_id;
    prepared_inputs_valid_ = prepare_book_levels(operation.bids, prepared.bids) &&
                             prepare_book_levels(operation.asks, prepared.asks) &&
                             prepared_inputs_valid_;
    return prepared;
}

CoreReplayExecutor::PreparedEvent
CoreReplayExecutor::prepare_event(const replay::RebaselineOp& operation) {
    PreparedEvent prepared;
    prepared.kind = PreparedKind::Rebaseline;
    prepared.final_id = operation.last_update_id;
    prepared_inputs_valid_ = prepare_book_levels(operation.bids, prepared.bids) &&
                             prepare_book_levels(operation.asks, prepared.asks) &&
                             prepared_inputs_valid_;
    return prepared;
}

CoreReplayExecutor::PreparedEvent
CoreReplayExecutor::prepare_event(const replay::DepthUpdateOp& operation) {
    PreparedEvent prepared;
    prepared.kind = PreparedKind::DepthUpdate;
    prepared.first_id = operation.first_update_id;
    prepared.final_id = operation.final_update_id;
    prepared.range = core::UpdateRange::try_create(core::UpdateId{operation.first_update_id},
                                                   core::UpdateId{operation.final_update_id});
    if (operation.previous_final.has_value()) {
        prepared.previous_final = core::UpdateId{*operation.previous_final};
    }
    prepared.updates.reserve(operation.levels.size());
    for (const auto& level : operation.levels) {
        const auto price = core::parse_price(level.price, numeric_spec_.price_scale);
        const auto quantity = core::parse_quantity(level.quantity, numeric_spec_.quantity_scale);
        if (!std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(price) ||
            !std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(quantity)) {
            prepared_inputs_valid_ = false;
            break;
        }
        prepared.updates.push_back(
            {core_side(level.side), std::get<core::ParsedDecimal<core::PriceUnits>>(price).value,
             std::get<core::ParsedDecimal<core::QuantityUnits>>(quantity).value});
    }
    prepared_inputs_valid_ = prepared.range.has_value() && prepared_inputs_valid_;
    return prepared;
}

CoreReplayExecutor::PreparedEvent
CoreReplayExecutor::prepare_event(const replay::ResetOp& operation) {
    static_cast<void>(operation);
    PreparedEvent prepared;
    prepared.kind = PreparedKind::Reset;
    return prepared;
}

CoreReplayExecutor::PreparedEvent
CoreReplayExecutor::prepare_event(const replay::SnapshotRequestOp& operation) {
    static_cast<void>(operation);
    PreparedEvent prepared;
    prepared.kind = PreparedKind::SnapshotRequest;
    return prepared;
}

CoreReplayExecutor::PreparedEvent
CoreReplayExecutor::prepare_event(const replay::AdapterMetadataOp& operation) {
    static_cast<void>(operation);
    PreparedEvent prepared;
    prepared.kind = PreparedKind::Metadata;
    return prepared;
}

CoreReplayExecutor::PreparedEvent
CoreReplayExecutor::prepare_event(const replay::MalformedRangeOp& operation) {
    PreparedEvent prepared;
    prepared.kind = PreparedKind::MalformedRange;
    prepared.first_id = operation.first_update_id;
    prepared.final_id = operation.final_update_id;
    return prepared;
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

EventEvidence CoreReplayExecutor::execute_prepared_event(core::BookProjection& projection,
                                                         std::size_t event_index) const {
    const auto& event = prepared_events_.at(event_index);
    const auto status_evidence = [&projection](std::uint64_t kind) {
        return EventEvidence{
            kind, kNoDisposition,
            static_cast<std::uint64_t>(static_cast<std::uint8_t>(projection.status())),
            kNoUpdateIdMarker};
    };
    if (event.kind == PreparedKind::Install || event.kind == PreparedKind::Rebaseline) {
        const auto result =
            projection.install_baseline({core::UpdateId{event.final_id}, event.bids, event.asks});
        return {event.kind == PreparedKind::Install ? kKindInstall : kKindRebaseline,
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.disposition)),
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after)),
                result.last_update_id_after.has_value() ? result.last_update_id_after->value()
                                                        : kNoUpdateIdMarker};
    }
    if (event.kind == PreparedKind::DepthUpdate) {
        if (!event.range.has_value()) {
            return {kKindDepthUpdate, kParseErrorDisposition, kNoDisposition, kNoUpdateIdMarker};
        }
        const auto result = projection.apply({*event.range, event.previous_final, event.updates});
        return {kKindDepthUpdate,
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.disposition)),
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after)),
                result.last_update_id_after.has_value() ? result.last_update_id_after->value()
                                                        : kNoUpdateIdMarker};
    }
    if (event.kind == PreparedKind::Reset) {
        projection.reset();
        return status_evidence(kKindReset);
    }
    if (event.kind == PreparedKind::SnapshotRequest) {
        return status_evidence(kKindSnapshotRequest);
    }
    if (event.kind == PreparedKind::Metadata) {
        return status_evidence(kKindMetadata);
    }
    const auto range = core::UpdateRange::try_create(core::UpdateId{event.first_id},
                                                     core::UpdateId{event.final_id});
    return {kKindMalformedRange, range.has_value() ? 1ULL : 0ULL, kNoDisposition,
            kNoUpdateIdMarker};
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
    append("generated_workload_sha256", fixture.canonical_log_sha256);
    append("generator_schema", "M5_PHASE6_REPLAY_V1");
    const auto provenance_value = [&fixture](std::string_view key) {
        const auto found =
            std::find_if(fixture.manifest.provenance.begin(), fixture.manifest.provenance.end(),
                         [key](const auto& entry) { return entry.first == key; });
        return found == fixture.manifest.provenance.end() ? std::string{"not_applicable"}
                                                          : found->second;
    };
    append("generator_version", provenance_value("generator"));
    append("seed", provenance_value("seed"));
    append("logical_items_per_iteration", std::to_string(fixture.replay.operations.size()));
    for (const auto& [key, value] : fixture.manifest.provenance) {
        append("provenance_" + key, value);
    }
    return text;
}

} // namespace bmd_projection::m5::benchmark

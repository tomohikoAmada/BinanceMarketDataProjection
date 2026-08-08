#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <binance_market_data/common/v1/enums.pb.h>
#include <binance_market_data/common/v1/metadata.pb.h>
#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace binance_market_data::projection_adapter::v1 {
namespace {

namespace core = ::binance_market_data::projection::v1;
namespace common_wire = ::binance_market_data::common::v1;

constexpr std::string_view kSnapshotSchema{"exchange-depth-snapshot.v1"};
constexpr std::string_view kUpdateSchema{"depth-update.v1"};
constexpr std::string_view kLocalSnapshotSchema{"local-order-book-snapshot.v1"};

[[nodiscard]] constexpr AdapterError error(AdapterErrorCode code, AdapterField field) noexcept {
    return {code, field, std::nullopt, std::nullopt};
}

[[nodiscard]] constexpr AdapterError enum_error(AdapterErrorCode code, AdapterField field,
                                                std::int32_t value) noexcept {
    return {code, field, std::nullopt, value};
}

[[nodiscard]] AdapterError decimal_error(AdapterErrorCode code, AdapterField field,
                                         core::DecimalError detail) noexcept {
    return {code, field, detail, std::nullopt};
}

[[nodiscard]] constexpr bool is_ascii_alphanumeric(char value) noexcept {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9');
}

[[nodiscard]] constexpr bool is_identifier_tail(char value) noexcept {
    return is_ascii_alphanumeric(value) || value == '.' || value == '_' || value == ':' ||
           value == '/' || value == '-';
}

[[nodiscard]] bool is_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > 128 || !is_ascii_alphanumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), is_identifier_tail);
}

[[nodiscard]] bool is_symbol(std::string_view value) noexcept {
    if (value.size() < 2 || value.size() > 20) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9');
    });
}

[[nodiscard]] constexpr bool is_ascii_whitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' ||
           value == '\v';
}

[[nodiscard]] bool is_non_empty_text(std::string_view value) noexcept {
    return !value.empty() && value.size() <= 256 && !is_ascii_whitespace(value.front()) &&
           !is_ascii_whitespace(value.back());
}

[[nodiscard]] AdapterResult<core::SequencePolicyKind>
map_market(common_wire::Market market) noexcept {
    switch (market) {
    case common_wire::MARKET_SPOT:
        return core::SequencePolicyKind::Spot;
    case common_wire::MARKET_USD_M_PERPETUAL:
        return core::SequencePolicyKind::UsdMPerpetual;
    case common_wire::MARKET_UNSPECIFIED:
        return enum_error(AdapterErrorCode::UnspecifiedEnum, AdapterField::Market,
                          static_cast<std::int32_t>(market));
    default:
        return enum_error(AdapterErrorCode::UnknownEnumValue, AdapterField::Market,
                          static_cast<std::int32_t>(market));
    }
}

[[nodiscard]] std::optional<AdapterError> validate_venue(common_wire::Venue venue) noexcept {
    switch (venue) {
    case common_wire::VENUE_BINANCE:
        return std::nullopt;
    case common_wire::VENUE_UNSPECIFIED:
        return enum_error(AdapterErrorCode::UnspecifiedEnum, AdapterField::Venue,
                          static_cast<std::int32_t>(venue));
    default:
        return enum_error(AdapterErrorCode::UnsupportedVenue, AdapterField::Venue,
                          static_cast<std::int32_t>(venue));
    }
}

[[nodiscard]] std::optional<AdapterError> validate_stream(common_wire::Stream stream) noexcept {
    switch (stream) {
    case common_wire::STREAM_DIFF_DEPTH:
        return std::nullopt;
    case common_wire::STREAM_UNSPECIFIED:
        return enum_error(AdapterErrorCode::UnspecifiedEnum, AdapterField::Stream,
                          static_cast<std::int32_t>(stream));
    case common_wire::STREAM_AGG_TRADE:
    case common_wire::STREAM_BOOK_TICKER:
    case common_wire::STREAM_DEPTH_SNAPSHOT:
        return enum_error(AdapterErrorCode::UnexpectedStream, AdapterField::Stream,
                          static_cast<std::int32_t>(stream));
    default:
        return enum_error(AdapterErrorCode::UnknownEnumValue, AdapterField::Stream,
                          static_cast<std::int32_t>(stream));
    }
}

[[nodiscard]] std::optional<HostQualityFact>
map_inbound_quality(common_wire::QualityFlag flag, AdapterError& mapping_error) noexcept {
    switch (flag) {
    case common_wire::QUALITY_FLAG_DUPLICATE:
        return HostQualityFact::Duplicate;
    case common_wire::QUALITY_FLAG_OUT_OF_ORDER:
        return HostQualityFact::OutOfOrder;
    case common_wire::QUALITY_FLAG_ORDERBOOK_RESYNC:
        return HostQualityFact::OrderBookResync;
    case common_wire::QUALITY_FLAG_SNAPSHOT_TOO_OLD:
        return HostQualityFact::SnapshotTooOld;
    case common_wire::QUALITY_FLAG_BOOTSTRAP_BUFFER_OVERFLOW:
        return HostQualityFact::BootstrapBufferOverflow;
    case common_wire::QUALITY_FLAG_RECOVERED_TAIL:
        return HostQualityFact::RecoveredTail;
    case common_wire::QUALITY_FLAG_MALFORMED_PAYLOAD:
        return HostQualityFact::MalformedPayload;
    case common_wire::QUALITY_FLAG_EXCHANGE_TIME_MISSING:
        return HostQualityFact::ExchangeTimeMissing;
    case common_wire::QUALITY_FLAG_RECEIVE_CLOCK_DISCONTINUITY:
        return HostQualityFact::ReceiveClockDiscontinuity;
    case common_wire::QUALITY_FLAG_SLOW_CONSUMER_GAP:
        return HostQualityFact::SlowConsumerGap;
    case common_wire::QUALITY_FLAG_PRODUCER_RESTART:
        return HostQualityFact::ProducerRestart;
    case common_wire::QUALITY_FLAG_OVERLAP:
        return HostQualityFact::Overlap;
    case common_wire::QUALITY_FLAG_IDENTITY_CONFLICT:
        return HostQualityFact::IdentityConflict;
    case common_wire::QUALITY_FLAG_SEQUENCE_GAP:
    case common_wire::QUALITY_FLAG_SNAPSHOT_BRIDGE_PENDING:
    case common_wire::QUALITY_FLAG_CROSSED_BOOK:
        return std::nullopt;
    case common_wire::QUALITY_FLAG_UNSPECIFIED:
        mapping_error = enum_error(AdapterErrorCode::UnspecifiedEnum, AdapterField::QualityFlag,
                                   static_cast<std::int32_t>(flag));
        return std::nullopt;
    default:
        mapping_error = enum_error(AdapterErrorCode::UnknownEnumValue, AdapterField::QualityFlag,
                                   static_cast<std::int32_t>(flag));
        return std::nullopt;
    }
}

[[nodiscard]] AdapterResult<AdaptedMetadata>
adapt_quality(const google::protobuf::RepeatedField<int>& quality_flags) {
    constexpr auto kHostFactCount = static_cast<std::size_t>(HostQualityFact::IdentityConflict) + 1;
    std::array<bool, kHostFactCount> present{};
    for (const auto raw_flag : quality_flags) {
        const auto flag = static_cast<common_wire::QualityFlag>(raw_flag);
        auto mapping_error = error(AdapterErrorCode::UnknownEnumValue, AdapterField::QualityFlag);
        const auto mapped = map_inbound_quality(flag, mapping_error);
        if (flag == common_wire::QUALITY_FLAG_UNSPECIFIED ||
            !common_wire::QualityFlag_IsValid(raw_flag)) {
            return mapping_error;
        }
        if (mapped.has_value()) {
            present.at(static_cast<std::size_t>(*mapped)) = true;
        }
    }

    AdaptedMetadata metadata;
    for (std::size_t index = 0; index < present.size(); ++index) {
        if (present.at(index)) {
            metadata.observed_quality.push_back(static_cast<HostQualityFact>(index));
        }
    }
    return metadata;
}

struct LevelFields final {
    AdapterField price;
    AdapterField quantity;
};

template <typename WireLevel>
[[nodiscard]] AdapterResult<core::BookLevel>
adapt_book_level(const WireLevel& wire, core::NumericSpec spec, LevelFields fields) noexcept {
    const auto price_result = core::parse_price(wire.price(), spec.price_scale);
    if (const auto* detail = std::get_if<core::DecimalError>(&price_result)) {
        auto code = AdapterErrorCode::InvalidDecimal;
        if (detail->code == core::DecimalErrorCode::ZeroNotAllowed ||
            (!wire.price().empty() && wire.price().front() == '-')) {
            code = AdapterErrorCode::NonPositivePrice;
        } else if (detail->code == core::DecimalErrorCode::InexactScale) {
            code = AdapterErrorCode::ScaleMismatch;
        } else if (detail->code == core::DecimalErrorCode::Overflow) {
            code = AdapterErrorCode::NumericOverflow;
        }
        return decimal_error(code, fields.price, *detail);
    }

    const auto quantity_result = core::parse_quantity(wire.quantity(), spec.quantity_scale);
    if (const auto* detail = std::get_if<core::DecimalError>(&quantity_result)) {
        auto code = AdapterErrorCode::InvalidDecimal;
        if (!wire.quantity().empty() && wire.quantity().front() == '-') {
            code = AdapterErrorCode::NegativeQuantity;
        } else if (detail->code == core::DecimalErrorCode::InexactScale) {
            code = AdapterErrorCode::ScaleMismatch;
        } else if (detail->code == core::DecimalErrorCode::Overflow) {
            code = AdapterErrorCode::NumericOverflow;
        }
        return decimal_error(code, fields.quantity, *detail);
    }

    return core::BookLevel{
        std::get<core::ParsedDecimal<core::PriceUnits>>(price_result).value,
        std::get<core::ParsedDecimal<core::QuantityUnits>>(quantity_result).value};
}

template <typename RepeatedLevels>
[[nodiscard]] AdapterResult<std::vector<core::BookLevel>>
adapt_baseline_side(const RepeatedLevels& wire_levels, core::NumericSpec spec, bool bids) {
    std::vector<core::BookLevel> levels;
    levels.reserve(static_cast<std::size_t>(wire_levels.size()));
    for (const auto& wire_level : wire_levels) {
        const auto adapted =
            adapt_book_level(wire_level, spec,
                             {bids ? AdapterField::BidPrice : AdapterField::AskPrice,
                              bids ? AdapterField::BidQuantity : AdapterField::AskQuantity});
        if (const auto* failure = std::get_if<AdapterError>(&adapted)) {
            return *failure;
        }
        const auto level = std::get<core::BookLevel>(adapted);
        if (!levels.empty()) {
            const bool ordered =
                bids ? level.price < levels.back().price : level.price > levels.back().price;
            if (!ordered) {
                return error(AdapterErrorCode::InvalidOrdering,
                             bids ? AdapterField::BidPrice : AdapterField::AskPrice);
            }
        }
        levels.push_back(level);
    }
    return levels;
}

template <typename RepeatedLevels>
[[nodiscard]] AdapterResult<std::vector<core::LevelUpdate>>
adapt_updates(const RepeatedLevels& wire_levels, core::NumericSpec spec, core::BookSide side) {
    std::vector<core::LevelUpdate> updates;
    updates.reserve(static_cast<std::size_t>(wire_levels.size()));
    for (const auto& wire_level : wire_levels) {
        const bool bids = side == core::BookSide::Bid;
        const auto adapted =
            adapt_book_level(wire_level, spec,
                             {bids ? AdapterField::BidPrice : AdapterField::AskPrice,
                              bids ? AdapterField::BidQuantity : AdapterField::AskQuantity});
        if (const auto* failure = std::get_if<AdapterError>(&adapted)) {
            return *failure;
        }
        const auto level = std::get<core::BookLevel>(adapted);
        updates.push_back({side, level.price, level.quantity});
    }
    return updates;
}

[[nodiscard]] std::optional<AdapterError>
validate_expected_identity(std::string_view wire_symbol, core::SequencePolicyKind wire_policy,
                           const ExpectedIdentity& expected) noexcept {
    if (!is_symbol(wire_symbol)) {
        return error(AdapterErrorCode::InvalidIdentifier, AdapterField::Symbol);
    }
    if (!is_symbol(expected.symbol) || wire_symbol != expected.symbol) {
        return error(AdapterErrorCode::IdentityMismatch, AdapterField::Symbol);
    }
    if (wire_policy != expected.policy) {
        return error(AdapterErrorCode::IdentityMismatch, AdapterField::Market);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<AdapterError>
validate_required_text(std::string_view producer, std::string_view producer_version,
                       std::string_view identifier, AdapterField identifier_field) noexcept {
    if (!is_non_empty_text(producer)) {
        return error(producer.empty() ? AdapterErrorCode::MissingRequiredField
                                      : AdapterErrorCode::InvalidIdentifier,
                     AdapterField::Producer);
    }
    if (!is_non_empty_text(producer_version)) {
        return error(producer_version.empty() ? AdapterErrorCode::MissingRequiredField
                                              : AdapterErrorCode::InvalidIdentifier,
                     AdapterField::ProducerVersion);
    }
    if (!is_identifier(identifier)) {
        return error(identifier.empty() ? AdapterErrorCode::MissingRequiredField
                                        : AdapterErrorCode::InvalidIdentifier,
                     identifier_field);
    }
    return std::nullopt;
}

[[nodiscard]] AdapterResult<core::SequencePolicyKind>
validate_wire_identity(common_wire::Venue venue, common_wire::Market market,
                       std::string_view symbol, const ExpectedIdentity& expected) noexcept {
    if (const auto venue_error = validate_venue(venue)) {
        return *venue_error;
    }
    const auto mapped_market = map_market(market);
    if (const auto* failure = std::get_if<AdapterError>(&mapped_market)) {
        return *failure;
    }
    const auto* mapped_policy = std::get_if<core::SequencePolicyKind>(&mapped_market);
    if (mapped_policy == nullptr) {
        return error(AdapterErrorCode::UnsupportedMarket, AdapterField::Market);
    }
    const auto policy = *mapped_policy;
    if (const auto identity_error = validate_expected_identity(symbol, policy, expected)) {
        return *identity_error;
    }
    return policy;
}

[[nodiscard]] std::optional<AdapterError>
validate_binding(core::NumericSpec owner_spec, core::SequencePolicyKind owner_policy,
                 const core::BookProjection& target) noexcept {
    const auto target_spec = target.numeric_spec();
    if (owner_spec.price_scale != target_spec.price_scale) {
        return error(AdapterErrorCode::ProjectionNumericSpecMismatch,
                     AdapterField::ProjectionPriceScale);
    }
    if (owner_spec.quantity_scale != target_spec.quantity_scale) {
        return error(AdapterErrorCode::ProjectionNumericSpecMismatch,
                     AdapterField::ProjectionQuantityScale);
    }
    if (owner_policy != target.policy()) {
        return error(AdapterErrorCode::ProjectionPolicyMismatch, AdapterField::ProjectionPolicy);
    }
    return std::nullopt;
}

[[nodiscard]] common_wire::Market market_for_policy(core::SequencePolicyKind policy) noexcept {
    switch (policy) {
    case core::SequencePolicyKind::Spot:
        return common_wire::MARKET_SPOT;
    case core::SequencePolicyKind::UsdMPerpetual:
        return common_wire::MARKET_USD_M_PERPETUAL;
    }
    return common_wire::MARKET_UNSPECIFIED;
}

[[nodiscard]] AdapterResult<common_wire::SnapshotSource>
map_snapshot_origin(SnapshotOrigin source) noexcept {
    switch (source) {
    case SnapshotOrigin::GatewayLive:
        return common_wire::SNAPSHOT_SOURCE_GATEWAY_LIVE;
    case SnapshotOrigin::RecorderReplay:
        return common_wire::SNAPSHOT_SOURCE_RECORDER_REPLAY;
    case SnapshotOrigin::HistoryReplay:
        return common_wire::SNAPSHOT_SOURCE_HISTORY_REPLAY;
    }
    return enum_error(AdapterErrorCode::UnknownEnumValue, AdapterField::SnapshotSource,
                      static_cast<std::int32_t>(source));
}

[[nodiscard]] AdapterResult<common_wire::ResyncState>
map_gap_recovery(GapRecoveryState state) noexcept {
    switch (state) {
    case GapRecoveryState::ResyncRequired:
        return common_wire::RESYNC_STATE_RESYNC_REQUIRED;
    case GapRecoveryState::ResyncInProgress:
        return common_wire::RESYNC_STATE_RESYNC_IN_PROGRESS;
    case GapRecoveryState::ResyncFailed:
        return common_wire::RESYNC_STATE_RESYNC_FAILED;
    case GapRecoveryState::Synchronized:
    case GapRecoveryState::Recovered:
        return error(AdapterErrorCode::InvalidGapContext, AdapterField::GapRecoveryState);
    }
    return enum_error(AdapterErrorCode::UnknownEnumValue, AdapterField::GapRecoveryState,
                      static_cast<std::int32_t>(state));
}

[[nodiscard]] AdapterResult<common_wire::ReasonCode>
map_gap_reason(core::GapReason reason) noexcept {
    switch (reason) {
    case core::GapReason::SpotBootstrapForwardGap:
    case core::GapReason::SpotLiveForwardGap:
    case core::GapReason::FuturesBootstrapRangeMiss:
    case core::GapReason::FuturesMissingPreviousFinal:
    case core::GapReason::FuturesPreviousFinalMismatch:
        return common_wire::REASON_CODE_SEQUENCE_GAP_DETECTED;
    }
    return enum_error(AdapterErrorCode::UnknownEnumValue, AdapterField::CurrentGap,
                      static_cast<std::int32_t>(reason));
}

[[nodiscard]] AdapterResult<common_wire::QualityFlag>
map_host_quality(HostQualityFact fact, core::ProjectionStatus status) noexcept {
    switch (fact) {
    case HostQualityFact::Duplicate:
        return common_wire::QUALITY_FLAG_DUPLICATE;
    case HostQualityFact::OutOfOrder:
        return common_wire::QUALITY_FLAG_OUT_OF_ORDER;
    case HostQualityFact::OrderBookResync:
        if (status != core::ProjectionStatus::AwaitingBridge &&
            status != core::ProjectionStatus::NeedsResync) {
            return error(AdapterErrorCode::InvalidHostQualityCombination,
                         AdapterField::HostQualityFact);
        }
        return common_wire::QUALITY_FLAG_ORDERBOOK_RESYNC;
    case HostQualityFact::SnapshotTooOld:
        return common_wire::QUALITY_FLAG_SNAPSHOT_TOO_OLD;
    case HostQualityFact::BootstrapBufferOverflow:
        return common_wire::QUALITY_FLAG_BOOTSTRAP_BUFFER_OVERFLOW;
    case HostQualityFact::RecoveredTail:
        if (status != core::ProjectionStatus::Synchronized) {
            return error(AdapterErrorCode::InvalidHostQualityCombination,
                         AdapterField::HostQualityFact);
        }
        return common_wire::QUALITY_FLAG_RECOVERED_TAIL;
    case HostQualityFact::MalformedPayload:
        if (status != core::ProjectionStatus::AwaitingBridge &&
            status != core::ProjectionStatus::NeedsResync) {
            return error(AdapterErrorCode::InvalidHostQualityCombination,
                         AdapterField::HostQualityFact);
        }
        return common_wire::QUALITY_FLAG_MALFORMED_PAYLOAD;
    case HostQualityFact::ExchangeTimeMissing:
        return common_wire::QUALITY_FLAG_EXCHANGE_TIME_MISSING;
    case HostQualityFact::ReceiveClockDiscontinuity:
        return common_wire::QUALITY_FLAG_RECEIVE_CLOCK_DISCONTINUITY;
    case HostQualityFact::SlowConsumerGap:
        return common_wire::QUALITY_FLAG_SLOW_CONSUMER_GAP;
    case HostQualityFact::ProducerRestart:
        return common_wire::QUALITY_FLAG_PRODUCER_RESTART;
    case HostQualityFact::Overlap:
        return common_wire::QUALITY_FLAG_OVERLAP;
    case HostQualityFact::IdentityConflict:
        return common_wire::QUALITY_FLAG_IDENTITY_CONFLICT;
    }
    return enum_error(AdapterErrorCode::UnknownEnumValue, AdapterField::HostQualityFact,
                      static_cast<std::int32_t>(fact));
}

[[nodiscard]] constexpr std::size_t quality_rank(common_wire::QualityFlag flag) noexcept {
    switch (flag) {
    case common_wire::QUALITY_FLAG_DUPLICATE:
        return 0;
    case common_wire::QUALITY_FLAG_OUT_OF_ORDER:
        return 1;
    case common_wire::QUALITY_FLAG_SEQUENCE_GAP:
        return 2;
    case common_wire::QUALITY_FLAG_ORDERBOOK_RESYNC:
        return 3;
    case common_wire::QUALITY_FLAG_SNAPSHOT_BRIDGE_PENDING:
        return 4;
    case common_wire::QUALITY_FLAG_SNAPSHOT_TOO_OLD:
        return 5;
    case common_wire::QUALITY_FLAG_BOOTSTRAP_BUFFER_OVERFLOW:
        return 6;
    case common_wire::QUALITY_FLAG_RECOVERED_TAIL:
        return 7;
    case common_wire::QUALITY_FLAG_MALFORMED_PAYLOAD:
        return 8;
    case common_wire::QUALITY_FLAG_EXCHANGE_TIME_MISSING:
        return 9;
    case common_wire::QUALITY_FLAG_RECEIVE_CLOCK_DISCONTINUITY:
        return 10;
    case common_wire::QUALITY_FLAG_SLOW_CONSUMER_GAP:
        return 11;
    case common_wire::QUALITY_FLAG_PRODUCER_RESTART:
        return 12;
    case common_wire::QUALITY_FLAG_OVERLAP:
        return 13;
    case common_wire::QUALITY_FLAG_IDENTITY_CONFLICT:
        return 14;
    case common_wire::QUALITY_FLAG_CROSSED_BOOK:
        return 15;
    default:
        return 16;
    }
}

[[nodiscard]] AdapterResult<std::vector<common_wire::QualityFlag>>
make_quality_flags(const core::OrderBook& book, core::ProjectionStatus status,
                   const SnapshotOptions& options) {
    std::vector<common_wire::QualityFlag> flags;
    flags.reserve(options.host_quality_facts.size() + 2);
    for (const auto fact : options.host_quality_facts) {
        const auto mapped = map_host_quality(fact, status);
        if (const auto* failure = std::get_if<AdapterError>(&mapped)) {
            return *failure;
        }
        flags.push_back(std::get<common_wire::QualityFlag>(mapped));
    }
    if (status == core::ProjectionStatus::AwaitingBridge) {
        flags.push_back(common_wire::QUALITY_FLAG_SNAPSHOT_BRIDGE_PENDING);
    }
    if (status == core::ProjectionStatus::NeedsResync) {
        flags.push_back(common_wire::QUALITY_FLAG_SEQUENCE_GAP);
    }
    const auto best_bid = book.best_bid();
    const auto best_ask = book.best_ask();
    if (best_bid.has_value() && best_ask.has_value() && best_bid->price >= best_ask->price) {
        flags.push_back(common_wire::QUALITY_FLAG_CROSSED_BOOK);
    }
    std::sort(flags.begin(), flags.end(),
              [](auto lhs, auto rhs) { return quality_rank(lhs) < quality_rank(rhs); });
    flags.erase(std::unique(flags.begin(), flags.end()), flags.end());
    return flags;
}

[[nodiscard]] AdapterResult<std::string> format_price(core::BookLevel level,
                                                      core::NumericSpec spec) {
    const auto formatted = core::format_price_fixed(level.price, spec.price_scale);
    if (const auto* failure = std::get_if<core::DecimalError>(&formatted)) {
        const auto code = failure->code == core::DecimalErrorCode::Overflow
                              ? AdapterErrorCode::NumericOverflow
                              : AdapterErrorCode::ScaleMismatch;
        return decimal_error(code, AdapterField::BidPrice, *failure);
    }
    return std::get<std::string>(formatted);
}

[[nodiscard]] AdapterResult<std::string> format_quantity(core::BookLevel level,
                                                         core::NumericSpec spec) {
    const auto formatted = core::format_quantity_fixed(level.quantity, spec.quantity_scale);
    if (const auto* failure = std::get_if<core::DecimalError>(&formatted)) {
        const auto code = failure->code == core::DecimalErrorCode::Overflow
                              ? AdapterErrorCode::NumericOverflow
                              : AdapterErrorCode::ScaleMismatch;
        return decimal_error(code, AdapterField::BidQuantity, *failure);
    }
    return std::get<std::string>(formatted);
}

[[nodiscard]] std::optional<AdapterError>
append_levels(::google::protobuf::RepeatedPtrField<common_wire::PriceLevel>* output,
              const std::vector<core::BookLevel>& levels, core::NumericSpec spec) {
    for (const auto& level : levels) {
        auto price = format_price(level, spec);
        if (const auto* failure = std::get_if<AdapterError>(&price)) {
            return *failure;
        }
        auto quantity = format_quantity(level, spec);
        if (const auto* failure = std::get_if<AdapterError>(&quantity)) {
            return *failure;
        }
        auto wire_level = std::make_unique<common_wire::PriceLevel>();
        try {
            auto owned_price =
                std::make_unique<std::string>(std::get<std::string>(std::move(price)));
            wire_level->set_allocated_price(owned_price.get());
            [[maybe_unused]] auto* transferred_price = owned_price.release();
            auto owned_quantity =
                std::make_unique<std::string>(std::get<std::string>(std::move(quantity)));
            wire_level->set_allocated_quantity(owned_quantity.get());
            [[maybe_unused]] auto* transferred_quantity = owned_quantity.release();
            output->AddAllocated(wire_level.get());
            [[maybe_unused]] auto* transferred_level = wire_level.release();
        } catch (const std::bad_alloc&) {
            wire_level->set_allocated_price(nullptr);
            wire_level->set_allocated_quantity(nullptr);
            throw;
        }
    }
    return std::nullopt;
}

struct SnapshotBookSelection final {
    const core::OrderBook* book;
    bool synchronized;
};

struct SnapshotLevels final {
    std::vector<core::BookLevel> bids;
    std::vector<core::BookLevel> asks;
};

[[nodiscard]] AdapterResult<common_wire::SnapshotSource>
validate_snapshot_context(const core::BookProjection& projection, core::ProjectionStatus status,
                          const SnapshotContext& context) noexcept {
    if (!is_symbol(context.identity.symbol)) {
        return error(AdapterErrorCode::InvalidIdentifier, AdapterField::Symbol);
    }
    if (context.identity.policy != projection.policy()) {
        return error(AdapterErrorCode::ProjectionPolicyMismatch, AdapterField::ProjectionPolicy);
    }
    if (!is_non_empty_text(context.producer)) {
        return error(context.producer.empty() ? AdapterErrorCode::MissingRequiredField
                                              : AdapterErrorCode::InvalidIdentifier,
                     AdapterField::Producer);
    }
    if (!is_non_empty_text(context.producer_version)) {
        return error(context.producer_version.empty() ? AdapterErrorCode::MissingRequiredField
                                                      : AdapterErrorCode::InvalidIdentifier,
                     AdapterField::ProducerVersion);
    }
    const auto source = map_snapshot_origin(context.source);
    if (const auto* failure = std::get_if<AdapterError>(&source)) {
        return *failure;
    }
    if (status == core::ProjectionStatus::NeedsResync && !context.current_gap.has_value()) {
        return error(AdapterErrorCode::MissingRequiredField, AdapterField::CurrentGap);
    }
    if (status != core::ProjectionStatus::NeedsResync && context.current_gap.has_value()) {
        return error(AdapterErrorCode::InvalidGapContext, AdapterField::CurrentGap);
    }
    return *std::get_if<common_wire::SnapshotSource>(&source);
}

[[nodiscard]] AdapterResult<SnapshotBookSelection>
select_snapshot_book(const core::BookProjection& projection, core::ProjectionStatus status) {
    if (status == core::ProjectionStatus::Synchronized) {
        const auto synchronized_book = projection.synchronized_book();
        if (!synchronized_book.has_value()) {
            return error(AdapterErrorCode::UnsupportedProjectionState, AdapterField::None);
        }
        return SnapshotBookSelection{&synchronized_book->get(), true};
    }
    if (status == core::ProjectionStatus::AwaitingBridge ||
        status == core::ProjectionStatus::NeedsResync) {
        return SnapshotBookSelection{&projection.diagnostic_book(), false};
    }
    return error(AdapterErrorCode::UnsupportedProjectionState, AdapterField::None);
}

[[nodiscard]] SnapshotLevels select_snapshot_levels(const core::OrderBook& book,
                                                    const SnapshotOptions& options) {
    if (options.depth_limit.has_value()) {
        const auto limit = static_cast<std::size_t>(options.depth_limit->value());
        return {book.top_levels(core::BookSide::Bid, limit),
                book.top_levels(core::BookSide::Ask, limit)};
    }
    return {book.all_levels(core::BookSide::Bid), book.all_levels(core::BookSide::Ask)};
}

[[nodiscard]] std::optional<AdapterError>
append_current_gap(core::LocalOrderBookSnapshot& candidate, const core::BookProjection& projection,
                   const SnapshotContext& context, core::ProjectionStatus status) {
    if (status != core::ProjectionStatus::NeedsResync) {
        return std::nullopt;
    }
    const auto gap = projection.last_gap();
    if (!gap.has_value() || !context.current_gap.has_value()) {
        return error(AdapterErrorCode::InvalidGapContext, AdapterField::CurrentGap);
    }
    const auto recovery = map_gap_recovery(context.current_gap->recovery_state);
    if (const auto* failure = std::get_if<AdapterError>(&recovery)) {
        return *failure;
    }
    const auto reason = map_gap_reason(gap->reason);
    if (const auto* failure = std::get_if<AdapterError>(&reason)) {
        return *failure;
    }
    auto wire_gap = std::make_unique<core::GapDescriptor>();
    wire_gap->set_stream(common_wire::STREAM_DIFF_DEPTH);
    wire_gap->set_detected_at_utc_ns(context.current_gap->detected_at_utc_ns);
    wire_gap->set_previous_sequence(gap->last_accepted_final.value());
    wire_gap->set_next_sequence(gap->incoming_range.first().value());
    wire_gap->set_reason_code(*std::get_if<common_wire::ReasonCode>(&reason));
    wire_gap->set_recovery_state(*std::get_if<common_wire::ResyncState>(&recovery));
    candidate.set_allocated_last_gap(wire_gap.get());
    [[maybe_unused]] auto* transferred_gap = wire_gap.release();
    return std::nullopt;
}

} // namespace

namespace detail {

struct AdapterFactory final {
    [[nodiscard]] static AdaptedBookBaseline
    baseline(core::NumericSpec spec, core::SequencePolicyKind policy, core::UpdateId id,
             std::vector<core::BookLevel> bids, std::vector<core::BookLevel> asks,
             AdaptedMetadata metadata) {
        return {spec, policy, id, std::move(bids), std::move(asks), std::move(metadata)};
    }

    [[nodiscard]] static AdaptedDepthBatch
    batch(core::NumericSpec spec, core::SequencePolicyKind policy, core::UpdateRange range,
          std::optional<core::UpdateId> previous, std::vector<core::LevelUpdate> levels,
          AdaptedMetadata metadata) {
        return {spec, policy, range, previous, std::move(levels), std::move(metadata)};
    }
};

} // namespace detail

AdaptedBookBaseline::AdaptedBookBaseline(core::NumericSpec numeric_spec,
                                         core::SequencePolicyKind policy,
                                         core::UpdateId last_update_id,
                                         std::vector<core::BookLevel> bids,
                                         std::vector<core::BookLevel> asks,
                                         AdaptedMetadata metadata) noexcept
    : numeric_spec_{numeric_spec}, policy_{policy}, last_update_id_{last_update_id},
      bids_{std::move(bids)}, asks_{std::move(asks)}, metadata_{std::move(metadata)} {}

AdaptedBookBaseline::AdaptedBookBaseline(AdaptedBookBaseline&&) noexcept = default;
AdaptedBookBaseline& AdaptedBookBaseline::operator=(AdaptedBookBaseline&&) noexcept = default;

core::BookBaseline AdaptedBookBaseline::view_unchecked() const& noexcept {
    return {last_update_id_, bids_, asks_};
}

AdapterResult<core::InstallResult>
AdaptedBookBaseline::install_into(core::BookProjection& target) const& {
    if (const auto binding_error = validate_binding(numeric_spec_, policy_, target)) {
        return *binding_error;
    }
    return target.install_baseline(view_unchecked());
}

const AdaptedMetadata& AdaptedBookBaseline::metadata() const& noexcept { return metadata_; }

AdaptedDepthBatch::AdaptedDepthBatch(core::NumericSpec numeric_spec,
                                     core::SequencePolicyKind policy, core::UpdateRange range,
                                     std::optional<core::UpdateId> previous_final,
                                     std::vector<core::LevelUpdate> levels,
                                     AdaptedMetadata metadata) noexcept
    : numeric_spec_{numeric_spec}, policy_{policy}, range_{range}, previous_final_{previous_final},
      levels_{std::move(levels)}, metadata_{std::move(metadata)} {}

AdaptedDepthBatch::AdaptedDepthBatch(AdaptedDepthBatch&&) noexcept = default;
AdaptedDepthBatch& AdaptedDepthBatch::operator=(AdaptedDepthBatch&&) noexcept = default;

core::DepthBatch AdaptedDepthBatch::view_unchecked() const& noexcept {
    return {range_, previous_final_, levels_};
}

AdapterResult<core::ApplyResult> AdaptedDepthBatch::apply_to(core::BookProjection& target) const& {
    if (const auto binding_error = validate_binding(numeric_spec_, policy_, target)) {
        return *binding_error;
    }
    return target.apply(view_unchecked());
}

const AdaptedMetadata& AdaptedDepthBatch::metadata() const& noexcept { return metadata_; }

AdapterResult<DepthLimit> DepthLimit::create(std::int64_t value) noexcept {
    if (value <= 0 || value > std::numeric_limits<std::int32_t>::max()) {
        return error(AdapterErrorCode::InvalidDepthLimit, AdapterField::DepthLimit);
    }
    return DepthLimit{static_cast<std::int32_t>(value)};
}

std::int32_t DepthLimit::value() const noexcept { return value_; }

AdapterResult<AdaptedBookBaseline>
adapt_exchange_depth_snapshot(const ::binance_market_data::market::v1::ExchangeDepthSnapshot& wire,
                              core::NumericSpec numeric_spec, const ExpectedIdentity& expected) {
    const auto identity =
        validate_wire_identity(wire.venue(), wire.market(), wire.symbol(), expected);
    if (const auto* failure = std::get_if<AdapterError>(&identity)) {
        return *failure;
    }
    const auto policy = std::get<core::SequencePolicyKind>(identity);
    if (wire.schema_version() != kSnapshotSchema) {
        return error(AdapterErrorCode::UnsupportedSchemaVersion, AdapterField::SchemaVersion);
    }
    if (const auto text_error = validate_required_text(
            wire.producer(), wire.producer_version(), wire.request_id(), AdapterField::RequestId)) {
        return *text_error;
    }
    const auto quality = adapt_quality(wire.quality_flags());
    if (const auto* failure = std::get_if<AdapterError>(&quality)) {
        return *failure;
    }
    const auto bids = adapt_baseline_side(wire.bids(), numeric_spec, true);
    if (const auto* failure = std::get_if<AdapterError>(&bids)) {
        return *failure;
    }
    const auto asks = adapt_baseline_side(wire.asks(), numeric_spec, false);
    if (const auto* failure = std::get_if<AdapterError>(&asks)) {
        return *failure;
    }
    return detail::AdapterFactory::baseline(
        numeric_spec, policy, core::UpdateId{wire.last_update_id()},
        std::get<std::vector<core::BookLevel>>(bids), std::get<std::vector<core::BookLevel>>(asks),
        std::get<AdaptedMetadata>(quality));
}

AdapterResult<AdaptedDepthBatch>
adapt_depth_update(const ::binance_market_data::market::v1::DepthUpdate& wire,
                   core::NumericSpec numeric_spec, const ExpectedIdentity& expected) {
    if (!wire.has_metadata()) {
        return error(AdapterErrorCode::MissingRequiredField, AdapterField::None);
    }
    const auto& metadata = wire.metadata();
    const auto identity =
        validate_wire_identity(metadata.venue(), metadata.market(), metadata.symbol(), expected);
    if (const auto* failure = std::get_if<AdapterError>(&identity)) {
        return *failure;
    }
    const auto policy = std::get<core::SequencePolicyKind>(identity);
    if (const auto stream_error = validate_stream(metadata.stream())) {
        return *stream_error;
    }
    if (metadata.schema_version() != kUpdateSchema) {
        return error(AdapterErrorCode::UnsupportedSchemaVersion, AdapterField::SchemaVersion);
    }
    if (const auto text_error =
            validate_required_text(metadata.producer(), metadata.producer_version(),
                                   metadata.connection_id(), AdapterField::ConnectionId)) {
        return *text_error;
    }
    const auto range = core::UpdateRange::try_create(core::UpdateId{wire.first_update_id()},
                                                     core::UpdateId{wire.final_update_id()});
    if (!range.has_value()) {
        return error(AdapterErrorCode::InvalidUpdateRange, AdapterField::FinalUpdateId);
    }
    const auto quality = adapt_quality(metadata.quality_flags());
    if (const auto* failure = std::get_if<AdapterError>(&quality)) {
        return *failure;
    }
    const auto bids = adapt_updates(wire.bids(), numeric_spec, core::BookSide::Bid);
    if (const auto* failure = std::get_if<AdapterError>(&bids)) {
        return *failure;
    }
    const auto asks = adapt_updates(wire.asks(), numeric_spec, core::BookSide::Ask);
    if (const auto* failure = std::get_if<AdapterError>(&asks)) {
        return *failure;
    }
    auto levels = std::get<std::vector<core::LevelUpdate>>(bids);
    auto ask_levels = std::get<std::vector<core::LevelUpdate>>(asks);
    levels.reserve(levels.size() + ask_levels.size());
    levels.insert(levels.end(), ask_levels.begin(), ask_levels.end());
    std::optional<core::UpdateId> previous_final;
    if (wire.has_previous_final_update_id()) {
        previous_final = core::UpdateId{wire.previous_final_update_id()};
    }
    return detail::AdapterFactory::batch(numeric_spec, policy, *range, previous_final,
                                         std::move(levels), std::get<AdaptedMetadata>(quality));
}

AdapterResult<::binance_market_data::projection::v1::LocalOrderBookSnapshot>
make_local_order_book_snapshot(const core::BookProjection& projection,
                               const SnapshotContext& context, const SnapshotOptions& options) {
    const auto status = projection.status();
    if (status == core::ProjectionStatus::AwaitingBaseline) {
        return error(AdapterErrorCode::MissingLastUpdateId, AdapterField::LastUpdateId);
    }
    const auto last_update_id = projection.last_update_id();
    if (!last_update_id.has_value()) {
        return error(AdapterErrorCode::MissingLastUpdateId, AdapterField::LastUpdateId);
    }
    const auto source = validate_snapshot_context(projection, status, context);
    if (const auto* failure = std::get_if<AdapterError>(&source)) {
        return *failure;
    }
    const auto selection_result = select_snapshot_book(projection, status);
    if (const auto* failure = std::get_if<AdapterError>(&selection_result)) {
        return *failure;
    }
    const auto selection = *std::get_if<SnapshotBookSelection>(&selection_result);

    const auto quality = make_quality_flags(*selection.book, status, options);
    if (const auto* failure = std::get_if<AdapterError>(&quality)) {
        return *failure;
    }

    const auto numeric_spec = projection.numeric_spec();
    const auto levels = select_snapshot_levels(*selection.book, options);

    ::binance_market_data::projection::v1::LocalOrderBookSnapshot candidate;
    try {
        candidate.set_venue(common_wire::VENUE_BINANCE);
        candidate.set_market(market_for_policy(projection.policy()));
        auto symbol = std::make_unique<std::string>(context.identity.symbol);
        candidate.set_allocated_symbol(symbol.get());
        [[maybe_unused]] auto* transferred_symbol = symbol.release();
        auto schema_version = std::make_unique<std::string>(kLocalSnapshotSchema);
        candidate.set_allocated_schema_version(schema_version.get());
        [[maybe_unused]] auto* transferred_schema_version = schema_version.release();
        auto producer = std::make_unique<std::string>(context.producer);
        candidate.set_allocated_producer(producer.get());
        [[maybe_unused]] auto* transferred_producer = producer.release();
        auto producer_version = std::make_unique<std::string>(context.producer_version);
        candidate.set_allocated_producer_version(producer_version.get());
        [[maybe_unused]] auto* transferred_producer_version = producer_version.release();
        candidate.set_source(*std::get_if<common_wire::SnapshotSource>(&source));
        candidate.set_last_update_id(last_update_id->value());
        if (const auto append_error =
                append_levels(candidate.mutable_bids(), levels.bids, numeric_spec)) {
            return *append_error;
        }
        if (const auto append_error =
                append_levels(candidate.mutable_asks(), levels.asks, numeric_spec)) {
            return *append_error;
        }
        if (options.depth_limit.has_value()) {
            candidate.set_depth_limit(options.depth_limit->value());
        }
        candidate.set_generated_time_utc_ns(context.generated_time_utc_ns);
        if (context.generated_monotonic_ns.has_value()) {
            candidate.set_generated_monotonic_ns(*context.generated_monotonic_ns);
        }
        candidate.set_synchronized(selection.synchronized);

        if (const auto gap_error = append_current_gap(candidate, projection, context, status)) {
            return *gap_error;
        }

        for (const auto flag : std::get<std::vector<common_wire::QualityFlag>>(quality)) {
            candidate.add_quality_flags(flag);
        }
        return candidate;
    } catch (const std::bad_alloc&) {
        // Protobuf sets required-field presence bits before allocating string storage. Clear the
        // local candidate before stack unwinding so an injected allocation failure cannot leave a
        // partially initialized generated message for its debug destructor to inspect.
        candidate.set_allocated_symbol(nullptr);
        candidate.set_allocated_schema_version(nullptr);
        candidate.set_allocated_producer(nullptr);
        candidate.set_allocated_producer_version(nullptr);
        candidate.set_allocated_last_gap(nullptr);
        candidate.Clear();
        throw;
    }
}

} // namespace binance_market_data::projection_adapter::v1

#include "replay_fuzz_decoder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bmd_projection::m5::replay::fuzz_decoder {
namespace {

inline constexpr std::uint32_t kMaxValidScale = 18;

[[nodiscard]] std::uint32_t clamp_scale(std::uint32_t raw) noexcept {
    return raw > kMaxValidScale ? raw % (kMaxValidScale + 1U) : raw;
}

[[nodiscard]] std::uint64_t read_fixed_u64(ByteCursor& cursor, unsigned width) noexcept {
    if (width == 0) {
        return 0;
    }
    std::uint64_t value = 0;
    for (unsigned i = 0; i < width && !cursor.exhausted(); ++i) {
        value = (value << 8) | cursor.read_u8();
    }
    return value;
}

[[nodiscard]] SourceLocation make_source(std::size_t event_index, const std::string& desc) {
    return {event_index, 0, desc};
}

[[nodiscard]] std::string decode_decimal_form_token(ByteCursor& cursor,
                                                    std::size_t max_len) noexcept {
    const std::uint8_t b = cursor.read_u8();
    const std::size_t int_len = (b & 0x0FU) % 5U;
    const bool has_frac = (b & 0x10U) != 0;
    const std::size_t frac_len = has_frac ? ((b >> 5U) & 0x03U) : 0U;
    std::string token;
    for (std::size_t i = 0; i < int_len && token.size() < max_len && !cursor.exhausted(); ++i) {
        token.push_back(static_cast<char>('0' + (cursor.read_u8() % 10U)));
    }
    if (has_frac && token.size() < max_len) {
        token.push_back('.');
        for (std::size_t i = 0; i < frac_len && token.size() < max_len && !cursor.exhausted();
             ++i) {
            token.push_back(static_cast<char>('0' + (cursor.read_u8() % 10U)));
        }
    }
    return token.empty() ? "0" : token;
}

// Decodes price/quantity level input strings that provide meaningful M1 fuzz coverage.
// Makes reachable: empty, "0", integer, decimal point, fractional, leading sign,
// large magnitude, leading zeros, invalid chars, etc.
[[nodiscard]] std::string decode_level_token(ByteCursor& cursor, std::size_t& token_idx) noexcept {
    if (cursor.exhausted()) {
        return "0";
    }
    const std::uint8_t style = cursor.read_u8();
    const unsigned mode = style & 0x7U;
    const std::size_t max_len = std::min<std::size_t>(kMaxLevelTokenBytes, 31U);

    if (mode == 0 && !cursor.exhausted()) {
        return decode_decimal_form_token(cursor, max_len);
    }

    if (mode == 1) {
        if (cursor.exhausted()) {
            return "1";
        }
        const std::uint8_t b = cursor.read_u8();
        const auto val = static_cast<std::uint64_t>(b);
        return std::to_string(val);
    }

    switch (mode) {
    case 2:
        return "";
    case 3:
        return "0";
    case 4: {
        const std::uint8_t b = cursor.exhausted() ? 0 : cursor.read_u8();
        if (b == 0) {
            return "00";
        }
        return "0";
    }
    case 5: {
        const char sign = (token_idx & 1U) != 0 ? '-' : '+';
        const auto val = std::to_string(
            cursor.exhausted() ? 0U : static_cast<std::uint64_t>(cursor.read_u8()) % 100U);
        return sign + val;
    }
    case 6:
        return "12a34";
    case 7:
        ++token_idx;
        return "9999999999999999999";
    default:
        return "0";
    }
}

[[nodiscard]] std::vector<LevelInput> decode_levels(ByteCursor& cursor, std::size_t max_count,
                                                    std::size_t& token_idx) noexcept {
    if (cursor.exhausted()) {
        return {};
    }

    const std::uint32_t count = cursor.read_bounded(static_cast<std::uint32_t>(max_count));
    std::vector<LevelInput> levels;
    levels.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const bool bid = (cursor.read_u8() & 1U) == 0;
        const auto side = bid ? Side::Bid : Side::Ask;
        ++token_idx;
        std::string price = decode_level_token(cursor, token_idx);
        ++token_idx;
        std::string quantity = decode_level_token(cursor, token_idx);
        levels.push_back({side, std::move(price), std::move(quantity)});
    }
    return levels;
}

// Decodes HostQualityFact from a byte — maps to the 13 valid enum values.
[[nodiscard]] HostQualityFact decode_quality_fact(ByteCursor& cursor) noexcept {
    static constexpr std::array<const HostQualityFact, 13> kMap{
        HostQualityFact::Duplicate,
        HostQualityFact::OutOfOrder,
        HostQualityFact::OrderBookResync,
        HostQualityFact::SnapshotTooOld,
        HostQualityFact::BootstrapBufferOverflow,
        HostQualityFact::RecoveredTail,
        HostQualityFact::MalformedPayload,
        HostQualityFact::ExchangeTimeMissing,
        HostQualityFact::ReceiveClockDiscontinuity,
        HostQualityFact::SlowConsumerGap,
        HostQualityFact::ProducerRestart,
        HostQualityFact::Overlap,
        HostQualityFact::IdentityConflict,
    };
    const std::uint32_t idx = cursor.read_bounded(12);
    return kMap.at(idx);
}

[[nodiscard]] std::vector<HostQualityFact>
decode_quality_facts(ByteCursor& cursor, std::size_t /*max_count*/) noexcept {
    if (cursor.exhausted()) {
        return {};
    }
    const std::uint32_t count = cursor.read_bounded(8U);
    std::vector<HostQualityFact> facts;
    facts.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        facts.push_back(decode_quality_fact(cursor));
    }
    return facts;
}

// Decodes a single operation from the cursor. event_index is used for SourceLocation.
[[nodiscard]] std::optional<Operation> decode_operation(ByteCursor& cursor,
                                                        std::size_t event_index) noexcept {
    if (cursor.exhausted()) {
        return std::nullopt;
    }
    const std::uint8_t type_raw = cursor.read_u8();
    const std::uint8_t op_type = type_raw % 7U;
    std::size_t token_idx = 0;

    switch (op_type) {
    case 0: {
        // InstallBaseline
        const auto last_update_id = cursor.read_var_u64();
        // Bid/ask counts are read by decode_levels; pass kMaxLevelsPerEvent as cap only.
        std::string desc = "structured-fuzz[op=";
        desc += std::to_string(event_index);
        desc += " InstallBaseline]";
        auto bids = decode_levels(cursor, kMaxLevelsPerEvent, token_idx);
        auto asks = decode_levels(cursor, kMaxLevelsPerEvent, token_idx);
        return InstallBaselineOp{make_source(event_index, desc), last_update_id, std::move(bids),
                                 std::move(asks)};
    }
    case 1: {
        // DepthUpdate — normal update range must satisfy first <= final.
        // When the decoded range is inverted, emit MalformedRangeOp instead.
        const auto first_id = cursor.read_var_u64();
        const auto final_id = cursor.read_var_u64();
        if (first_id > final_id) {
            std::string desc = "structured-fuzz[op=";
            desc += std::to_string(event_index);
            desc += " MalformedRange]";
            return MalformedRangeOp{make_source(event_index, desc), first_id, final_id};
        }
        const bool has_previous = (cursor.read_u8() & 1U) != 0;
        const auto previous = has_previous ? std::optional{cursor.read_var_u64()} : std::nullopt;
        std::string desc = "structured-fuzz[op=";
        desc += std::to_string(event_index);
        desc += " DepthUpdate]";
        auto levels = decode_levels(cursor, kMaxLevelsPerEvent, token_idx);
        return DepthUpdateOp{make_source(event_index, desc), first_id, final_id, previous,
                             std::move(levels)};
    }
    case 2: {
        // Rebaseline
        const auto last_update_id = cursor.read_var_u64();
        std::string desc = "structured-fuzz[op=";
        desc += std::to_string(event_index);
        desc += " Rebaseline]";
        auto bids = decode_levels(cursor, kMaxLevelsPerEvent, token_idx);
        auto asks = decode_levels(cursor, kMaxLevelsPerEvent, token_idx);
        return RebaselineOp{make_source(event_index, desc), last_update_id, std::move(bids),
                            std::move(asks)};
    }
    case 3: {
        // Reset
        std::string desc = "structured-fuzz[op=";
        desc += std::to_string(event_index);
        desc += " Reset]";
        return ResetOp{make_source(event_index, desc)};
    }
    case 4: {
        // SnapshotRequest
        const bool has_depth = (cursor.read_u8() & 1U) != 0;
        const auto depth_limit =
            has_depth ? std::optional(static_cast<std::uint32_t>(cursor.read_u8())) : std::nullopt;
        const auto host_quality = decode_quality_facts(cursor, 6);
        const auto snapshot_id = cursor.read_string(kMaxSnapshotStringBytes);
        const auto producer = cursor.read_string(kMaxSnapshotStringBytes);
        const auto producer_version = cursor.read_string(kMaxSnapshotStringBytes);
        const std::uint8_t origin_byte = cursor.read_u8();
        SnapshotOrigin source_origin;
        switch (origin_byte % 3U) {
        case 0:
            source_origin = SnapshotOrigin::GatewayLive;
            break;
        case 1:
            source_origin = SnapshotOrigin::RecorderReplay;
            break;
        default:
            source_origin = SnapshotOrigin::HistoryReplay;
            break;
        }
        const auto generated_time = cursor.read_var_u64();
        const bool has_monotonic = (cursor.read_u8() & 1U) != 0;
        const auto generated_monotonic =
            has_monotonic ? std::optional(cursor.read_var_u64()) : std::nullopt;
        const bool has_gap = (cursor.read_u8() & 1U) != 0;
        std::optional<std::pair<std::uint64_t, GapRecoveryState>> current_gap;
        if (has_gap) {
            const auto gap_seq = cursor.read_var_u64();
            GapRecoveryState state;
            switch (cursor.read_u8() % 5U) {
            case 0:
                state = GapRecoveryState::Synchronized;
                break;
            case 1:
                state = GapRecoveryState::ResyncRequired;
                break;
            case 2:
                state = GapRecoveryState::ResyncInProgress;
                break;
            case 3:
                state = GapRecoveryState::Recovered;
                break;
            default:
                state = GapRecoveryState::ResyncFailed;
                break;
            }
            current_gap = std::pair{gap_seq, state};
        }
        std::string desc = "structured-fuzz[op=";
        desc += std::to_string(event_index);
        desc += " SnapshotRequest]";
        return SnapshotRequestOp{make_source(event_index, desc),
                                 depth_limit,
                                 std::move(host_quality),
                                 std::move(snapshot_id),
                                 std::move(producer),
                                 std::move(producer_version),
                                 source_origin,
                                 generated_time,
                                 generated_monotonic,
                                 std::move(current_gap)};
    }
    case 5: {
        // AdapterMetadata — observed quality from the input side
        std::string desc = "structured-fuzz[op=";
        desc += std::to_string(event_index);
        desc += " AdapterMetadata]";
        return AdapterMetadataOp{make_source(event_index, desc), decode_quality_facts(cursor, 8)};
    }
    case 6: {
        // MalformedRange — must denote a reversed range (first > final).
        // A structurally valid range belongs to DepthUpdateOp instead.
        const auto first_id = cursor.read_var_u64();
        const auto final_id = cursor.read_var_u64();
        if (first_id <= final_id) {
            const bool has_previous = (cursor.read_u8() & 1U) != 0;
            const auto previous =
                has_previous ? std::optional{cursor.read_var_u64()} : std::nullopt;
            std::string desc = "structured-fuzz[op=";
            desc += std::to_string(event_index);
            desc += " DepthUpdate]";
            auto levels = decode_levels(cursor, kMaxLevelsPerEvent, token_idx);
            return DepthUpdateOp{make_source(event_index, desc), first_id, final_id, previous,
                                 std::move(levels)};
        }
        std::string desc = "structured-fuzz[op=";
        desc += std::to_string(event_index);
        desc += " MalformedRange]";
        return MalformedRangeOp{make_source(event_index, desc), first_id, final_id};
    }
    default:
        return std::nullopt;
    }
}

[[nodiscard]] std::vector<Operation> decode_operations(ByteCursor& cursor) noexcept {
    const std::uint32_t requested = cursor.exhausted() ? 0U : cursor.read_u8();
    const std::size_t max_ops = static_cast<std::size_t>(requested > 0 ? requested : 1);
    const std::size_t cap = std::min(max_ops, kMaxOperations);
    std::vector<Operation> ops;
    ops.reserve(cap);
    for (std::size_t i = 0; i < cap; ++i) {
        auto op = decode_operation(cursor, i);
        if (!op.has_value()) {
            break;
        }
        ops.push_back(std::move(*op));
    }
    return ops;
}

} // namespace

std::uint64_t ByteCursor::read_var_u64() noexcept {
    if (exhausted()) {
        return 0;
    }
    const std::uint8_t mode = read_u8();
    const unsigned width = (mode & 0x07U) + 1U;
    const bool special = (mode & 0x08U) != 0;

    if (special) {
        switch ((mode >> 4U) & 0x07U) {
        case 0:
            return 0;
        case 1:
            return 1;
        case 2:
            return 2;
        case 3:
            return UINT64_MAX;
        case 4:
            return UINT64_MAX - 1;
        case 5:
            return UINT64_MAX - 2;
        default:
            return 0;
        }
    }

    return read_fixed_u64(*this, width);
}

std::string ByteCursor::read_string(std::size_t max_len) noexcept {
    if (exhausted()) {
        return {};
    }
    const std::uint8_t raw_len = read_u8();
    const std::size_t len = std::min<std::size_t>(static_cast<std::size_t>(raw_len), max_len);
    std::string result;
    result.reserve(len + 1);
    for (std::size_t i = 0; i < len && !exhausted(); ++i) {
        const std::uint8_t b = read_u8();
        if (b != 0) {
            result.push_back(static_cast<char>(b));
        }
    }
    return result;
}

std::uint32_t ByteCursor::read_bounded(std::uint32_t max_val) noexcept {
    if (exhausted() || max_val == 0) {
        return 0;
    }
    const std::uint8_t v = read_u8();
    return static_cast<std::uint32_t>(v) % (max_val + 1U);
}

std::optional<FuzzCase> decode(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < 1) {
        return std::nullopt;
    }

    ByteCursor cursor(data, size);

    const std::uint8_t header = cursor.read_u8();
    const bool adapter_mode = (header & 0x01U) != 0;
    const bool usdm_market = (header & 0x02U) != 0;
    const std::uint32_t price_scale_raw = (header >> 2U) & 0x3FU;
    const std::uint32_t price_scale = clamp_scale(price_scale_raw);

    const std::uint8_t qs_raw = cursor.read_u8();
    const std::uint32_t quantity_scale = clamp_scale(static_cast<std::uint32_t>(qs_raw));

    const Market market = usdm_market ? Market::UsdMPerpetual : Market::Spot;
    const SequencePolicy policy =
        usdm_market ? SequencePolicy::UsdMPerpetual : SequencePolicy::Spot;
    const NumericSpec numeric_spec{price_scale, quantity_scale};

    std::string symbol = cursor.read_string(kMaxSymbolBytes);
    if (symbol.empty()) {
        symbol = usdm_market ? "BTCUSDT" : "BTCUSDT";
    }

    auto operations = decode_operations(cursor);
    if (operations.empty()) {
        return std::nullopt;
    }

    FuzzCase result;
    result.mode = adapter_mode ? DecodedMode::AdapterEnabled : DecodedMode::CoreOnly;
    result.market = market;
    result.numeric_spec = numeric_spec;
    result.sequence_policy = policy;
    result.symbol = std::move(symbol);
    result.operations = std::move(operations);

    // Adapter scenario dimensions
    if (adapter_mode && !cursor.exhausted()) {
        const std::uint8_t sbyte = cursor.read_u8();
        switch (sbyte & 0x03U) {
        case 0:
            result.scenario_venue = FuzzVenue::Binance;
            break;
        case 1:
            result.scenario_venue = FuzzVenue::Unspecified;
            break;
        default:
            result.scenario_venue = FuzzVenue::UnknownNumeric;
            break;
        }
        switch ((sbyte >> 2U) & 0x03U) {
        case 0:
            result.scenario_market = FuzzMarket::Spot;
            break;
        case 1:
            result.scenario_market = FuzzMarket::UsdMPerpetual;
            break;
        case 2:
            result.scenario_market = FuzzMarket::Unspecified;
            break;
        default:
            result.scenario_market = FuzzMarket::UnknownNumeric;
            break;
        }
        result.adapter_wire_symbol = cursor.read_string(kMaxSymbolBytes);
        result.adapter_expected_symbol = cursor.read_string(kMaxSymbolBytes);

        const std::uint8_t ps = cursor.read_u8();
        const std::uint8_t qs = cursor.read_u8();
        const std::uint8_t pps = cursor.read_u8();
        const std::uint8_t pqs = cursor.read_u8();
        result.adapter_conversion_numeric_spec = NumericSpec{clamp_scale(ps), clamp_scale(qs)};
        result.adapter_projection_numeric_spec = NumericSpec{clamp_scale(pps), clamp_scale(pqs)};

        const std::uint8_t epbyte = cursor.read_u8();
        result.adapter_expected_policy =
            (epbyte & 1U) != 0 ? SequencePolicy::UsdMPerpetual : SequencePolicy::Spot;
        result.adapter_projection_policy =
            (epbyte & 2U) != 0 ? SequencePolicy::UsdMPerpetual : SequencePolicy::Spot;
    }

    return result;
}

} // namespace bmd_projection::m5::replay::fuzz_decoder

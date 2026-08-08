#include "replay_parser.hpp"

#include "canonical_text.hpp"

#include <array>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace bmd_projection::m5::replay {
namespace {

using Tokens = std::vector<std::string_view>;

[[nodiscard]] ParseError error(ErrorCategory category, std::size_t line, std::size_t event,
                               std::size_t token, std::string message) {
    return {category, line, event, token, std::move(message)};
}

[[nodiscard]] Tokens split_spaces(std::string_view line) {
    Tokens tokens;
    std::size_t start = 0;
    while (start < line.size()) {
        const auto end = line.find(' ', start);
        tokens.push_back(
            line.substr(start, end == std::string_view::npos ? line.size() - start : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return tokens;
}

[[nodiscard]] bool token_char(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '.' || value == '_' || value == ':' ||
           value == '/' || value == '+' || value == '-';
}

[[nodiscard]] bool valid_symbolic_token(std::string_view token) {
    if (token.empty()) {
        return false;
    }
    for (const auto character : token) {
        if (!token_char(character)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_identifier_token(std::string_view token) {
    return token != "-" && valid_symbolic_token(token);
}

[[nodiscard]] bool valid_decimal_lexeme(std::string_view token) {
    if (token.empty() || token == "-") {
        return false;
    }
    for (const auto character : token) {
        if (!((character >= '0' && character <= '9') || character == '+' || character == '-' ||
              character == '.' || character == 'e' || character == 'E')) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] Result<Market> parse_market(std::string_view token, std::size_t line,
                                          std::size_t event, std::size_t field) {
    if (token == "Spot") {
        return Market::Spot;
    }
    if (token == "UsdMPerpetual") {
        return Market::UsdMPerpetual;
    }
    return error(ErrorCategory::InvalidMetadata, line, event, field, "unknown market");
}

[[nodiscard]] Result<SequencePolicy> parse_policy(std::string_view token, std::size_t line,
                                                  std::size_t event, std::size_t field) {
    if (token == "Spot") {
        return SequencePolicy::Spot;
    }
    if (token == "UsdMPerpetual") {
        return SequencePolicy::UsdMPerpetual;
    }
    return error(ErrorCategory::InvalidMetadata, line, event, field, "unknown sequence policy");
}

[[nodiscard]] Result<HostQualityFact> parse_quality(std::string_view token, std::size_t line,
                                                    std::size_t event, std::size_t field) {
    static const std::map<std::string_view, HostQualityFact> values = {
        {"Duplicate", HostQualityFact::Duplicate},
        {"OutOfOrder", HostQualityFact::OutOfOrder},
        {"OrderBookResync", HostQualityFact::OrderBookResync},
        {"SnapshotTooOld", HostQualityFact::SnapshotTooOld},
        {"BootstrapBufferOverflow", HostQualityFact::BootstrapBufferOverflow},
        {"RecoveredTail", HostQualityFact::RecoveredTail},
        {"MalformedPayload", HostQualityFact::MalformedPayload},
        {"ExchangeTimeMissing", HostQualityFact::ExchangeTimeMissing},
        {"ReceiveClockDiscontinuity", HostQualityFact::ReceiveClockDiscontinuity},
        {"SlowConsumerGap", HostQualityFact::SlowConsumerGap},
        {"ProducerRestart", HostQualityFact::ProducerRestart},
        {"Overlap", HostQualityFact::Overlap},
        {"IdentityConflict", HostQualityFact::IdentityConflict},
    };
    const auto found = values.find(token);
    if (found == values.end()) {
        return error(ErrorCategory::ReplaySyntax, line, event, field, "unknown quality fact");
    }
    return found->second;
}

template <typename T>
[[nodiscard]] Result<std::vector<T>>
parse_comma_values(std::string_view token, std::size_t line, std::size_t event, std::size_t field,
                   Result<T> (*parse)(std::string_view, std::size_t, std::size_t, std::size_t)) {
    if (token == "-") {
        return std::vector<T>{};
    }
    if (token.empty() || token.front() == ',' || token.back() == ',' ||
        token.find(",,") != std::string_view::npos) {
        return error(ErrorCategory::ReplaySyntax, line, event, field, "invalid comma list");
    }
    std::vector<T> values;
    std::size_t start = 0;
    std::size_t item = 0;
    while (start < token.size()) {
        const auto end = token.find(',', start);
        const auto part =
            token.substr(start, end == std::string_view::npos ? token.size() - start : end - start);
        const auto parsed = parse(part, line, event, field + item);
        if (std::holds_alternative<ParseError>(parsed)) {
            return std::get<ParseError>(parsed);
        }
        values.push_back(std::get<T>(parsed));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
        ++item;
    }
    return values;
}

[[nodiscard]] Result<std::vector<HostQualityFact>>
parse_quality_list(std::string_view token, std::size_t line, std::size_t event, std::size_t field) {
    return parse_comma_values<HostQualityFact>(token, line, event, field, parse_quality);
}

[[nodiscard]] Result<std::vector<LevelInput>> parse_levels(std::string_view token,
                                                           std::optional<Side> expected_side,
                                                           std::size_t line, std::size_t event,
                                                           std::size_t field) {
    if (token == "-") {
        return std::vector<LevelInput>{};
    }
    if (token.empty() || token.front() == '|' || token.back() == '|' ||
        token.find("||") != std::string_view::npos) {
        return error(ErrorCategory::ReplaySyntax, line, event, field, "invalid level list");
    }
    std::vector<LevelInput> levels;
    std::size_t start = 0;
    std::size_t item = 0;
    while (start < token.size()) {
        const auto end = token.find('|', start);
        const auto entry =
            token.substr(start, end == std::string_view::npos ? token.size() - start : end - start);
        if (entry.size() < 5 || (entry.front() != 'B' && entry.front() != 'A') || entry[1] != ':') {
            return error(ErrorCategory::ReplaySyntax, line, event, field + item,
                         "level must be Side:price,quantity");
        }
        const Side side = entry.front() == 'B' ? Side::Bid : Side::Ask;
        if (expected_side.has_value() && side != expected_side.value()) {
            return error(ErrorCategory::ReplaySyntax, line, event, field + item,
                         "level side does not match its field");
        }
        const auto comma = entry.find(',', 2);
        if (comma == std::string_view::npos || comma == 2 || comma + 1 >= entry.size() ||
            entry.find(',', comma + 1) != std::string_view::npos) {
            return error(ErrorCategory::ReplaySyntax, line, event, field + item,
                         "level must contain exactly price,quantity");
        }
        const auto price = entry.substr(2, comma - 2);
        const auto quantity = entry.substr(comma + 1);
        if (!valid_decimal_lexeme(price) || !valid_decimal_lexeme(quantity)) {
            return error(ErrorCategory::ReplaySyntax, line, event, field + item,
                         "invalid decimal token in level");
        }
        levels.push_back({side, std::string(price), std::string(quantity)});
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
        ++item;
    }
    return levels;
}

[[nodiscard]] Result<std::optional<std::uint64_t>> parse_optional_uint(std::string_view token,
                                                                       std::size_t line,
                                                                       std::size_t event,
                                                                       std::size_t field) {
    if (token == "-") {
        return std::optional<std::uint64_t>{};
    }
    const auto value = parse_uint64(token, line, event, field);
    if (std::holds_alternative<ParseError>(value)) {
        return std::get<ParseError>(value);
    }
    return std::optional<std::uint64_t>{std::get<std::uint64_t>(value)};
}

[[nodiscard]] Result<SnapshotOrigin> parse_origin(std::string_view token, std::size_t line,
                                                  std::size_t event, std::size_t field) {
    if (token == "GatewayLive") {
        return SnapshotOrigin::GatewayLive;
    }
    if (token == "RecorderReplay") {
        return SnapshotOrigin::RecorderReplay;
    }
    if (token == "HistoryReplay") {
        return SnapshotOrigin::HistoryReplay;
    }
    return error(ErrorCategory::ReplaySyntax, line, event, field, "unknown snapshot origin");
}

[[nodiscard]] Result<GapRecoveryState> parse_recovery(std::string_view token, std::size_t line,
                                                      std::size_t event, std::size_t field) {
    if (token == "Synchronized") {
        return GapRecoveryState::Synchronized;
    }
    if (token == "ResyncRequired") {
        return GapRecoveryState::ResyncRequired;
    }
    if (token == "ResyncInProgress") {
        return GapRecoveryState::ResyncInProgress;
    }
    if (token == "Recovered") {
        return GapRecoveryState::Recovered;
    }
    if (token == "ResyncFailed") {
        return GapRecoveryState::ResyncFailed;
    }
    return error(ErrorCategory::ReplaySyntax, line, event, field, "unknown gap recovery state");
}

[[nodiscard]] Result<std::optional<std::pair<std::uint64_t, GapRecoveryState>>>
parse_current_gap(std::string_view token, std::size_t line, std::size_t event, std::size_t field) {
    if (token == "-") {
        return std::optional<std::pair<std::uint64_t, GapRecoveryState>>{};
    }
    const auto comma = token.find(',');
    if (comma == std::string_view::npos || token.find(',', comma + 1) != std::string_view::npos) {
        return error(ErrorCategory::ReplaySyntax, line, event, field,
                     "current gap must be detected_time,recovery_state");
    }
    const auto detected = parse_uint64(token.substr(0, comma), line, event, field);
    if (std::holds_alternative<ParseError>(detected)) {
        return std::get<ParseError>(detected);
    }
    const auto state = parse_recovery(token.substr(comma + 1), line, event, field + 1);
    if (std::holds_alternative<ParseError>(state)) {
        return std::get<ParseError>(state);
    }
    return std::optional<std::pair<std::uint64_t, GapRecoveryState>>{
        std::make_pair(std::get<std::uint64_t>(detected), std::get<GapRecoveryState>(state))};
}

[[nodiscard]] Result<std::pair<std::string, std::string>>
key_value(std::string_view token, std::size_t line, std::size_t event, std::size_t field) {
    const auto separator = token.find('=');
    if (separator == std::string_view::npos || separator == 0 || separator + 1 >= token.size() ||
        token.find('=', separator + 1) != std::string_view::npos) {
        return error(ErrorCategory::InvalidMetadata, line, event, field,
                     "header field must be key=value");
    }
    const auto key = token.substr(0, separator);
    const auto value = token.substr(separator + 1);
    if (!valid_symbolic_token(key) || !valid_symbolic_token(value)) {
        return error(ErrorCategory::InvalidMetadata, line, event, field,
                     "header field contains an invalid token");
    }
    return std::make_pair(std::string(key), std::string(value));
}

[[nodiscard]] Result<ReplayHeader> parse_header(std::string_view line) {
    const auto tokens = split_spaces(line);
    if (tokens.empty() || tokens.front() != kReplaySchemaName) {
        return error(ErrorCategory::UnsupportedSchema, 1, 0, 0, "unsupported replay schema");
    }
    std::map<std::string, std::string> fields;
    const std::array required = {"market",         "symbol", "price_scale",
                                 "quantity_scale", "policy", "fixture_id"};
    std::string previous_provenance;
    for (std::size_t index = 1; index < tokens.size(); ++index) {
        const auto parsed = key_value(tokens[index], 1, 0, index);
        if (std::holds_alternative<ParseError>(parsed)) {
            return std::get<ParseError>(parsed);
        }
        const auto [key, value] = std::get<std::pair<std::string, std::string>>(parsed);
        if (index <= required.size()) {
            if (key != required[index - 1]) {
                return error(ErrorCategory::InvalidMetadata, 1, 0, index,
                             "header fields are not in canonical order");
            }
        } else if (!starts_with(key, "provenance_") ||
                   (!previous_provenance.empty() && key <= previous_provenance)) {
            return error(ErrorCategory::InvalidMetadata, 1, 0, index,
                         "provenance fields are not in canonical order");
        }
        if (fields.contains(key)) {
            return error(ErrorCategory::InvalidMetadata, 1, 0, index, "duplicate header field");
        }
        if (key != "market" && key != "symbol" && key != "price_scale" && key != "quantity_scale" &&
            key != "policy" && key != "fixture_id" && !starts_with(key, "provenance_")) {
            return error(ErrorCategory::InvalidMetadata, 1, 0, index, "unknown header field");
        }
        fields.emplace(key, value);
        if (starts_with(key, "provenance_")) {
            previous_provenance = key;
        }
    }
    for (const auto key : required) {
        if (!fields.contains(key)) {
            return error(ErrorCategory::InvalidMetadata, 1, 0, 0, "missing header field");
        }
    }
    const auto market = parse_market(fields.at("market"), 1, 0, 1);
    const auto policy = parse_policy(fields.at("policy"), 1, 0, 4);
    const auto price_scale = parse_uint32(fields.at("price_scale"), 1, 0, 3);
    const auto quantity_scale = parse_uint32(fields.at("quantity_scale"), 1, 0, 4);
    if (std::holds_alternative<ParseError>(market))
        return std::get<ParseError>(market);
    if (std::holds_alternative<ParseError>(policy))
        return std::get<ParseError>(policy);
    if (std::holds_alternative<ParseError>(price_scale))
        return std::get<ParseError>(price_scale);
    if (std::holds_alternative<ParseError>(quantity_scale)) {
        return std::get<ParseError>(quantity_scale);
    }
    if (std::get<std::uint32_t>(price_scale) > 18U ||
        std::get<std::uint32_t>(quantity_scale) > 18U) {
        return error(ErrorCategory::InvalidMetadata, 1, 0, 3, "NumericSpec scale must be 0..18");
    }
    if (!valid_identifier_token(fields.at("symbol")) ||
        !valid_identifier_token(fields.at("fixture_id"))) {
        return error(ErrorCategory::InvalidMetadata, 1, 0, 0,
                     "symbol and fixture ID must be non-empty identifier tokens");
    }
    if (std::get<Market>(market) == Market::Spot &&
        std::get<SequencePolicy>(policy) != SequencePolicy::Spot) {
        return error(ErrorCategory::InvalidMetadata, 1, 0, 4, "market and policy disagree");
    }
    if (std::get<Market>(market) == Market::UsdMPerpetual &&
        std::get<SequencePolicy>(policy) != SequencePolicy::UsdMPerpetual) {
        return error(ErrorCategory::InvalidMetadata, 1, 0, 4, "market and policy disagree");
    }
    std::vector<std::pair<std::string, std::string>> provenance;
    for (const auto& [key, value] : fields) {
        if (starts_with(key, "provenance_")) {
            provenance.emplace_back(key, value);
        }
    }
    return ReplayHeader{
        kReplaySchemaVersion,
        std::get<Market>(market),
        fields.at("symbol"),
        {std::get<std::uint32_t>(price_scale), std::get<std::uint32_t>(quantity_scale)},
        std::get<SequencePolicy>(policy),
        fields.at("fixture_id"),
        provenance};
}

[[nodiscard]] SourceLocation source(std::size_t event, std::size_t line, std::string_view text) {
    return {event, line, std::string(text)};
}

} // namespace

Result<NormalizedReplay> parse_replay_log(std::string_view bytes) {
    const auto canonical = validate_canonical_bytes(bytes);
    if (std::holds_alternative<ParseError>(canonical)) {
        return std::get<ParseError>(canonical);
    }
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start < bytes.size()) {
        const auto end = bytes.find('\n', start);
        lines.push_back(bytes.substr(start, end == std::string_view::npos ? bytes.size() - start
                                                                          : end - start));
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    const auto header = parse_header(lines.front());
    if (std::holds_alternative<ParseError>(header)) {
        return std::get<ParseError>(header);
    }
    NormalizedReplay replay{std::get<ReplayHeader>(header), {}};
    for (std::size_t line_index = 1; line_index < lines.size(); ++line_index) {
        const auto line = lines[line_index];
        const auto event_index = replay.operations.size();
        const auto tokens = split_spaces(line);
        if (tokens.empty()) {
            return error(ErrorCategory::ReplaySyntax, line_index + 1, event_index, 0,
                         "empty event");
        }
        const auto location = source(event_index, line_index + 1, line);
        if (tokens.front() == "INSTALL_BASELINE" || tokens.front() == "REBASELINE") {
            if (tokens.size() != 4) {
                return error(ErrorCategory::ReplaySyntax, line_index + 1, event_index,
                             tokens.size(), "baseline event requires id, bids, asks");
            }
            const auto id = parse_uint64(tokens[1], line_index + 1, event_index, 1);
            const auto bids = parse_levels(tokens[2], Side::Bid, line_index + 1, event_index, 2);
            const auto asks = parse_levels(tokens[3], Side::Ask, line_index + 1, event_index, 3);
            if (std::holds_alternative<ParseError>(id))
                return std::get<ParseError>(id);
            if (std::holds_alternative<ParseError>(bids))
                return std::get<ParseError>(bids);
            if (std::holds_alternative<ParseError>(asks))
                return std::get<ParseError>(asks);
            if (tokens.front() == "INSTALL_BASELINE") {
                replay.operations.emplace_back(InstallBaselineOp{
                    location, std::get<std::uint64_t>(id), std::get<std::vector<LevelInput>>(bids),
                    std::get<std::vector<LevelInput>>(asks)});
            } else {
                replay.operations.emplace_back(RebaselineOp{
                    location, std::get<std::uint64_t>(id), std::get<std::vector<LevelInput>>(bids),
                    std::get<std::vector<LevelInput>>(asks)});
            }
        } else if (tokens.front() == "DEPTH_UPDATE") {
            if (tokens.size() != 5 || !starts_with(tokens[3], "pu=")) {
                return error(ErrorCategory::ReplaySyntax, line_index + 1, event_index,
                             tokens.size(), "depth event requires first final pu=... levels");
            }
            const auto first = parse_uint64(tokens[1], line_index + 1, event_index, 1);
            const auto final = parse_uint64(tokens[2], line_index + 1, event_index, 2);
            const auto previous =
                parse_optional_uint(tokens[3].substr(3), line_index + 1, event_index, 3);
            const auto levels =
                parse_levels(tokens[4], std::nullopt, line_index + 1, event_index, 4);
            if (std::holds_alternative<ParseError>(first))
                return std::get<ParseError>(first);
            if (std::holds_alternative<ParseError>(final))
                return std::get<ParseError>(final);
            if (std::holds_alternative<ParseError>(previous))
                return std::get<ParseError>(previous);
            if (std::holds_alternative<ParseError>(levels))
                return std::get<ParseError>(levels);
            replay.operations.emplace_back(DepthUpdateOp{
                location, std::get<std::uint64_t>(first), std::get<std::uint64_t>(final),
                std::get<std::optional<std::uint64_t>>(previous),
                std::get<std::vector<LevelInput>>(levels)});
        } else if (tokens.front() == "RESET") {
            if (tokens.size() != 1) {
                return error(ErrorCategory::ReplaySyntax, line_index + 1, event_index,
                             tokens.size(), "reset takes no arguments");
            }
            replay.operations.emplace_back(ResetOp{location});
        } else if (tokens.front() == "ADAPTER_METADATA") {
            if (tokens.size() != 2) {
                return error(ErrorCategory::ReplaySyntax, line_index + 1, event_index,
                             tokens.size(), "adapter metadata requires one quality list");
            }
            const auto facts = parse_quality_list(tokens[1], line_index + 1, event_index, 1);
            if (std::holds_alternative<ParseError>(facts))
                return std::get<ParseError>(facts);
            replay.operations.emplace_back(
                AdapterMetadataOp{location, std::get<std::vector<HostQualityFact>>(facts)});
        } else if (tokens.front() == "MALFORMED_RANGE") {
            if (tokens.size() != 3) {
                return error(ErrorCategory::ReplaySyntax, line_index + 1, event_index,
                             tokens.size(), "malformed range requires first and final IDs");
            }
            const auto first = parse_uint64(tokens[1], line_index + 1, event_index, 1);
            const auto final = parse_uint64(tokens[2], line_index + 1, event_index, 2);
            if (std::holds_alternative<ParseError>(first))
                return std::get<ParseError>(first);
            if (std::holds_alternative<ParseError>(final))
                return std::get<ParseError>(final);
            replay.operations.emplace_back(MalformedRangeOp{
                location, std::get<std::uint64_t>(first), std::get<std::uint64_t>(final)});
        } else if (tokens.front() == "SNAPSHOT_REQUEST") {
            if (tokens.size() != 10) {
                return error(ErrorCategory::ReplaySyntax, line_index + 1, event_index,
                             tokens.size(), "snapshot request has ten fields");
            }
            std::optional<std::uint32_t> depth;
            if (tokens[1] != "-") {
                const auto parsed = parse_uint32(tokens[1], line_index + 1, event_index, 1);
                if (std::holds_alternative<ParseError>(parsed))
                    return std::get<ParseError>(parsed);
                depth = std::get<std::uint32_t>(parsed);
            }
            const auto facts = parse_quality_list(tokens[2], line_index + 1, event_index, 2);
            const auto origin = parse_origin(tokens[6], line_index + 1, event_index, 6);
            const auto generated = parse_uint64(tokens[7], line_index + 1, event_index, 7);
            const auto monotonic = parse_optional_uint(tokens[8], line_index + 1, event_index, 8);
            const auto gap = parse_current_gap(tokens[9], line_index + 1, event_index, 9);
            if (std::holds_alternative<ParseError>(facts))
                return std::get<ParseError>(facts);
            if (std::holds_alternative<ParseError>(origin))
                return std::get<ParseError>(origin);
            if (std::holds_alternative<ParseError>(generated))
                return std::get<ParseError>(generated);
            if (std::holds_alternative<ParseError>(monotonic))
                return std::get<ParseError>(monotonic);
            if (std::holds_alternative<ParseError>(gap))
                return std::get<ParseError>(gap);
            for (const auto token_index : {3U, 4U, 5U}) {
                if (!valid_identifier_token(tokens[token_index])) {
                    return error(ErrorCategory::ReplaySyntax, line_index + 1, event_index,
                                 token_index, "invalid snapshot context token");
                }
            }
            replay.operations.emplace_back(SnapshotRequestOp{
                location, depth, std::get<std::vector<HostQualityFact>>(facts),
                std::string(tokens[3]), std::string(tokens[4]), std::string(tokens[5]),
                std::get<SnapshotOrigin>(origin), std::get<std::uint64_t>(generated),
                std::get<std::optional<std::uint64_t>>(monotonic),
                std::get<std::optional<std::pair<std::uint64_t, GapRecoveryState>>>(gap)});
        } else {
            return error(ErrorCategory::ReplaySyntax, line_index + 1, event_index, 0,
                         "unknown event name");
        }
    }

    for (std::size_t index = 0; index < replay.operations.size(); ++index) {
        if (!std::holds_alternative<AdapterMetadataOp>(replay.operations[index]))
            continue;
        if (index + 1 >= replay.operations.size() ||
            !std::holds_alternative<DepthUpdateOp>(replay.operations[index + 1])) {
            const auto& metadata = std::get<AdapterMetadataOp>(replay.operations[index]);
            return error(ErrorCategory::ReplaySyntax, metadata.source.line_number, index, 0,
                         "ADAPTER_METADATA must immediately precede DEPTH_UPDATE");
        }
    }
    return replay;
}

Result<NormalizedReplay> load_replay_log(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return error(ErrorCategory::IoFailure, 0, 0, 0, "cannot open replay log");
    }
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (!input.good() && !input.eof()) {
        return error(ErrorCategory::IoFailure, 0, 0, 0, "cannot read replay log");
    }
    return parse_replay_log(bytes.str());
}

} // namespace bmd_projection::m5::replay

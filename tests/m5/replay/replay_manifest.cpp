#include "replay_manifest.hpp"

#include "canonical_text.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace bmd_projection::m5::replay {
namespace {

[[nodiscard]] ParseError error(ErrorCategory category, std::size_t line, std::size_t token,
                               std::string message) {
    return {category, line, 0, token, std::move(message)};
}

[[nodiscard]] Result<std::pair<std::string, std::string>> parse_field(std::string_view line,
                                                                      std::size_t line_number) {
    const auto separator = line.find('=');
    if (separator == std::string_view::npos || separator == 0 || separator + 1 >= line.size() ||
        line.find('=', separator + 1) != std::string_view::npos) {
        return error(ErrorCategory::ManifestParse, line_number, 0,
                     "manifest field must be key=value");
    }
    return std::make_pair(std::string(line.substr(0, separator)),
                          std::string(line.substr(separator + 1)));
}

[[nodiscard]] Result<Market> parse_market(std::string_view value, std::size_t line) {
    if (value == "Spot") {
        return Market::Spot;
    }
    if (value == "UsdMPerpetual") {
        return Market::UsdMPerpetual;
    }
    return error(ErrorCategory::ManifestParse, line, 0, "unknown manifest market");
}

[[nodiscard]] Result<SequencePolicy> parse_policy(std::string_view value, std::size_t line) {
    if (value == "Spot") {
        return SequencePolicy::Spot;
    }
    if (value == "UsdMPerpetual") {
        return SequencePolicy::UsdMPerpetual;
    }
    return error(ErrorCategory::ManifestParse, line, 0, "unknown manifest policy");
}

constexpr std::array<std::string_view, 9> kRequiredFields = {
    "fixture_id",  "schema_version", "log_sha256", "market",     "symbol",
    "price_scale", "quantity_scale", "policy",     "event_count"};

constexpr std::string_view kTokenCharacters =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._:/+-";

[[nodiscard]] bool valid_token(std::string_view token) {
    return !token.empty() && token != "-" &&
           token.find_first_not_of(kTokenCharacters) == std::string_view::npos;
}

[[nodiscard]] bool is_hex_digit(char value) {
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
}

[[nodiscard]] bool valid_sha256(std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, is_hex_digit);
}

struct ManifestFields final {
    std::map<std::string, std::string> fields;
    std::vector<std::pair<std::string, std::string>> provenance;
};

[[nodiscard]] Result<std::vector<std::string_view>> manifest_lines(std::string_view bytes) {
    const auto canonical = validate_canonical_bytes(bytes);
    if (std::holds_alternative<ParseError>(canonical)) {
        auto failure = std::get<ParseError>(canonical);
        failure.category = ErrorCategory::ManifestParse;
        failure.message = "invalid canonical manifest bytes: " + failure.message;
        return failure;
    }
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start < bytes.size()) {
        const auto end = bytes.find('\n', start);
        lines.push_back(bytes.substr(start, end == std::string_view::npos ? bytes.size() - start
                                                                          : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return lines;
}

[[nodiscard]] Result<ManifestFields> collect_fields(const std::vector<std::string_view>& lines) {
    ManifestFields collected;
    std::map<std::string, bool> seen_keys;
    std::string previous_provenance;
    for (std::size_t index = 1; index < lines.size(); ++index) {
        const auto parsed = parse_field(lines[index], index + 1);
        if (std::holds_alternative<ParseError>(parsed)) {
            return std::get<ParseError>(parsed);
        }
        const auto [key, value] = std::get<std::pair<std::string, std::string>>(parsed);
        if (!valid_token(key) || !valid_token(value)) {
            return error(ErrorCategory::ManifestParse, index + 1, 0,
                         "manifest field contains an invalid token");
        }
        if (index <= kRequiredFields.size()) {
            if (key != kRequiredFields.at(index - 1)) {
                return error(ErrorCategory::ManifestParse, index + 1, 0,
                             "manifest fields are not in canonical order");
            }
        } else if (!key.starts_with("provenance_") ||
                   (!previous_provenance.empty() && key <= previous_provenance)) {
            return error(ErrorCategory::ManifestParse, index + 1, 0,
                         "provenance fields are not in canonical order");
        }
        if (seen_keys.contains(key)) {
            return error(ErrorCategory::ManifestParse, index + 1, 0, "duplicate manifest field");
        }
        seen_keys.emplace(key, true);
        if (key.starts_with("provenance_")) {
            collected.provenance.emplace_back(key, value);
            previous_provenance = key;
        } else {
            collected.fields.emplace(key, value);
        }
    }
    return collected;
}

[[nodiscard]] Result<std::monostate> check_required_present(const ManifestFields& collected) {
    for (const auto& key : kRequiredFields) {
        if (!collected.fields.contains(std::string{key})) {
            return error(ErrorCategory::ManifestParse, 0, 0, "missing manifest field");
        }
    }
    return std::monostate{};
}

[[nodiscard]] bool is_known_field(std::string_view key) {
    return std::ranges::find(kRequiredFields, key) != kRequiredFields.end();
}

[[nodiscard]] Result<std::monostate> check_unknown_fields(const ManifestFields& collected) {
    for (const auto& [key, unused] : collected.fields) {
        static_cast<void>(unused);
        if (!is_known_field(key)) {
            return error(ErrorCategory::ManifestParse, 0, 0, "unknown manifest field");
        }
    }
    return std::monostate{};
}

[[nodiscard]] Result<std::monostate> check_identity_fields(const ManifestFields& collected) {
    if (collected.fields.at("schema_version") != kReplaySchemaName) {
        return error(ErrorCategory::UnsupportedSchema, 0, 0, "unsupported manifest schema");
    }
    if (!valid_sha256(collected.fields.at("log_sha256"))) {
        return error(ErrorCategory::ManifestParse, 0, 0, "invalid schema or log SHA-256");
    }
    return std::monostate{};
}

struct MarketPolicy final {
    Market market{};
    SequencePolicy policy{};
};

[[nodiscard]] Result<MarketPolicy> parse_market_policy(const ManifestFields& collected) {
    const auto parsed_market = parse_market(collected.fields.at("market"), 0);
    const auto parsed_policy = parse_policy(collected.fields.at("policy"), 0);
    if (std::holds_alternative<ParseError>(parsed_market)) {
        return std::get<ParseError>(parsed_market);
    }
    if (std::holds_alternative<ParseError>(parsed_policy)) {
        return std::get<ParseError>(parsed_policy);
    }
    return MarketPolicy{std::get<Market>(parsed_market), std::get<SequencePolicy>(parsed_policy)};
}

[[nodiscard]] Result<std::monostate>
check_market_policy_agreement(const MarketPolicy& market_policy) {
    if ((market_policy.market == Market::Spot && market_policy.policy != SequencePolicy::Spot) ||
        (market_policy.market == Market::UsdMPerpetual &&
         market_policy.policy != SequencePolicy::UsdMPerpetual)) {
        return error(ErrorCategory::ManifestParse, 0, 0, "manifest market and policy disagree");
    }
    return std::monostate{};
}

struct ManifestScalars final {
    std::uint32_t price_scale{};
    std::uint32_t quantity_scale{};
    std::size_t event_count{};
};

[[nodiscard]] Result<ManifestScalars> parse_scalars(const ManifestFields& collected) {
    const auto price_scale = parse_uint32(collected.fields.at("price_scale"), 0, 0, 0);
    const auto quantity_scale = parse_uint32(collected.fields.at("quantity_scale"), 0, 0, 0);
    const auto event_count = parse_uint64(collected.fields.at("event_count"), 0, 0, 0);
    if (std::holds_alternative<ParseError>(price_scale)) {
        return std::get<ParseError>(price_scale);
    }
    if (std::holds_alternative<ParseError>(quantity_scale)) {
        return std::get<ParseError>(quantity_scale);
    }
    if (std::holds_alternative<ParseError>(event_count)) {
        return std::get<ParseError>(event_count);
    }
    if (std::get<std::uint64_t>(event_count) > SIZE_MAX) {
        return error(ErrorCategory::ManifestParse, 0, 0, "event count exceeds platform size");
    }
    return ManifestScalars{std::get<std::uint32_t>(price_scale),
                           std::get<std::uint32_t>(quantity_scale),
                           static_cast<std::size_t>(std::get<std::uint64_t>(event_count))};
}

[[nodiscard]] Result<std::monostate> check_scale_and_identity(const ManifestFields& collected,
                                                              const ManifestScalars& scalars) {
    if (scalars.price_scale > 18U || scalars.quantity_scale > 18U ||
        collected.fields.at("fixture_id") == "-" || collected.fields.at("symbol") == "-") {
        return error(ErrorCategory::ManifestParse, 0, 0, "invalid NumericSpec or identity token");
    }
    return std::monostate{};
}

[[nodiscard]] ReplayManifest build_manifest(const ManifestFields& collected,
                                            const ManifestScalars& scalars,
                                            const MarketPolicy& market_policy) {
    return ReplayManifest{{kReplaySchemaVersion,
                           collected.fields.at("log_sha256"),
                           collected.fields.at("fixture_id"),
                           market_policy.market,
                           collected.fields.at("symbol"),
                           {scalars.price_scale, scalars.quantity_scale},
                           market_policy.policy},
                          scalars.event_count,
                          collected.provenance};
}

} // namespace

Result<ReplayManifest> parse_manifest(std::string_view bytes) {
    const auto lines = manifest_lines(bytes);
    if (std::holds_alternative<ParseError>(lines)) {
        return std::get<ParseError>(lines);
    }
    const auto& split_lines = std::get<std::vector<std::string_view>>(lines);
    if (split_lines.empty() || split_lines.front() != "MANIFEST_V1") {
        return error(ErrorCategory::UnsupportedSchema, 1, 0, "unsupported manifest schema");
    }
    const auto collected = collect_fields(split_lines);
    if (std::holds_alternative<ParseError>(collected)) {
        return std::get<ParseError>(collected);
    }
    const auto& parsed_fields = std::get<ManifestFields>(collected);
    const auto required = check_required_present(parsed_fields);
    if (std::holds_alternative<ParseError>(required)) {
        return std::get<ParseError>(required);
    }
    const auto unknown = check_unknown_fields(parsed_fields);
    if (std::holds_alternative<ParseError>(unknown)) {
        return std::get<ParseError>(unknown);
    }
    const auto identity = check_identity_fields(parsed_fields);
    if (std::holds_alternative<ParseError>(identity)) {
        return std::get<ParseError>(identity);
    }
    const auto market_policy = parse_market_policy(parsed_fields);
    if (std::holds_alternative<ParseError>(market_policy)) {
        return std::get<ParseError>(market_policy);
    }
    const auto scalars = parse_scalars(parsed_fields);
    if (std::holds_alternative<ParseError>(scalars)) {
        return std::get<ParseError>(scalars);
    }
    const auto bounded =
        check_scale_and_identity(parsed_fields, std::get<ManifestScalars>(scalars));
    if (std::holds_alternative<ParseError>(bounded)) {
        return std::get<ParseError>(bounded);
    }
    const auto agreement = check_market_policy_agreement(std::get<MarketPolicy>(market_policy));
    if (std::holds_alternative<ParseError>(agreement)) {
        return std::get<ParseError>(agreement);
    }
    return build_manifest(parsed_fields, std::get<ManifestScalars>(scalars),
                          std::get<MarketPolicy>(market_policy));
}

Result<ReplayManifest> load_manifest(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return error(ErrorCategory::IoFailure, 0, 0, "cannot open manifest");
    }
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (!input.good() && !input.eof()) {
        return error(ErrorCategory::IoFailure, 0, 0, "cannot read manifest");
    }
    return parse_manifest(bytes.str());
}

} // namespace bmd_projection::m5::replay

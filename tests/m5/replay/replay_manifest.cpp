#include "replay_manifest.hpp"

#include "canonical_text.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <sstream>

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

[[nodiscard]] Result<Market> market(std::string_view value, std::size_t line) {
    if (value == "Spot")
        return Market::Spot;
    if (value == "UsdMPerpetual")
        return Market::UsdMPerpetual;
    return error(ErrorCategory::ManifestParse, line, 0, "unknown manifest market");
}

[[nodiscard]] Result<SequencePolicy> policy(std::string_view value, std::size_t line) {
    if (value == "Spot")
        return SequencePolicy::Spot;
    if (value == "UsdMPerpetual")
        return SequencePolicy::UsdMPerpetual;
    return error(ErrorCategory::ManifestParse, line, 0, "unknown manifest policy");
}

[[nodiscard]] bool valid_sha256(std::string_view value) {
    if (value.size() != 64)
        return false;
    for (const auto character : value) {
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

} // namespace

Result<ReplayManifest> parse_manifest(std::string_view bytes) {
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
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    if (lines.empty() || lines.front() != "MANIFEST_V1") {
        return error(ErrorCategory::UnsupportedSchema, 1, 0, "unsupported manifest schema");
    }
    std::map<std::string, std::string> fields;
    std::map<std::string, bool> seen_keys;
    std::vector<std::pair<std::string, std::string>> provenance;
    const std::array required = {"fixture_id",     "schema_version", "log_sha256",
                                 "market",         "symbol",         "price_scale",
                                 "quantity_scale", "policy",         "event_count"};
    std::string previous_provenance;
    for (std::size_t index = 1; index < lines.size(); ++index) {
        const auto parsed = parse_field(lines[index], index + 1);
        if (std::holds_alternative<ParseError>(parsed))
            return std::get<ParseError>(parsed);
        const auto [key, value] = std::get<std::pair<std::string, std::string>>(parsed);
        if (key.empty() || value.empty() || key == "-" || value == "-" ||
            key.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._:/+-") !=
                std::string::npos ||
            value.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._:/+-") !=
                std::string::npos) {
            return error(ErrorCategory::ManifestParse, index + 1, 0,
                         "manifest field contains an invalid token");
        }
        if (index <= required.size()) {
            if (key != required[index - 1]) {
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
            provenance.emplace_back(key, value);
            previous_provenance = key;
        } else {
            fields.emplace(key, value);
        }
    }
    for (const auto key : required) {
        if (!fields.contains(key)) {
            return error(ErrorCategory::ManifestParse, 0, 0, "missing manifest field");
        }
    }
    for (const auto& [key, unused] : fields) {
        static_cast<void>(unused);
        if (key != "fixture_id" && key != "schema_version" && key != "log_sha256" &&
            key != "market" && key != "symbol" && key != "price_scale" && key != "quantity_scale" &&
            key != "policy" && key != "event_count") {
            return error(ErrorCategory::ManifestParse, 0, 0, "unknown manifest field");
        }
    }
    if (fields.at("schema_version") != kReplaySchemaName) {
        return error(ErrorCategory::UnsupportedSchema, 0, 0, "unsupported manifest schema");
    }
    if (!valid_sha256(fields.at("log_sha256"))) {
        return error(ErrorCategory::ManifestParse, 0, 0, "invalid schema or log SHA-256");
    }
    const auto parsed_market = market(fields.at("market"), 0);
    const auto parsed_policy = policy(fields.at("policy"), 0);
    const auto price_scale = parse_uint32(fields.at("price_scale"), 0, 0, 0);
    const auto quantity_scale = parse_uint32(fields.at("quantity_scale"), 0, 0, 0);
    const auto event_count = parse_uint64(fields.at("event_count"), 0, 0, 0);
    if (std::holds_alternative<ParseError>(parsed_market))
        return std::get<ParseError>(parsed_market);
    if (std::holds_alternative<ParseError>(parsed_policy))
        return std::get<ParseError>(parsed_policy);
    if (std::holds_alternative<ParseError>(price_scale))
        return std::get<ParseError>(price_scale);
    if (std::holds_alternative<ParseError>(quantity_scale))
        return std::get<ParseError>(quantity_scale);
    if (std::holds_alternative<ParseError>(event_count))
        return std::get<ParseError>(event_count);
    if (std::get<std::uint64_t>(event_count) > SIZE_MAX) {
        return error(ErrorCategory::ManifestParse, 0, 0, "event count exceeds platform size");
    }
    const auto actual_market = std::get<Market>(parsed_market);
    const auto actual_policy = std::get<SequencePolicy>(parsed_policy);
    if (std::get<std::uint32_t>(price_scale) > 18U ||
        std::get<std::uint32_t>(quantity_scale) > 18U || fields.at("fixture_id") == "-" ||
        fields.at("symbol") == "-") {
        return error(ErrorCategory::ManifestParse, 0, 0, "invalid NumericSpec or identity token");
    }
    std::sort(provenance.begin(), provenance.end());
    if ((actual_market == Market::Spot && actual_policy != SequencePolicy::Spot) ||
        (actual_market == Market::UsdMPerpetual &&
         actual_policy != SequencePolicy::UsdMPerpetual)) {
        return error(ErrorCategory::ManifestParse, 0, 0, "manifest market and policy disagree");
    }
    return ReplayManifest{
        {kReplaySchemaVersion,
         fields.at("log_sha256"),
         fields.at("fixture_id"),
         actual_market,
         fields.at("symbol"),
         {std::get<std::uint32_t>(price_scale), std::get<std::uint32_t>(quantity_scale)},
         actual_policy},
        static_cast<std::size_t>(std::get<std::uint64_t>(event_count)),
        provenance};
}

Result<ReplayManifest> load_manifest(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return error(ErrorCategory::IoFailure, 0, 0, "cannot open manifest");
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (!input.good() && !input.eof())
        return error(ErrorCategory::IoFailure, 0, 0, "cannot read manifest");
    return parse_manifest(bytes.str());
}

} // namespace bmd_projection::m5::replay

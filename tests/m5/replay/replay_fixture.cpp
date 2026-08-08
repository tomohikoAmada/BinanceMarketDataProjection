#include "replay_fixture.hpp"

#include "canonical_text.hpp"
#include "replay_manifest.hpp"
#include "replay_parser.hpp"

#include <fstream>
#include <sstream>

namespace bmd_projection::m5::replay {
namespace {

[[nodiscard]] ParseError error(ErrorCategory category, std::string message) {
    return {category, 0, 0, 0, std::move(message)};
}

[[nodiscard]] Result<std::string> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return error(ErrorCategory::IoFailure, "cannot open fixture file");
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (!input.good() && !input.eof())
        return error(ErrorCategory::IoFailure, "cannot read fixture file");
    return bytes.str();
}

} // namespace

Result<ReplayFixture> load_fixture(const std::filesystem::path& directory) {
    const auto log_bytes = read_bytes(directory / "replay.log");
    if (std::holds_alternative<ParseError>(log_bytes))
        return std::get<ParseError>(log_bytes);
    const auto manifest = load_manifest(directory / "manifest.txt");
    if (std::holds_alternative<ParseError>(manifest))
        return std::get<ParseError>(manifest);
    const auto replay = parse_replay_log(std::get<std::string>(log_bytes));
    if (std::holds_alternative<ParseError>(replay))
        return std::get<ParseError>(replay);
    const auto hash = sha256_hex(std::get<std::string>(log_bytes));
    if (std::holds_alternative<ParseError>(hash))
        return std::get<ParseError>(hash);
    const auto& normalized = std::get<NormalizedReplay>(replay);
    const auto& parsed_manifest = std::get<ReplayManifest>(manifest);
    const auto actual_hash = std::get<std::string>(hash);
    const FixtureIdentity header_identity{
        normalized.header.schema_version, actual_hash,
        normalized.header.fixture_id,     normalized.header.market,
        normalized.header.symbol,         normalized.header.numeric_spec,
        normalized.header.sequence_policy};
    if (parsed_manifest.identity.replay_log_sha256 != actual_hash) {
        return error(ErrorCategory::IdentityMismatch,
                     "manifest log SHA-256 does not match canonical bytes");
    }
    if (parsed_manifest.event_count != normalized.operations.size()) {
        return error(ErrorCategory::EventCountMismatch,
                     "manifest event count does not match replay log");
    }
    if (parsed_manifest.identity.schema_version != header_identity.schema_version ||
        parsed_manifest.identity.fixture_id != header_identity.fixture_id ||
        parsed_manifest.identity.market != header_identity.market ||
        parsed_manifest.identity.symbol != header_identity.symbol ||
        parsed_manifest.identity.numeric_spec != header_identity.numeric_spec ||
        parsed_manifest.identity.sequence_policy != header_identity.sequence_policy ||
        parsed_manifest.identity.replay_log_sha256 != header_identity.replay_log_sha256) {
        return error(ErrorCategory::IdentityMismatch,
                     "manifest interpretation metadata does not match replay header");
    }
    if (parsed_manifest.provenance != normalized.header.provenance) {
        return error(ErrorCategory::IdentityMismatch,
                     "manifest provenance does not match replay header");
    }
    if (parsed_manifest.identity.symbol.empty() || normalized.header.fixture_id.empty()) {
        return error(ErrorCategory::InvalidMetadata, "fixture identity contains an empty field");
    }
    return ReplayFixture{normalized, parsed_manifest, header_identity, actual_hash};
}

} // namespace bmd_projection::m5::replay

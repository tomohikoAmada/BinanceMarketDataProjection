#include "replay_types.hpp"

namespace bmd_projection::m5::replay {

MaterializerContract spot_materializer_contract() {
    return {MaterializerContract::MarketRule::Spot,
            "REST depth snapshot lastUpdateId=L",
            "all diff-depth events received from stream start through snapshot acquisition",
            "discard buffered events with u < L+1",
            "first eligible event satisfies U <= L+1 <= u",
            "after bridge, each next event covers local_last_update_id+1; apply u",
            "snapshot-too-old, forward gap, integrity, or provenance failure rejects the fixture "
            "and triggers resync",
            "consume only explicitly supplied immutable source archive plus expected provenance "
            "identity; no live discovery"};
}

MaterializerContract usdm_materializer_contract() {
    return {MaterializerContract::MarketRule::UsdMPerpetual,
            "REST depth snapshot lastUpdateId=L",
            "all U/u/pu diff-depth events received from stream start through snapshot acquisition",
            "discard buffered events with u < L",
            "first eligible event satisfies U <= L <= u",
            "after bridge, each next event requires pu == local_last_update_id and applies u",
            "snapshot-too-old, missing/mismatched pu, gap, integrity, or provenance failure "
            "rejects the fixture and triggers resync",
            "consume only explicitly supplied immutable source archive plus expected provenance "
            "identity; no live discovery"};
}

Result<std::monostate> validate_source_archive(const ImmutableSourceArchive& source) {
    if (source.root_identity.empty() || source.recorder_commit.empty() ||
        source.recorder_wheel_sha256.size() != 64 || source.recorder_config_sha256.size() != 64 ||
        source.run_identity.empty() || source.raw_chunk_sha256.empty() || source.market.empty() ||
        source.symbol.empty() || source.source_interval.empty()) {
        return ParseError{ErrorCategory::InvalidMetadata, 0, 0, 0,
                          "materializer source archive requires complete provenance identity"};
    }
    if (source.market != "Spot" && source.market != "UsdMPerpetual") {
        return ParseError{ErrorCategory::InvalidMetadata, 0, 0, 0, "unsupported source market"};
    }
    const auto valid_hash = [](const std::string& value) {
        if (value.size() != 64)
            return false;
        for (const auto character : value) {
            if (!((character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f'))) {
                return false;
            }
        }
        return true;
    };
    if (!valid_hash(source.recorder_wheel_sha256) || !valid_hash(source.recorder_config_sha256)) {
        return ParseError{ErrorCategory::InvalidMetadata, 0, 0, 0,
                          "Recorder wheel/config identity must be lowercase SHA-256"};
    }
    for (const auto& [chunk, hash] : source.raw_chunk_sha256) {
        if (chunk.empty() || !valid_hash(hash)) {
            return ParseError{ErrorCategory::InvalidMetadata, 0, 0, 0,
                              "raw chunk provenance requires lowercase SHA-256"};
        }
    }
    return std::monostate{};
}

} // namespace bmd_projection::m5::replay

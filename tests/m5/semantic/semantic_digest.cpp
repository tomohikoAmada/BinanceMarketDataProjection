#include "semantic_digest.hpp"

#include "../replay/canonical_text.hpp"

#include <string>
#include <variant>

namespace bmd_projection::m5::semantic {

std::string compute_semantic_digest(const std::vector<std::string>& canonical_observations) {
    std::string concatenated;
    for (const auto& canon : canonical_observations) {
        concatenated += canon;
        concatenated += '\n';
    }
    const auto hash_result = replay::sha256_hex(concatenated);
    if (!std::holds_alternative<std::string>(hash_result)) {
        return {};
    }
    return std::get<std::string>(hash_result);
}

std::string compute_semantic_digest_from_observations(
    const std::vector<oracle::OperationObservation>& observations) {
    return compute_semantic_digest(serialize_observation_stream(observations));
}

} // namespace bmd_projection::m5::semantic

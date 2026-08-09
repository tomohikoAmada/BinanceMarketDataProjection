#pragma once

// Test-only independently variable M4 dimensions. This support model is separate
// from canonical replay grammar and production APIs. Production and R4 map these
// neutral values through their own enum/value models.

#include "replay_types.hpp"

#include <cstdint>
#include <string>

namespace bmd_projection::m5::oracle {

enum class ScenarioVenue : std::uint8_t {
    Binance,
    Unspecified,
    UnknownNumeric,
};

enum class ScenarioMarket : std::uint8_t {
    Spot,
    UsdMPerpetual,
    Unspecified,
    UnknownNumeric,
};

struct AdapterScenario final {
    ScenarioVenue wire_venue{ScenarioVenue::Binance};
    ScenarioMarket wire_market{ScenarioMarket::Spot};
    std::string wire_symbol;
    std::string expected_symbol;
    replay::SequencePolicy expected_policy{replay::SequencePolicy::Spot};
    replay::NumericSpec conversion_numeric_spec;
    replay::NumericSpec projection_numeric_spec;
    replay::SequencePolicy projection_policy{replay::SequencePolicy::Spot};

    friend bool operator==(const AdapterScenario&, const AdapterScenario&) = default;
};

[[nodiscard]] inline AdapterScenario
default_adapter_scenario(const replay::ReplayFixture& fixture) {
    const auto policy = fixture.identity.sequence_policy;
    return {ScenarioVenue::Binance,
            policy == replay::SequencePolicy::Spot ? ScenarioMarket::Spot
                                                   : ScenarioMarket::UsdMPerpetual,
            fixture.identity.symbol,
            fixture.identity.symbol,
            policy,
            fixture.identity.numeric_spec,
            fixture.identity.numeric_spec,
            policy};
}

} // namespace bmd_projection::m5::oracle

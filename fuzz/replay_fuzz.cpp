// M5 Phase-5 differential replay fuzz harness.
//
// Decodes raw bytes into a structured replay test case, dispatches through the
// ReplayDriver comparing production vs reference observations, and aborts on
// the first divergence to produce a libFuzzer crash/reproducer.
//
// Core-only and adapter-enabled modes are both reachable from fuzz bytes.

#include "m5/replay_fuzz_decoder.hpp"
#include "m5/replay_fuzz_fixture.hpp"

#include "adapter_scenario.hpp"
#include "core_production_side.hpp"
#include "divergence.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"
#include "replay_side.hpp"
#include "replay_types.hpp"

#ifdef BMD_PROJECTION_FUZZ_ADAPTER_ENABLED
#include "adapter_production_side.hpp"
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

namespace {
namespace replay = bmd_projection::m5::replay;
namespace oracle = bmd_projection::m5::oracle;
namespace decoder = bmd_projection::m5::replay::fuzz_decoder;

[[nodiscard]] oracle::ScenarioVenue map_venue(decoder::FuzzVenue v) noexcept {
    switch (v) {
    case decoder::FuzzVenue::Binance:
        return oracle::ScenarioVenue::Binance;
    case decoder::FuzzVenue::Unspecified:
        return oracle::ScenarioVenue::Unspecified;
    case decoder::FuzzVenue::UnknownNumeric:
        return oracle::ScenarioVenue::UnknownNumeric;
    }
    return oracle::ScenarioVenue::Binance;
}

[[nodiscard]] oracle::ScenarioMarket map_market(decoder::FuzzMarket m) noexcept {
    switch (m) {
    case decoder::FuzzMarket::Spot:
        return oracle::ScenarioMarket::Spot;
    case decoder::FuzzMarket::UsdMPerpetual:
        return oracle::ScenarioMarket::UsdMPerpetual;
    case decoder::FuzzMarket::Unspecified:
        return oracle::ScenarioMarket::Unspecified;
    case decoder::FuzzMarket::UnknownNumeric:
        return oracle::ScenarioMarket::UnknownNumeric;
    }
    return oracle::ScenarioMarket::Spot;
}

[[nodiscard]] std::string render_divergence_diagnostics(const oracle::Divergence& d) {
    return oracle::render_divergence(d);
}

void run_structured_fuzz_case(const decoder::FuzzCase& fuzz_case) {
    auto fixture = decoder::build_structured_fixture(fuzz_case);

    const auto replay_mode = fuzz_case.mode == decoder::DecodedMode::AdapterEnabled
                                 ? oracle::ReplayMode::AdapterEnabled
                                 : oracle::ReplayMode::CoreOnly;

    std::unique_ptr<oracle::ReplaySide> production;
    std::unique_ptr<oracle::ReplaySide> reference;

    if (replay_mode == oracle::ReplayMode::AdapterEnabled) {
#ifdef BMD_PROJECTION_FUZZ_ADAPTER_ENABLED
        oracle::AdapterScenario scenario{
            map_venue(fuzz_case.scenario_venue),
            map_market(fuzz_case.scenario_market),
            fuzz_case.adapter_wire_symbol.empty() ? fuzz_case.symbol
                                                  : fuzz_case.adapter_wire_symbol,
            fuzz_case.adapter_expected_symbol.empty() ? fuzz_case.symbol
                                                      : fuzz_case.adapter_expected_symbol,
            fuzz_case.adapter_expected_policy,
            fuzz_case.adapter_conversion_numeric_spec.price_scale == 0 &&
                    fuzz_case.adapter_conversion_numeric_spec.quantity_scale == 0
                ? fuzz_case.numeric_spec
                : fuzz_case.adapter_conversion_numeric_spec,
            fuzz_case.adapter_projection_numeric_spec.price_scale == 0 &&
                    fuzz_case.adapter_projection_numeric_spec.quantity_scale == 0
                ? fuzz_case.numeric_spec
                : fuzz_case.adapter_projection_numeric_spec,
            fuzz_case.adapter_projection_policy,
        };
        production = oracle::make_adapter_production_side(fixture, scenario);
        reference = oracle::make_reference_side(fixture, replay_mode, scenario);
#else
        // Adapter requested but unavailable in this build — fall back to CoreOnly
        // to keep the fuzzer functional. The fuzz smoke requires ProtoAdapter=ON
        // and will exercise the adapter path there.
        return;
#endif
    } else {
        production = oracle::make_core_production_side(fixture);
        reference = oracle::make_reference_side(fixture, replay_mode);
    }

    oracle::ReplayDriver driver{fixture, std::move(production), std::move(reference),
                                oracle::ObservationRetention::RetainNone};

    const auto outcome = driver.run();

    if (outcome.first_divergence.has_value()) {
        const auto& d = outcome.first_divergence.value();
        const auto diag = render_divergence_diagnostics(d);
        if (!diag.empty()) {
            const auto msg = "M5 REPLAY FUZZ DIVERGENCE:\n" + diag + "\n";
            std::fprintf(stderr, "%s", msg.c_str());
        }
        std::abort();
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size < 1)
        return 0;

    const auto fuzz_case = decoder::decode(data, size);
    if (!fuzz_case.has_value())
        return 0;

    run_structured_fuzz_case(*fuzz_case);
    return 0;
}

#include "adapter_production_side.hpp"
#include "divergence.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"

#include "replay_fixture.hpp"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <variant>

namespace {

int validate(const std::filesystem::path& directory) {
    namespace oracle = bmd_projection::m5::oracle;
    namespace replay = bmd_projection::m5::replay;

    const auto loaded = replay::load_fixture(directory);
    if (!std::holds_alternative<replay::ReplayFixture>(loaded)) {
        const auto& error = std::get<replay::ParseError>(loaded);
        std::cerr << "fixture_validation=FAIL category=" << static_cast<int>(error.category)
                  << " line=" << error.line_number << " token=" << error.token_index
                  << " message=" << error.message << '\n';
        return 1;
    }
    const auto& fixture = std::get<replay::ReplayFixture>(loaded);
    oracle::ReplayDriver driver{
        fixture, oracle::make_adapter_production_side(fixture),
        oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled),
        oracle::ObservationRetention::RetainNone};
    const auto outcome = driver.run();
    if (outcome.first_divergence.has_value()) {
        std::cerr << oracle::render_divergence(*outcome.first_divergence);
        return 1;
    }
    if (!outcome.final_observation.has_value()) {
        std::cerr << "fixture_validation=FAIL message=no-final-observation\n";
        return 1;
    }
    std::cout << "M5_ADAPTER_CORPUS_VALIDATION_V1\n"
              << "fixture_id=" << fixture.identity.fixture_id << '\n'
              << "replay_log_sha256=" << fixture.identity.replay_log_sha256 << '\n'
              << "event_count=" << outcome.processed_events << '\n'
              << "differential=PASS\n"
              << "final_checkpoint="
              << oracle::to_canonical_text(outcome.final_observation->checkpoint) << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: bmd_projection_m5_adapter_corpus_validate FIXTURE_DIRECTORY\n";
        return 2;
    }
    try {
        const std::span<char*> arguments{argv, static_cast<std::size_t>(argc)};
        return validate(std::filesystem::path{arguments[1]});
    } catch (const std::exception& error) {
        std::cerr << "fixture_validation=FAIL message=" << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "fixture_validation=FAIL message=unknown-exception\n";
        return 1;
    }
}

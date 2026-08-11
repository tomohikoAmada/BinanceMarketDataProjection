#pragma once

// Shared Phase-3 corpus validation body used by both validator executables.
//
// Success requires differential equality (production/reference) AND medium
// lifecycle validity for the materializer-generated corpus. This is a header
// template so the Core-only validator never links ProtoAdapter while the
// adapter validator drives the real M4 production side.

#include "divergence.hpp"
#include "medium_validity.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"

#include "replay_fixture.hpp"

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace bmd_projection::m5::phase3 {

template <typename ProductionFactory>
int run_corpus_validation_impl(const std::filesystem::path& directory,
                               ProductionFactory make_production, std::string_view pass_label,
                               oracle::ReplayMode mode) {
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
    oracle::ReplayDriver driver{fixture, make_production(fixture),
                                oracle::make_reference_side(fixture, mode),
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
    const auto target = read_target_live_updates(directory);
    if (!target.has_value()) {
        std::cerr << "fixture_validation=FAIL reason=corpus-provenance-missing\n";
        return 1;
    }
    const auto report = check_medium_validity(outcome, fixture, *target);
    if (!report.valid) {
        std::cerr << "fixture_validation=FAIL reason=" << report.reason
                  << " event_index=" << report.event_index << '\n';
        return 1;
    }

    const auto disposition_text = [](std::optional<oracle::CompactInstallResult> install) {
        return install.has_value() ? std::string(oracle::to_text(install->disposition)) : "-";
    };
    const auto status_after_install_text = [](std::optional<oracle::CompactInstallResult> install) {
        return install.has_value() ? std::string(oracle::to_text(install->status_after)) : "-";
    };
    const auto depth_disposition_text = [](std::optional<oracle::CompactDepthResult> depth) {
        return depth.has_value() ? std::string(oracle::to_text(depth->disposition)) : "-";
    };
    const auto depth_status_after_text = [](std::optional<oracle::CompactDepthResult> depth) {
        return depth.has_value() ? std::string(oracle::to_text(depth->status_after)) : "-";
    };
    const auto id_text = [](std::optional<std::uint64_t> value) {
        return value.has_value() ? std::to_string(*value) : "-";
    };

    std::cout << pass_label << '\n'
              << "fixture_id=" << fixture.identity.fixture_id << '\n'
              << "replay_log_sha256=" << fixture.identity.replay_log_sha256 << '\n'
              << "event_count=" << outcome.processed_events << '\n'
              << "differential=PASS\n"
              << "medium_validation=PASS\n"
              << "target_live_updates=" << report.target_live_updates << '\n'
              << "baseline_result=" << disposition_text(report.first_install) << '\n'
              << "status_after_baseline=" << status_after_install_text(report.first_install) << '\n'
              << "bridge_result=" << depth_disposition_text(report.first_depth_update) << '\n'
              << "status_after_bridge=" << depth_status_after_text(report.first_depth_update)
              << '\n'
              << "installed_count=" << report.installed_count << '\n'
              << "applied_count=" << report.applied_count << '\n'
              << "ignored_stale_count=" << report.ignored_stale_count << '\n'
              << "ignored_duplicate_count=" << report.ignored_duplicate_count << '\n'
              << "gap_detected_count=" << report.gap_detected_count << '\n'
              << "rejected_wrong_state_count=" << report.rejected_wrong_state_count << '\n'
              << "adapter_error_count=" << report.adapter_error_count << '\n'
              << "final_status=" << oracle::to_text(report.final_status) << '\n'
              << "final_accepted_update_id=" << id_text(report.final_accepted_update_id) << '\n'
              << "last_selected_diff_final_update_id="
              << id_text(report.last_selected_diff_final_update_id) << '\n'
              << "final_checkpoint="
              << oracle::to_canonical_text(outcome.final_observation->checkpoint) << '\n';
    return 0;
}

} // namespace bmd_projection::m5::phase3

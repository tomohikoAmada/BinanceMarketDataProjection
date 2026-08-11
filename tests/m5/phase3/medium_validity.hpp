#pragma once

// Phase-3 medium-corpus lifecycle validity gate.
//
// Differential equality between the production pipeline and the layered
// reference oracle is necessary but NOT sufficient for a mandatory
// materializer-generated medium corpus. This gate derives lifecycle validity
// exclusively from the already-produced, already-compared typed
// OperationObservation values (via the driver's neutral ExecutionSummary) and
// from the fixture's own operation sequence. It contains no Spot/USD-M
// bootstrap or live classification, no pu arithmetic, no decimal parsing, no
// book mutation, and no adapter mapping.
//
// The mandatory emitted corpus shape is: 1 INSTALL_BASELINE, 1 selected
// bootstrap bridge DEPTH_UPDATE, then N selected post-bridge advancing live
// DEPTH_UPDATEs with N = target_live_updates. The gate therefore requires:
//   - the single baseline installs (Installed / AwaitingBridge);
//   - every DEPTH_UPDATE is Applied and leaves the projection Synchronized
//     (the first DEPTH_UPDATE is the synchronization bridge);
//   - zero GapDetected / RejectedWrongState / adapter errors;
//   - final status Synchronized and final accepted update ID equal to the
//     final selected source diff final_update_id;
//   - exactly target_live_updates + 1 applied DEPTH_UPDATEs and exactly
//     target_live_updates + 2 processed events.

#include "operation_observation.hpp"
#include "replay_driver.hpp"

#include "replay_types.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace bmd_projection::m5::phase3 {

struct MediumValidityReport final {
    bool valid{};
    // Stable typed reason such as "bridge-not-applied", "final-status-NeedsResync",
    // "unexpected-applied-count". The first unexpected replay event index is
    // recorded in event_index when the reason identifies a specific event.
    std::string reason;
    std::size_t event_index{};
    std::uint64_t target_live_updates{};
    std::size_t install_events{};
    std::size_t depth_events{};
    std::size_t installed_count{};
    std::size_t applied_count{};
    std::size_t ignored_stale_count{};
    std::size_t ignored_duplicate_count{};
    std::size_t gap_detected_count{};
    std::size_t rejected_wrong_state_count{};
    std::size_t adapter_error_count{};
    std::size_t decimal_error_count{};
    std::size_t other_events_count{};
    std::optional<oracle::CompactInstallResult> first_install;
    std::optional<oracle::CompactDepthResult> first_depth_update;
    oracle::CanonicalStatus final_status{};
    std::optional<std::uint64_t> final_accepted_update_id;
    std::optional<std::uint64_t> last_selected_diff_final_update_id;
};

// Deterministic lifecycle-validity verdict for one compared replay. The caller
// must already have established differential equality (no first divergence).
[[nodiscard]] MediumValidityReport check_medium_validity(const oracle::ReplayOutcome& outcome,
                                                         const replay::ReplayFixture& fixture,
                                                         std::uint64_t target_live_updates);

// Strict minimal reader for the materializer's canonical corpus_provenance.json
// (emitted by json.dumps(sort_keys=True, separators=(",", ":"))). Returns the
// recorded target_live_updates intent, or nullopt when the file is absent,
// malformed, or lacks the required key.
[[nodiscard]] std::optional<std::uint64_t>
read_target_live_updates(const std::filesystem::path& directory);

} // namespace bmd_projection::m5::phase3

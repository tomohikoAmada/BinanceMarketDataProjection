#pragma once

// R4 ReferenceAdapter: independent semantic prediction of the M4 adapter boundary.
//
// This layer owns its own wire-enum mapping tables, quality mapping and ranking,
// snapshot eligibility matrix, gap mapping, and semantic snapshot construction. It
// must not call production adapter helpers to determine expected values. It consumes
// R1 (reference decimal) and R3 (reference projection state) values as value types
// only. Reference enums are mapped to canonical observation names at the observation
// boundary by the reference pipeline side.

#include "reference_decimal.hpp"

#include "replay_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bmd_projection_reference {
class ReferenceProjection;
} // namespace bmd_projection_reference

namespace bmd_projection::m5::reference {

enum class ReferenceVenue : std::uint8_t {
    Binance,
    Unspecified,
};

enum class ReferenceMarket : std::uint8_t {
    Spot,
    UsdMPerpetual,
    Unspecified,
};

enum class ReferencePolicy : std::uint8_t {
    Spot,
    UsdMPerpetual,
};

struct ReferenceNumericSpec final {
    std::uint32_t price_scale{};
    std::uint32_t quantity_scale{};

    friend bool operator==(const ReferenceNumericSpec&, const ReferenceNumericSpec&) = default;
};

// R4's own semantic input model. It intentionally does not contain production
// Protobuf enums or adapter-owned production values.
struct ReferenceAdapterDimensions final {
    ReferenceVenue wire_venue{ReferenceVenue::Binance};
    ReferenceMarket wire_market{ReferenceMarket::Spot};
    std::string wire_symbol;
    std::string expected_symbol;
    ReferencePolicy expected_policy{ReferencePolicy::Spot};
    ReferenceNumericSpec conversion_numeric_spec;
    ReferenceNumericSpec projection_numeric_spec;
    ReferencePolicy projection_policy{ReferencePolicy::Spot};
};

// Independent adapter error categories. Semantically aligned with the M4 adapter
// error domain; the mapping to canonical names happens at the observation boundary.
enum class ReferenceAdapterErrorCode : std::uint8_t {
    UnsupportedVenue,
    UnsupportedMarket,
    UnexpectedStream,
    IdentityMismatch,
    UnsupportedSchemaVersion,
    UnspecifiedEnum,
    UnknownEnumValue,
    InvalidUpdateRange,
    MissingRequiredField,
    InvalidIdentifier,
    InvalidDecimal,
    NegativeQuantity,
    NonPositivePrice,
    ScaleMismatch,
    NumericOverflow,
    InvalidDepthLimit,
    InvalidOrdering,
    UnsupportedProjectionState,
    MissingLastUpdateId,
    InvalidGapContext,
    InvalidHostQualityCombination,
    ProjectionNumericSpecMismatch,
    ProjectionPolicyMismatch,
};

enum class ReferenceAdapterField : std::uint8_t {
    None,
    Venue,
    Market,
    Stream,
    Symbol,
    SchemaVersion,
    Producer,
    ProducerVersion,
    RequestId,
    ConnectionId,
    FirstUpdateId,
    FinalUpdateId,
    PreviousFinalUpdateId,
    BidPrice,
    BidQuantity,
    AskPrice,
    AskQuantity,
    QualityFlag,
    ProjectionPriceScale,
    ProjectionQuantityScale,
    ProjectionPolicy,
    DepthLimit,
    SnapshotSource,
    LastUpdateId,
    CurrentGap,
    GapRecoveryState,
    HostQualityFact,
};

struct ReferenceAdapterError final {
    ReferenceAdapterErrorCode code{};
    ReferenceAdapterField field{};
    std::optional<ReferenceDecimalErrorCode> decimal_error;

    friend bool operator==(const ReferenceAdapterError&, const ReferenceAdapterError&) = default;
};

// Quality flags in the combined output domain: the 13 inbound/host-observable facts
// plus the three Core-derived facts. Rank order and deduplication follow the R4
// quality-ranking table below.
enum class ReferenceQualityFlag : std::uint8_t {
    Duplicate,
    OutOfOrder,
    SequenceGap,
    OrderBookResync,
    SnapshotBridgePending,
    SnapshotTooOld,
    BootstrapBufferOverflow,
    RecoveredTail,
    MalformedPayload,
    ExchangeTimeMissing,
    ReceiveClockDiscontinuity,
    SlowConsumerGap,
    ProducerRestart,
    Overlap,
    IdentityConflict,
    CrossedBook,
};

enum class ReferenceSnapshotSource : std::uint8_t { GatewayLive, RecorderReplay, HistoryReplay };

enum class ReferenceResyncState : std::uint8_t {
    ResyncRequired,
    ResyncInProgress,
    ResyncFailed,
};

enum class ReferenceReasonCode : std::uint8_t { SequenceGapDetected };

struct ReferenceGapDescriptor final {
    std::uint64_t detected_at_utc_ns;
    std::uint64_t previous_sequence;
    std::uint64_t next_sequence;
    ReferenceReasonCode reason_code;
    ReferenceResyncState recovery_state;

    friend bool operator==(const ReferenceGapDescriptor&, const ReferenceGapDescriptor&) = default;
};

// Snapshot level in canonical fixed format (storage-scale fractional digits).
struct ReferenceSnapshotLevel final {
    std::string price;
    std::string quantity;

    friend bool operator==(const ReferenceSnapshotLevel&, const ReferenceSnapshotLevel&) = default;
};

// Predicted semantic snapshot output for a SNAPSHOT_REQUEST.
struct ReferenceSnapshotPrediction final {
    bool synchronized{};
    std::optional<std::uint64_t> last_update_id;
    std::vector<ReferenceSnapshotLevel> bids;
    std::vector<ReferenceSnapshotLevel> asks;
    std::vector<ReferenceQualityFlag> quality_flags;
    std::optional<std::uint32_t> depth_limit;
    std::optional<ReferenceGapDescriptor> gap_descriptor;

    friend bool operator==(const ReferenceSnapshotPrediction&,
                           const ReferenceSnapshotPrediction&) = default;
};

// Successful inbound adaptation prediction: the adapted observed quality.
struct ReferenceInputPrediction final {
    std::vector<ReferenceQualityFlag> observed_quality;

    friend bool operator==(const ReferenceInputPrediction&,
                           const ReferenceInputPrediction&) = default;
};

using ReferenceBaselinePrediction = std::variant<ReferenceInputPrediction, ReferenceAdapterError>;
using ReferenceDepthPrediction = std::variant<ReferenceInputPrediction, ReferenceAdapterError>;
using ReferenceSnapshotResult = std::variant<ReferenceSnapshotPrediction, ReferenceAdapterError>;

class ReferenceAdapter final {
  public:
    ReferenceAdapter(replay::SequencePolicy policy, std::string_view symbol,
                     replay::NumericSpec numeric_spec);
    explicit ReferenceAdapter(ReferenceAdapterDimensions dimensions);

    [[nodiscard]] ReferenceBaselinePrediction
    predict_baseline_input(const replay::InstallBaselineOp& operation,
                           const std::vector<replay::HostQualityFact>& inbound_facts) const;

    [[nodiscard]] ReferenceDepthPrediction
    predict_depth_update_input(const replay::DepthUpdateOp& operation,
                               const std::vector<replay::HostQualityFact>& inbound_facts) const;

    // The projection argument is the R3 reference projection state; R4 reads it
    // through its accessors and never mutates it.
    [[nodiscard]] ReferenceSnapshotResult
    predict_snapshot(const bmd_projection_reference::ReferenceProjection& projection,
                     const replay::SnapshotRequestOp& operation) const;

    [[nodiscard]] static std::vector<ReferenceQualityFlag>
    map_observed_quality(const std::vector<replay::HostQualityFact>& facts);

  private:
    [[nodiscard]] ReferenceBaselinePrediction
    predict_baseline_levels(const std::vector<replay::LevelInput>& bids,
                            const std::vector<replay::LevelInput>& asks,
                            const std::vector<replay::HostQualityFact>& inbound_facts) const;

    [[nodiscard]] ReferenceDepthPrediction
    predict_update_levels(const std::vector<replay::LevelInput>& levels,
                          const std::vector<replay::HostQualityFact>& inbound_facts) const;
    [[nodiscard]] std::optional<ReferenceAdapterError> validate_inbound_identity() const;
    [[nodiscard]] std::optional<ReferenceAdapterError> validate_snapshot_identity() const;
    [[nodiscard]] std::optional<ReferenceAdapterError> validate_binding() const;
    [[nodiscard]] static std::optional<ReferenceAdapterError>
    validate_depth_limit(const std::optional<std::uint32_t>& depth_limit);

    [[nodiscard]] static std::optional<ReferenceAdapterError>
    predict_host_quality(const bmd_projection_reference::ReferenceProjection& projection,
                         const replay::SnapshotRequestOp& operation,
                         std::vector<ReferenceQualityFlag>& flags);

    [[nodiscard]] static std::optional<ReferenceGapDescriptor>
    predict_gap_descriptor(const bmd_projection_reference::ReferenceProjection& projection,
                           const replay::SnapshotRequestOp& operation);

    ReferenceAdapterDimensions dimensions_;
};

} // namespace bmd_projection::m5::reference

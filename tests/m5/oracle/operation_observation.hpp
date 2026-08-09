#pragma once

// M5 differential observation model.
//
// OperationObservation is the fundamental unit of production/reference comparison:
// event identity, ordered pre-business decimal evidence, the observable operation
// result, the post-operation semantic checkpoint, and the optional snapshot semantic
// observation. All values here are canonical (semantic) values produced at the
// observation boundary of each pipeline side. Deterministic integer/unit values and
// stable enum names are used; no addresses, ABI layout, compiler strings, or
// Protobuf bytes ever enter this model.

#include "replay_types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bmd_projection::m5::oracle {

enum class Layer : std::uint8_t {
    R1,
    R2,
    R3,
    R4,
    D,
};

enum class DivergenceCategory : std::uint8_t {
    ParseEvidence,
    OperationResult,
    Checkpoint,
    SnapshotObservation,
    Composition,
};

enum class CanonicalDisposition : std::uint8_t {
    Installed,
    Applied,
    IgnoredStale,
    IgnoredDuplicate,
    GapDetected,
    RejectedWrongState,
};

enum class CanonicalStatus : std::uint8_t {
    AwaitingBaseline,
    AwaitingBridge,
    Synchronized,
    NeedsResync,
};

enum class CanonicalGapReason : std::uint8_t {
    SpotBootstrapForwardGap,
    SpotLiveForwardGap,
    FuturesBootstrapRangeMiss,
    FuturesMissingPreviousFinal,
    FuturesPreviousFinalMismatch,
};

enum class CanonicalPolicy : std::uint8_t { Spot, UsdMPerpetual };

enum class CanonicalDecimalError : std::uint8_t {
    Empty,
    InvalidSyntax,
    SignNotAllowed,
    LeadingZero,
    MissingFractionDigits,
    ZeroNotAllowed,
    InexactScale,
    Overflow,
};

enum class CanonicalBookSide : std::uint8_t { Bid, Ask };

enum class CanonicalDecimalRole : std::uint8_t { Price, Quantity };

struct CanonicalDecimalValue final {
    std::int64_t units{};
    std::uint32_t storage_scale{};
    std::size_t source_fraction_digits{};

    friend bool operator==(const CanonicalDecimalValue&, const CanonicalDecimalValue&) = default;
};

struct CanonicalDecimalFailure final {
    CanonicalDecimalError category{};
    std::size_t offset{};

    friend bool operator==(const CanonicalDecimalFailure&,
                           const CanonicalDecimalFailure&) = default;
};

using CanonicalDecimalResult = std::variant<CanonicalDecimalValue, CanonicalDecimalFailure>;

// Pre-business M1 evidence for one replay decimal token. level_position is the
// token's position in its normalized input collection; side and role retain its
// semantic identity even when M3 later ignores or rejects the operation.
struct CanonicalDecimalObservation final {
    CanonicalBookSide side{};
    std::size_t level_position{};
    CanonicalDecimalRole role{};
    CanonicalDecimalResult result;

    friend bool operator==(const CanonicalDecimalObservation&,
                           const CanonicalDecimalObservation&) = default;
};

enum class CanonicalAdapterCode : std::uint8_t {
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
    ContractsVersionMismatch,
    ProjectionNumericSpecMismatch,
    ProjectionPolicyMismatch,
};

enum class CanonicalAdapterField : std::uint8_t {
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

enum class CanonicalQualityFlag : std::uint8_t {
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

enum class CanonicalSnapshotSource : std::uint8_t { GatewayLive, RecorderReplay, HistoryReplay };

enum class CanonicalResyncState : std::uint8_t {
    ResyncRequired,
    ResyncInProgress,
    ResyncFailed,
};

enum class CanonicalReasonCode : std::uint8_t { SequenceGapDetected };

struct CanonicalLevel final {
    std::int64_t price{};
    std::int64_t quantity{};

    friend bool operator==(const CanonicalLevel&, const CanonicalLevel&) = default;
};

struct CanonicalGapEvidence final {
    std::uint64_t last_accepted_final{};
    std::uint64_t first_update_id{};
    std::uint64_t final_update_id{};
    std::optional<std::uint64_t> previous_final;
    CanonicalGapReason reason{};
    CanonicalPolicy policy{};

    friend bool operator==(const CanonicalGapEvidence&, const CanonicalGapEvidence&) = default;
};

// Core-only decimal parse failure. Only failures are observed as results; a fully
// parsed event proceeds to its install/apply result.
struct DecimalErrorOutcome final {
    CanonicalDecimalError category;

    friend bool operator==(const DecimalErrorOutcome&, const DecimalErrorOutcome&) = default;
};

struct InstallOutcome final {
    CanonicalDisposition disposition{};
    CanonicalStatus status_after{};
    std::optional<std::uint64_t> last_update_id_after;

    friend bool operator==(const InstallOutcome&, const InstallOutcome&) = default;
};

struct ApplyOutcome final {
    CanonicalDisposition disposition{};
    CanonicalStatus status_after{};
    std::optional<std::uint64_t> last_update_id_after;
    std::optional<CanonicalGapEvidence> gap;

    friend bool operator==(const ApplyOutcome&, const ApplyOutcome&) = default;
};

struct AdapterErrorOutcome final {
    CanonicalAdapterCode code{};
    CanonicalAdapterField field{};
    std::optional<CanonicalDecimalError> decimal_error;

    friend bool operator==(const AdapterErrorOutcome&, const AdapterErrorOutcome&) = default;
};

// Successful adapter-mode inbound adaptation: the underlying M3 result plus the
// adapted inbound wire quality (observed quality) in canonical rank order.
struct AdapterSuccessOutcome final {
    std::variant<InstallOutcome, ApplyOutcome> core_result;
    std::vector<CanonicalQualityFlag> observed_quality;

    friend bool operator==(const AdapterSuccessOutcome&, const AdapterSuccessOutcome&) = default;
};

// RESET is a real operation; its observable result is a stable success kind.
struct ResetOutcome final {
    friend bool operator==(const ResetOutcome&, const ResetOutcome&) = default;
};

// MALFORMED_RANGE must fail domain construction and never reach apply. invalid_as_intended
// is true when construction failed (first > final) and false when the range was valid,
// which contradicts the event's negative intent; both pipeline sides must agree.
struct RangeOutcome final {
    bool invalid_as_intended;

    friend bool operator==(const RangeOutcome&, const RangeOutcome&) = default;
};

// ADAPTER_METADATA carries no business result: it is context for the following
// INSTALL_BASELINE or DEPTH_UPDATE and is observable only through that event's result.
struct MetadataOutcome final {
    friend bool operator==(const MetadataOutcome&, const MetadataOutcome&) = default;
};

// SNAPSHOT_REQUEST in Core-only mode: the snapshot boundary (M4 adapter + R4) is not
// linked, so no snapshot semantics are produced. The checkpoint is still observed.
struct SnapshotNotProducedOutcome final {
    friend bool operator==(const SnapshotNotProducedOutcome&,
                           const SnapshotNotProducedOutcome&) = default;
};

// A snapshot level pair in canonical fixed format (storage-scale fractional digits).
struct SnapshotLevel final {
    std::string price;
    std::string quantity;

    friend bool operator==(const SnapshotLevel&, const SnapshotLevel&) = default;
};

struct GapDescriptorObservation final {
    std::uint64_t detected_at_utc_ns{};
    std::uint64_t previous_sequence{};
    std::uint64_t next_sequence{};
    CanonicalReasonCode reason_code{};
    CanonicalResyncState recovery_state{};

    friend bool operator==(const GapDescriptorObservation&,
                           const GapDescriptorObservation&) = default;
};

// Semantic snapshot output. All fields are stable observable semantics; Protobuf
// byte equality is deliberately not compared.
struct SnapshotOutcome final {
    CanonicalPolicy policy{};
    std::string symbol;
    std::string producer;
    std::string producer_version;
    CanonicalSnapshotSource source{};
    std::uint64_t generated_time_utc_ns{};
    std::optional<std::uint64_t> generated_monotonic_ns;
    std::optional<std::uint64_t> last_update_id;
    bool synchronized{};
    std::vector<SnapshotLevel> bids;
    std::vector<SnapshotLevel> asks;
    std::vector<CanonicalQualityFlag> quality_flags;
    std::optional<std::uint32_t> depth_limit;
    std::optional<GapDescriptorObservation> gap_descriptor;

    friend bool operator==(const SnapshotOutcome&, const SnapshotOutcome&) = default;
};

using OperationResultValue =
    std::variant<DecimalErrorOutcome, InstallOutcome, ApplyOutcome, AdapterErrorOutcome,
                 AdapterSuccessOutcome, SnapshotOutcome, SnapshotNotProducedOutcome, ResetOutcome,
                 RangeOutcome, MetadataOutcome>;

struct OperationResult final {
    OperationResultValue value;

    friend bool operator==(const OperationResult&, const OperationResult&) = default;
};

struct SemanticCheckpoint final {
    CanonicalStatus status{};
    std::optional<std::uint64_t> last_update_id;
    std::optional<CanonicalGapEvidence> last_gap;
    bool synchronized_visible{};
    std::vector<CanonicalLevel> bids;
    std::vector<CanonicalLevel> asks;
    std::uint32_t price_scale{};
    std::uint32_t quantity_scale{};

    friend bool operator==(const SemanticCheckpoint&, const SemanticCheckpoint&) = default;
};

struct OperationObservation final {
    std::size_t event_index{};
    replay::EventKind event_kind{};
    std::vector<CanonicalDecimalObservation> decimal_observations;
    OperationResult result;
    SemanticCheckpoint checkpoint;
    std::optional<SnapshotOutcome> snapshot;

    friend bool operator==(const OperationObservation&, const OperationObservation&) = default;
};

} // namespace bmd_projection::m5::oracle

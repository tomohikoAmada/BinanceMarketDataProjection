#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bmd_projection::m5::replay {

inline constexpr std::uint32_t kReplaySchemaVersion = 1;
inline constexpr std::string_view kReplaySchemaName = "REPLAY_V1";

enum class ErrorCategory : std::uint8_t {
    IoFailure,
    InvalidCanonicalBytes,
    ReplaySyntax,
    UnsupportedSchema,
    ManifestParse,
    IdentityMismatch,
    EventCountMismatch,
    InvalidMetadata,
};

struct ParseError final {
    ErrorCategory category;
    std::size_t line_number{};
    std::size_t event_index{};
    std::size_t token_index{};
    std::string message;

    friend bool operator==(const ParseError&, const ParseError&) = default;
};

template <typename T> using Result = std::variant<T, ParseError>;

enum class Market : std::uint8_t { Spot, UsdMPerpetual };
enum class SequencePolicy : std::uint8_t { Spot, UsdMPerpetual };
enum class Side : std::uint8_t { Bid, Ask };

enum class EventKind : std::uint8_t {
    InstallBaseline,
    DepthUpdate,
    Rebaseline,
    Reset,
    SnapshotRequest,
    AdapterMetadata,
    MalformedRange,
};

enum class HostQualityFact : std::uint8_t {
    Duplicate,
    OutOfOrder,
    OrderBookResync,
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
};

enum class SnapshotOrigin : std::uint8_t { GatewayLive, RecorderReplay, HistoryReplay };
enum class GapRecoveryState : std::uint8_t {
    Synchronized,
    ResyncRequired,
    ResyncInProgress,
    Recovered,
    ResyncFailed,
};

struct NumericSpec final {
    std::uint32_t price_scale{};
    std::uint32_t quantity_scale{};

    friend bool operator==(const NumericSpec&, const NumericSpec&) = default;
};

struct FixtureIdentity final {
    std::uint32_t schema_version{};
    std::string replay_log_sha256;
    std::string fixture_id;
    Market market{};
    std::string symbol;
    NumericSpec numeric_spec;
    SequencePolicy sequence_policy{};

    friend bool operator==(const FixtureIdentity&, const FixtureIdentity&) = default;
};

struct SourceLocation final {
    std::size_t event_index{};
    std::size_t line_number{};
    std::string canonical_line;

    friend bool operator==(const SourceLocation&, const SourceLocation&) = default;
};

struct LevelInput final {
    Side side{};
    std::string price;
    std::string quantity;

    friend bool operator==(const LevelInput&, const LevelInput&) = default;
};

struct InstallBaselineOp final {
    SourceLocation source;
    std::uint64_t last_update_id{};
    std::vector<LevelInput> bids;
    std::vector<LevelInput> asks;

    friend bool operator==(const InstallBaselineOp&, const InstallBaselineOp&) = default;
};

struct DepthUpdateOp final {
    SourceLocation source;
    std::uint64_t first_update_id{};
    std::uint64_t final_update_id{};
    std::optional<std::uint64_t> previous_final;
    std::vector<LevelInput> levels;

    friend bool operator==(const DepthUpdateOp&, const DepthUpdateOp&) = default;
};

struct RebaselineOp final {
    SourceLocation source;
    std::uint64_t last_update_id{};
    std::vector<LevelInput> bids;
    std::vector<LevelInput> asks;

    friend bool operator==(const RebaselineOp&, const RebaselineOp&) = default;
};

struct ResetOp final {
    SourceLocation source;

    friend bool operator==(const ResetOp&, const ResetOp&) = default;
};

struct SnapshotRequestOp final {
    SourceLocation source;
    std::optional<std::uint32_t> depth_limit;
    std::vector<HostQualityFact> host_quality_facts;
    std::string snapshot_id;
    std::string producer;
    std::string producer_version;
    SnapshotOrigin source_origin{};
    std::uint64_t generated_time_utc_ns{};
    std::optional<std::uint64_t> generated_monotonic_ns;
    std::optional<std::pair<std::uint64_t, GapRecoveryState>> current_gap;

    friend bool operator==(const SnapshotRequestOp&, const SnapshotRequestOp&) = default;
};

struct AdapterMetadataOp final {
    SourceLocation source;
    std::vector<HostQualityFact> observed_quality;

    friend bool operator==(const AdapterMetadataOp&, const AdapterMetadataOp&) = default;
};

struct MalformedRangeOp final {
    SourceLocation source;
    std::uint64_t first_update_id{};
    std::uint64_t final_update_id{};

    friend bool operator==(const MalformedRangeOp&, const MalformedRangeOp&) = default;
};

using Operation = std::variant<InstallBaselineOp, DepthUpdateOp, RebaselineOp, ResetOp,
                               SnapshotRequestOp, AdapterMetadataOp, MalformedRangeOp>;

struct ReplayHeader final {
    std::uint32_t schema_version{};
    Market market{};
    std::string symbol;
    NumericSpec numeric_spec;
    SequencePolicy sequence_policy{};
    std::string fixture_id;
    std::vector<std::pair<std::string, std::string>> provenance;

    friend bool operator==(const ReplayHeader&, const ReplayHeader&) = default;
};

struct NormalizedReplay final {
    ReplayHeader header;
    std::vector<Operation> operations;

    friend bool operator==(const NormalizedReplay&, const NormalizedReplay&) = default;
};

struct ReplayManifest final {
    FixtureIdentity identity;
    std::size_t event_count{};
    std::vector<std::pair<std::string, std::string>> provenance;

    friend bool operator==(const ReplayManifest&, const ReplayManifest&) = default;
};

struct ReplayFixture final {
    NormalizedReplay replay;
    ReplayManifest manifest;
    FixtureIdentity identity;
    std::string canonical_log_sha256;

    friend bool operator==(const ReplayFixture&, const ReplayFixture&) = default;
};

struct ImmutableSourceArchive final {
    std::string root_identity;
    std::string recorder_commit;
    std::string recorder_wheel_sha256;
    std::string recorder_config_sha256;
    std::string run_identity;
    std::vector<std::pair<std::string, std::string>> raw_chunk_sha256;
    std::string market;
    std::string symbol;
    std::string source_interval;

    friend bool operator==(const ImmutableSourceArchive&, const ImmutableSourceArchive&) = default;
};

struct MaterializerContract final {
    enum class MarketRule : std::uint8_t { Spot, UsdMPerpetual };

    MarketRule market_rule{};
    std::string snapshot_identity;
    std::string buffer_window;
    std::string discard_rule;
    std::string first_bridge_rule;
    std::string post_bridge_rule;
    std::string failure_rule;
    std::string acquisition_boundary;

    friend bool operator==(const MaterializerContract&, const MaterializerContract&) = default;
};

enum class SpotBootstrapOutcome : std::uint8_t {
    Stale,
    NonAdvancingDuplicate,
    BridgeCandidate,
    ForwardGap,
};

[[nodiscard]] SpotBootstrapOutcome classify_spot_bootstrap(std::uint64_t snapshot_last_update_id,
                                                           std::uint64_t first_update_id,
                                                           std::uint64_t final_update_id);

[[nodiscard]] MaterializerContract spot_materializer_contract();
[[nodiscard]] MaterializerContract usdm_materializer_contract();
[[nodiscard]] Result<std::monostate> validate_source_archive(const ImmutableSourceArchive& source);

} // namespace bmd_projection::m5::replay

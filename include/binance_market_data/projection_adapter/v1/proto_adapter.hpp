#pragma once

#include <binance_market_data/market/v1/market_events.pb.h>
#include <binance_market_data/projection/v1/numeric/decimal_error.hpp>
#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>
#include <binance_market_data/projection/v1/snapshots.pb.h>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace binance_market_data::projection_adapter::v1 {

enum class AdapterErrorCode : std::uint8_t {
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

enum class AdapterField : std::uint8_t {
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

struct AdapterError final {
    AdapterErrorCode code;
    AdapterField field;
    std::optional<projection::v1::DecimalError> decimal_error;
    std::optional<std::int32_t> raw_enum_value;

    friend constexpr bool operator==(const AdapterError&, const AdapterError&) noexcept = default;
};

template <typename T> using AdapterResult = std::variant<T, AdapterError>;

struct ExpectedIdentity final {
    std::string symbol;
    projection::v1::SequencePolicyKind policy;
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

struct AdaptedMetadata final {
    std::vector<HostQualityFact> observed_quality;

    friend bool operator==(const AdaptedMetadata&, const AdaptedMetadata&) = default;
};

namespace detail {
struct AdapterFactory;
}

class AdaptedBookBaseline final {
  public:
    AdaptedBookBaseline(AdaptedBookBaseline&&) noexcept;
    AdaptedBookBaseline& operator=(AdaptedBookBaseline&&) noexcept;
    AdaptedBookBaseline(const AdaptedBookBaseline&) = delete;
    AdaptedBookBaseline& operator=(const AdaptedBookBaseline&) = delete;

    [[nodiscard]] AdapterResult<projection::v1::InstallResult>
    install_into(projection::v1::BookProjection& target) const&;
    AdapterResult<projection::v1::InstallResult>
    install_into(projection::v1::BookProjection& target) const&& = delete;
    AdapterResult<projection::v1::InstallResult>
    install_into(projection::v1::BookProjection&& target) const& = delete;

    [[nodiscard]] const AdaptedMetadata& metadata() const& noexcept;
    const AdaptedMetadata& metadata() const&& = delete;

  private:
    friend struct detail::AdapterFactory;
    AdaptedBookBaseline(projection::v1::NumericSpec numeric_spec,
                        projection::v1::SequencePolicyKind policy,
                        projection::v1::UpdateId last_update_id,
                        std::vector<projection::v1::BookLevel> bids,
                        std::vector<projection::v1::BookLevel> asks,
                        AdaptedMetadata metadata) noexcept;
    [[nodiscard]] projection::v1::BookBaseline view_unchecked() const& noexcept;

    projection::v1::NumericSpec numeric_spec_;
    projection::v1::SequencePolicyKind policy_;
    projection::v1::UpdateId last_update_id_;
    std::vector<projection::v1::BookLevel> bids_;
    std::vector<projection::v1::BookLevel> asks_;
    AdaptedMetadata metadata_;
};

class AdaptedDepthBatch final {
  public:
    AdaptedDepthBatch(AdaptedDepthBatch&&) noexcept;
    AdaptedDepthBatch& operator=(AdaptedDepthBatch&&) noexcept;
    AdaptedDepthBatch(const AdaptedDepthBatch&) = delete;
    AdaptedDepthBatch& operator=(const AdaptedDepthBatch&) = delete;

    [[nodiscard]] AdapterResult<projection::v1::ApplyResult>
    apply_to(projection::v1::BookProjection& target) const&;
    AdapterResult<projection::v1::ApplyResult>
    apply_to(projection::v1::BookProjection& target) const&& = delete;
    AdapterResult<projection::v1::ApplyResult>
    apply_to(projection::v1::BookProjection&& target) const& = delete;

    [[nodiscard]] const AdaptedMetadata& metadata() const& noexcept;
    const AdaptedMetadata& metadata() const&& = delete;

  private:
    friend struct detail::AdapterFactory;
    AdaptedDepthBatch(projection::v1::NumericSpec numeric_spec,
                      projection::v1::SequencePolicyKind policy, projection::v1::UpdateRange range,
                      std::optional<projection::v1::UpdateId> previous_final,
                      std::vector<projection::v1::LevelUpdate> levels,
                      AdaptedMetadata metadata) noexcept;
    [[nodiscard]] projection::v1::DepthBatch view_unchecked() const& noexcept;

    projection::v1::NumericSpec numeric_spec_;
    projection::v1::SequencePolicyKind policy_;
    projection::v1::UpdateRange range_;
    std::optional<projection::v1::UpdateId> previous_final_;
    std::vector<projection::v1::LevelUpdate> levels_;
    AdaptedMetadata metadata_;
};

enum class SnapshotOrigin : std::uint8_t {
    GatewayLive,
    RecorderReplay,
    HistoryReplay,
};

enum class GapRecoveryState : std::uint8_t {
    Synchronized,
    ResyncRequired,
    ResyncInProgress,
    Recovered,
    ResyncFailed,
};

class DepthLimit final {
  public:
    [[nodiscard]] static AdapterResult<DepthLimit> create(std::int64_t value) noexcept;
    [[nodiscard]] std::int32_t value() const noexcept;

    friend constexpr bool operator==(DepthLimit, DepthLimit) noexcept = default;

  private:
    explicit constexpr DepthLimit(std::int32_t value) noexcept : value_{value} {}
    std::int32_t value_;
};

struct CurrentGapContext final {
    std::uint64_t detected_at_utc_ns;
    GapRecoveryState recovery_state;
};

struct SnapshotContext final {
    ExpectedIdentity identity;
    std::string producer;
    std::string producer_version;
    SnapshotOrigin source;
    std::uint64_t generated_time_utc_ns;
    std::optional<std::uint64_t> generated_monotonic_ns;
    std::optional<CurrentGapContext> current_gap;
};

struct SnapshotOptions final {
    std::optional<DepthLimit> depth_limit;
    std::vector<HostQualityFact> host_quality_facts;
};

[[nodiscard]] AdapterResult<AdaptedBookBaseline>
adapt_exchange_depth_snapshot(const ::binance_market_data::market::v1::ExchangeDepthSnapshot& wire,
                              projection::v1::NumericSpec numeric_spec,
                              const ExpectedIdentity& expected);

[[nodiscard]] AdapterResult<AdaptedDepthBatch>
adapt_depth_update(const ::binance_market_data::market::v1::DepthUpdate& wire,
                   projection::v1::NumericSpec numeric_spec, const ExpectedIdentity& expected);

[[nodiscard]] AdapterResult<::binance_market_data::projection::v1::LocalOrderBookSnapshot>
make_local_order_book_snapshot(const projection::v1::BookProjection& projection,
                               const SnapshotContext& context, const SnapshotOptions& options);

} // namespace binance_market_data::projection_adapter::v1

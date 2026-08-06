#pragma once

#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <compare>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>

namespace binance_market_data::projection::v1 {

class UpdateId final {
  public:
    explicit constexpr UpdateId(std::uint64_t value) noexcept : value_{value} {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }

    friend constexpr auto operator<=>(UpdateId, UpdateId) noexcept = default;

  private:
    std::uint64_t value_;
};

class UpdateRange final {
  public:
    [[nodiscard]] static constexpr std::optional<UpdateRange> try_create(UpdateId first,
                                                                         UpdateId final) noexcept {
        if (first > final) {
            return std::nullopt;
        }
        return UpdateRange{first, final};
    }

    [[nodiscard]] constexpr UpdateId first() const noexcept { return first_; }

    [[nodiscard]] constexpr UpdateId final() const noexcept { return final_; }

    friend constexpr bool operator==(const UpdateRange&, const UpdateRange&) noexcept = default;

  private:
    constexpr UpdateRange(UpdateId first, UpdateId final) noexcept : first_{first}, final_{final} {}

    UpdateId first_;
    UpdateId final_;
};

enum class SequencePolicyKind : std::uint8_t {
    Spot,
    UsdMPerpetual,
};

enum class ProjectionStatus : std::uint8_t {
    AwaitingBaseline,
    AwaitingBridge,
    Synchronized,
    NeedsResync,
};

enum class ApplyDisposition : std::uint8_t {
    Applied,
    IgnoredStale,
    IgnoredDuplicate,
    GapDetected,
    RejectedWrongState,
};

enum class GapReason : std::uint8_t {
    SpotBootstrapForwardGap,
    SpotLiveForwardGap,
    FuturesBootstrapRangeMiss,
    FuturesMissingPreviousFinal,
    FuturesPreviousFinalMismatch,
};

struct GapInfo final {
    UpdateId last_accepted_final;
    UpdateRange incoming_range;
    std::optional<UpdateId> incoming_previous_final;
    GapReason reason;
    SequencePolicyKind policy;

    friend constexpr bool operator==(const GapInfo&, const GapInfo&) noexcept = default;
};

struct DepthBatch final {
    UpdateRange range;
    std::optional<UpdateId> previous_final;
    std::span<const LevelUpdate> levels;
};

struct BookBaseline final {
    UpdateId last_update_id;
    std::span<const BookLevel> bids;
    std::span<const BookLevel> asks;
};

enum class InstallDisposition : std::uint8_t {
    Installed,
    RejectedWrongState,
};

struct InstallResult final {
    InstallDisposition disposition;
    ProjectionStatus status_after;
    std::optional<UpdateId> last_update_id_after;

    friend constexpr bool operator==(const InstallResult&, const InstallResult&) noexcept = default;
};

struct ApplyResult final {
    ApplyDisposition disposition;
    ProjectionStatus status_after;
    std::optional<UpdateId> last_update_id_after;
    std::optional<GapInfo> gap;

    friend constexpr bool operator==(const ApplyResult&, const ApplyResult&) noexcept = default;
};

class BookProjection final {
  public:
    explicit BookProjection(NumericSpec numeric_spec, SequencePolicyKind policy);
    ~BookProjection();

    BookProjection(BookProjection&&) noexcept;
    BookProjection& operator=(BookProjection&&) noexcept;
    BookProjection(const BookProjection&) = delete;
    BookProjection& operator=(const BookProjection&) = delete;

    [[nodiscard]] NumericSpec numeric_spec() const noexcept;
    [[nodiscard]] SequencePolicyKind policy() const noexcept;
    [[nodiscard]] ProjectionStatus status() const noexcept;
    [[nodiscard]] std::optional<UpdateId> last_update_id() const noexcept;
    [[nodiscard]] std::optional<GapInfo> last_gap() const noexcept;

    [[nodiscard]] InstallResult install_baseline(BookBaseline baseline);
    [[nodiscard]] ApplyResult apply(DepthBatch batch);
    void reset() noexcept;

    [[nodiscard]] std::optional<std::reference_wrapper<const OrderBook>>
    synchronized_book() const noexcept;
    [[nodiscard]] const OrderBook& diagnostic_book() const noexcept;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace binance_market_data::projection::v1

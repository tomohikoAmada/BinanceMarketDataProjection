#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace bmd_projection_reference {

// The model's state guards establish the optional-value preconditions independently of production.
// NOLINTBEGIN(bugprone-unchecked-optional-access)

enum class Policy : std::uint8_t {
    Spot,
    UsdM,
};

enum class Status : std::uint8_t {
    AwaitingBaseline,
    AwaitingBridge,
    Synchronized,
    NeedsResync,
};

enum class Disposition : std::uint8_t {
    Applied,
    IgnoredStale,
    IgnoredDuplicate,
    GapDetected,
    RejectedWrongState,
};

enum class InstallDisposition : std::uint8_t {
    Installed,
    RejectedWrongState,
};

enum class GapReason : std::uint8_t {
    SpotBootstrapForwardGap,
    SpotLiveForwardGap,
    FuturesBootstrapRangeMiss,
    FuturesMissingPreviousFinal,
    FuturesPreviousFinalMismatch,
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct RawLevel final {
    bool bid;
    std::int64_t price;
    std::int64_t quantity;

    friend bool operator==(const RawLevel&, const RawLevel&) = default;
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct Gap final {
    std::uint64_t last;
    std::uint64_t first;
    std::uint64_t final;
    std::optional<std::uint64_t> previous;
    GapReason reason;
    Policy policy;

    friend bool operator==(const Gap&, const Gap&) = default;
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct Result final {
    Disposition disposition;
    Status status;
    std::optional<std::uint64_t> last;
    std::optional<Gap> gap;

    friend bool operator==(const Result&, const Result&) = default;
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct InstallResult final {
    InstallDisposition disposition;
    Status status;
    std::optional<std::uint64_t> last;

    friend bool operator==(const InstallResult&, const InstallResult&) = default;
};

class ReferenceProjection final {
  public:
    explicit ReferenceProjection(Policy policy) : policy_{policy} {}

    [[nodiscard]] InstallResult install(std::uint64_t last, const std::vector<RawLevel>& levels) {
        if (status_ == Status::Synchronized) {
            return {InstallDisposition::RejectedWrongState, status_, last_};
        }
        bids_.clear();
        asks_.clear();
        for (const auto& level : levels) {
            apply_level(level);
        }
        last_ = last;
        status_ = Status::AwaitingBridge;
        return {InstallDisposition::Installed, status_, last_};
    }

    [[nodiscard]] Result apply(std::uint64_t first, std::uint64_t final,
                               std::optional<std::uint64_t> previous,
                               const std::vector<RawLevel>& levels) {
        if (status_ == Status::AwaitingBaseline || status_ == Status::NeedsResync) {
            return simple(Disposition::RejectedWrongState);
        }

        const auto current = last_.value();
        if (final < current) {
            return simple(Disposition::IgnoredStale);
        }

        if (policy_ == Policy::Spot) {
            if (final == current) {
                return simple(Disposition::IgnoredDuplicate);
            }
            if (status_ == Status::AwaitingBridge) {
                if (first > current) {
                    return gap(first, final, previous, GapReason::SpotBootstrapForwardGap);
                }
            } else {
                const bool starts_at_successor =
                    current != UINT64_MAX && first == current + std::uint64_t{1};
                if (first > current && !starts_at_successor) {
                    return gap(first, final, previous, GapReason::SpotLiveForwardGap);
                }
            }
        } else if (status_ == Status::AwaitingBridge) {
            if (first > current) {
                return gap(first, final, previous, GapReason::FuturesBootstrapRangeMiss);
            }
            if (!previous.has_value()) {
                return gap(first, final, previous, GapReason::FuturesMissingPreviousFinal);
            }
        } else {
            if (final == current) {
                return simple(Disposition::IgnoredDuplicate);
            }
            if (!previous.has_value()) {
                return gap(first, final, previous, GapReason::FuturesMissingPreviousFinal);
            }
            if (previous.value() != current) {
                return gap(first, final, previous, GapReason::FuturesPreviousFinalMismatch);
            }
        }

        for (const auto& level : levels) {
            apply_level(level);
        }
        if (final > current) {
            last_ = final;
        }
        status_ = Status::Synchronized;
        return simple(Disposition::Applied);
    }

    void reset() noexcept {
        status_ = Status::AwaitingBaseline;
        last_.reset();
        last_gap_.reset();
        bids_.clear();
        asks_.clear();
    }

    [[nodiscard]] Policy policy() const noexcept { return policy_; }
    [[nodiscard]] Status status() const noexcept { return status_; }
    [[nodiscard]] std::optional<std::uint64_t> last() const noexcept { return last_; }
    [[nodiscard]] std::optional<Gap> last_gap() const noexcept { return last_gap_; }
    [[nodiscard]] bool synchronized_visible() const noexcept {
        return status_ == Status::Synchronized;
    }
    [[nodiscard]] const std::vector<RawLevel>& bids() const noexcept { return bids_; }
    [[nodiscard]] const std::vector<RawLevel>& asks() const noexcept { return asks_; }

  private:
    using LevelVector = std::vector<RawLevel>;

    void apply_level(const RawLevel& level) {
        auto& levels = level.bid ? bids_ : asks_;
        const auto found = std::find_if(levels.begin(), levels.end(), [&](const RawLevel& current) {
            return current.price == level.price;
        });
        if (level.quantity == 0) {
            if (found != levels.end()) {
                levels.erase(found);
            }
        } else if (found != levels.end()) {
            found->quantity = level.quantity;
        } else {
            levels.push_back(level);
        }
        std::sort(levels.begin(), levels.end(), [&](const RawLevel& lhs, const RawLevel& rhs) {
            return level.bid ? lhs.price > rhs.price : lhs.price < rhs.price;
        });
    }

    [[nodiscard]] Result simple(Disposition disposition) const noexcept {
        return {disposition, status_, last_, std::nullopt};
    }

    [[nodiscard]] Result gap(std::uint64_t first, std::uint64_t final,
                             std::optional<std::uint64_t> previous, GapReason reason) noexcept {
        Gap evidence{last_.value(), first, final, previous, reason, policy_};
        last_gap_ = evidence;
        status_ = Status::NeedsResync;
        return {Disposition::GapDetected, status_, last_, evidence};
    }

    Policy policy_;
    Status status_{Status::AwaitingBaseline};
    std::optional<std::uint64_t> last_;
    std::optional<Gap> last_gap_;
    LevelVector bids_;
    LevelVector asks_;
};

// NOLINTEND(bugprone-unchecked-optional-access)

} // namespace bmd_projection_reference

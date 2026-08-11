#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <limits>
#include <utility>

namespace binance_market_data::projection::v1 {

namespace {

enum class SequenceDecision : std::uint8_t {
    Apply,
    Stale,
    Duplicate,
    Gap,
};

struct Classification final {
    SequenceDecision decision{SequenceDecision::Gap};
    std::optional<GapReason> gap_reason;
};

[[nodiscard]] constexpr Classification apply_decision() noexcept {
    return {SequenceDecision::Apply, std::nullopt};
}

[[nodiscard]] constexpr Classification stale_decision() noexcept {
    return {SequenceDecision::Stale, std::nullopt};
}

[[nodiscard]] constexpr Classification duplicate_decision() noexcept {
    return {SequenceDecision::Duplicate, std::nullopt};
}

[[nodiscard]] constexpr Classification gap_decision(GapReason reason) noexcept {
    return {SequenceDecision::Gap, reason};
}

[[nodiscard]] constexpr bool covers_successor(UpdateId current, UpdateId first) noexcept {
    if (first <= current) {
        return true;
    }
    const auto current_value = current.value();
    return current_value != std::numeric_limits<std::uint64_t>::max() &&
           first.value() == current_value + 1U;
}

[[nodiscard]] constexpr Classification classify_spot_bootstrap(UpdateId current,
                                                               UpdateRange range) noexcept {
    if (range.final() < current) {
        return stale_decision();
    }
    if (range.final() == current) {
        return duplicate_decision();
    }
    if (covers_successor(current, range.first())) {
        return apply_decision();
    }
    return gap_decision(GapReason::SpotBootstrapForwardGap);
}

[[nodiscard]] constexpr Classification classify_spot_live(UpdateId current,
                                                          UpdateRange range) noexcept {
    if (range.final() < current) {
        return stale_decision();
    }
    if (range.final() == current) {
        return duplicate_decision();
    }
    if (covers_successor(current, range.first())) {
        return apply_decision();
    }
    return gap_decision(GapReason::SpotLiveForwardGap);
}

[[nodiscard]] constexpr Classification
classify_usdm_bootstrap(UpdateId current, UpdateRange range,
                        const std::optional<UpdateId>& previous_final) noexcept {
    if (range.final() < current) {
        return stale_decision();
    }
    if (range.first() > current) {
        return gap_decision(GapReason::FuturesBootstrapRangeMiss);
    }
    if (!previous_final.has_value()) {
        return gap_decision(GapReason::FuturesMissingPreviousFinal);
    }
    return apply_decision();
}

[[nodiscard]] constexpr Classification
classify_usdm_live(UpdateId current, UpdateRange range,
                   const std::optional<UpdateId>& previous_final) noexcept {
    if (range.final() < current) {
        return stale_decision();
    }
    if (range.final() == current) {
        return duplicate_decision();
    }
    if (!previous_final.has_value()) {
        return gap_decision(GapReason::FuturesMissingPreviousFinal);
    }
    if (previous_final.value() != current) {
        return gap_decision(GapReason::FuturesPreviousFinalMismatch);
    }
    return apply_decision();
}

} // namespace

class BookProjection::Impl final {
  public:
    explicit Impl(NumericSpec numeric_spec, SequencePolicyKind policy)
        : numeric_spec_{numeric_spec}, policy_{policy}, book_{numeric_spec} {}

    [[nodiscard]] NumericSpec numeric_spec() const noexcept { return numeric_spec_; }

    [[nodiscard]] SequencePolicyKind policy() const noexcept { return policy_; }

    [[nodiscard]] ProjectionStatus status() const noexcept { return status_; }

    [[nodiscard]] std::optional<UpdateId> last_update_id() const noexcept {
        return last_update_id_;
    }

    [[nodiscard]] std::optional<GapInfo> last_gap() const noexcept { return last_gap_; }

    [[nodiscard]] InstallResult install_baseline(BookBaseline baseline) {
        if (status_ == ProjectionStatus::Synchronized) {
            return install_result(InstallDisposition::RejectedWrongState);
        }

        book_.replace_all(baseline.bids, baseline.asks);
        last_update_id_ = baseline.last_update_id;
        status_ = ProjectionStatus::AwaitingBridge;
        return install_result(InstallDisposition::Installed);
    }

    [[nodiscard]] ApplyResult apply(DepthBatch batch) {
        if (status_ == ProjectionStatus::AwaitingBaseline ||
            status_ == ProjectionStatus::NeedsResync) {
            return apply_result(ApplyDisposition::RejectedWrongState);
        }

        const auto current = last_update_id_.value();
        const auto classification = classify(current, batch);
        switch (classification.decision) {
        case SequenceDecision::Stale:
            return apply_result(ApplyDisposition::IgnoredStale);
        case SequenceDecision::Duplicate:
            return apply_result(ApplyDisposition::IgnoredDuplicate);
        case SequenceDecision::Gap:
            return record_gap(batch, classification.gap_reason.value());
        case SequenceDecision::Apply:
            apply_transaction(batch.levels);
            if (batch.range.final() > current) {
                last_update_id_ = batch.range.final();
            }
            status_ = ProjectionStatus::Synchronized;
            return apply_result(ApplyDisposition::Applied);
        }
        return apply_result(ApplyDisposition::RejectedWrongState);
    }

    void reset() noexcept {
        book_.clear();
        last_update_id_.reset();
        last_gap_.reset();
        status_ = ProjectionStatus::AwaitingBaseline;
    }

    [[nodiscard]] std::optional<std::reference_wrapper<const OrderBook>>
    synchronized_book() const noexcept {
        if (status_ != ProjectionStatus::Synchronized) {
            return std::nullopt;
        }
        return std::cref(book_);
    }

    [[nodiscard]] const OrderBook& diagnostic_book() const noexcept { return book_; }

  private:
    [[nodiscard]] Classification classify(UpdateId current, DepthBatch batch) const noexcept {
        switch (policy_) {
        case SequencePolicyKind::Spot:
            if (status_ == ProjectionStatus::AwaitingBridge) {
                return classify_spot_bootstrap(current, batch.range);
            }
            return classify_spot_live(current, batch.range);
        case SequencePolicyKind::UsdMPerpetual:
            if (status_ == ProjectionStatus::AwaitingBridge) {
                return classify_usdm_bootstrap(current, batch.range, batch.previous_final);
            }
            return classify_usdm_live(current, batch.range, batch.previous_final);
        }
        return gap_decision(GapReason::SpotLiveForwardGap);
    }

    void apply_transaction(std::span<const LevelUpdate> levels) {
        const auto bids = book_.all_levels(BookSide::Bid);
        const auto asks = book_.all_levels(BookSide::Ask);
        OrderBook candidate{numeric_spec_};
        candidate.replace_all(bids, asks);
        candidate.apply_updates(levels);
        book_ = std::move(candidate);
    }

    [[nodiscard]] ApplyResult record_gap(DepthBatch batch, GapReason reason) noexcept {
        // The lifecycle admits this path only after a baseline supplied the current ID.
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        GapInfo gap{last_update_id_.value(), batch.range, batch.previous_final, reason, policy_};
        last_gap_ = gap;
        status_ = ProjectionStatus::NeedsResync;
        return {ApplyDisposition::GapDetected, status_, last_update_id_, gap};
    }

    [[nodiscard]] ApplyResult apply_result(ApplyDisposition disposition) const noexcept {
        return {disposition, status_, last_update_id_, std::nullopt};
    }

    [[nodiscard]] InstallResult install_result(InstallDisposition disposition) const noexcept {
        return {disposition, status_, last_update_id_};
    }

    NumericSpec numeric_spec_;
    SequencePolicyKind policy_;
    ProjectionStatus status_{ProjectionStatus::AwaitingBaseline};
    std::optional<UpdateId> last_update_id_;
    std::optional<GapInfo> last_gap_;
    OrderBook book_;
};

BookProjection::BookProjection(NumericSpec numeric_spec, SequencePolicyKind policy)
    : impl_{std::make_unique<Impl>(numeric_spec, policy)} {}

BookProjection::~BookProjection() = default;

BookProjection::BookProjection(BookProjection&&) noexcept = default;
BookProjection& BookProjection::operator=(BookProjection&&) noexcept = default;

NumericSpec BookProjection::numeric_spec() const noexcept { return impl_->numeric_spec(); }

SequencePolicyKind BookProjection::policy() const noexcept { return impl_->policy(); }

ProjectionStatus BookProjection::status() const noexcept { return impl_->status(); }

std::optional<UpdateId> BookProjection::last_update_id() const noexcept {
    return impl_->last_update_id();
}

std::optional<GapInfo> BookProjection::last_gap() const noexcept { return impl_->last_gap(); }

InstallResult BookProjection::install_baseline(BookBaseline baseline) {
    return impl_->install_baseline(baseline);
}

ApplyResult BookProjection::apply(DepthBatch batch) { return impl_->apply(batch); }

void BookProjection::reset() noexcept { impl_->reset(); }

std::optional<std::reference_wrapper<const OrderBook>>
BookProjection::synchronized_book() const noexcept {
    return impl_->synchronized_book();
}

const OrderBook& BookProjection::diagnostic_book() const noexcept {
    return impl_->diagnostic_book();
}

} // namespace binance_market_data::projection::v1

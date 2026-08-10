#include "reference_projection.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <vector>

namespace bmd = binance_market_data::projection::v1;
namespace reference = bmd_projection_reference;

namespace {

void require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

[[nodiscard]] bmd::DecimalScale scale() {
    const auto value = bmd::DecimalScale::create(8);
    require(value.has_value());
    return value.value();
}

[[nodiscard]] bmd::PriceUnits price(std::int64_t value) {
    const auto result = bmd::PriceUnits::create(value);
    require(result.has_value());
    return result.value();
}

[[nodiscard]] bmd::QuantityUnits quantity(std::int64_t value) {
    const auto result = bmd::QuantityUnits::create(value);
    require(result.has_value());
    return result.value();
}

[[nodiscard]] std::uint64_t decode_id(std::uint8_t value) noexcept {
    if (value == static_cast<std::uint8_t>('Z')) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    if (value >= static_cast<std::uint8_t>('0') && value <= static_cast<std::uint8_t>('9')) {
        return static_cast<std::uint64_t>(value - static_cast<std::uint8_t>('0'));
    }
    return static_cast<std::uint64_t>(value % 64U);
}

[[nodiscard]] std::vector<reference::RawLevel> decode_levels(const std::uint8_t* operation,
                                                             bool baseline) {
    if (operation[7] == static_cast<std::uint8_t>('E')) {
        return {};
    }
    const auto price_value = static_cast<std::int64_t>((operation[5] % 32U) + 1U);
    const auto quantity_value = static_cast<std::int64_t>(operation[6] % 8U);
    const bool bid = (operation[4] & 1U) == 0;
    std::vector<reference::RawLevel> levels{{bid, price_value, quantity_value}};
    if ((operation[7] & 1U) != 0) {
        levels.push_back({bid, price_value, static_cast<std::int64_t>((operation[6] + 1U) % 8U)});
    }
    if (baseline && (operation[7] & 2U) != 0) {
        const auto other_price =
            static_cast<std::int64_t>(((operation[5] + operation[7]) % 32U) + 1U);
        levels.push_back({!bid, other_price, static_cast<std::int64_t>((operation[6] + 2U) % 8U)});
    }
    return levels;
}

[[nodiscard]] std::vector<bmd::LevelUpdate>
to_updates(const std::vector<reference::RawLevel>& raw) {
    std::vector<bmd::LevelUpdate> result;
    result.reserve(raw.size());
    for (const auto& level : raw) {
        result.push_back({level.bid ? bmd::BookSide::Bid : bmd::BookSide::Ask, price(level.price),
                          quantity(level.quantity)});
    }
    return result;
}

[[nodiscard]] std::vector<bmd::BookLevel> to_baseline(const std::vector<reference::RawLevel>& raw,
                                                      bool bid) {
    std::vector<bmd::BookLevel> result;
    for (const auto& level : raw) {
        if (level.bid == bid) {
            result.push_back({price(level.price), quantity(level.quantity)});
        }
    }
    return result;
}

[[nodiscard]] reference::Status reference_status(bmd::ProjectionStatus status) noexcept {
    switch (status) {
    case bmd::ProjectionStatus::AwaitingBaseline:
        return reference::Status::AwaitingBaseline;
    case bmd::ProjectionStatus::AwaitingBridge:
        return reference::Status::AwaitingBridge;
    case bmd::ProjectionStatus::Synchronized:
        return reference::Status::Synchronized;
    case bmd::ProjectionStatus::NeedsResync:
        return reference::Status::NeedsResync;
    }
    return reference::Status::AwaitingBaseline;
}

[[nodiscard]] reference::Policy reference_policy(bmd::SequencePolicyKind policy) noexcept {
    return policy == bmd::SequencePolicyKind::Spot ? reference::Policy::Spot
                                                   : reference::Policy::UsdM;
}

[[nodiscard]] reference::Disposition
reference_disposition(bmd::ApplyDisposition disposition) noexcept {
    switch (disposition) {
    case bmd::ApplyDisposition::Applied:
        return reference::Disposition::Applied;
    case bmd::ApplyDisposition::IgnoredStale:
        return reference::Disposition::IgnoredStale;
    case bmd::ApplyDisposition::IgnoredDuplicate:
        return reference::Disposition::IgnoredDuplicate;
    case bmd::ApplyDisposition::GapDetected:
        return reference::Disposition::GapDetected;
    case bmd::ApplyDisposition::RejectedWrongState:
        return reference::Disposition::RejectedWrongState;
    }
    return reference::Disposition::RejectedWrongState;
}

[[nodiscard]] reference::GapReason reference_gap_reason(bmd::GapReason reason) noexcept {
    switch (reason) {
    case bmd::GapReason::SpotBootstrapForwardGap:
        return reference::GapReason::SpotBootstrapForwardGap;
    case bmd::GapReason::SpotLiveForwardGap:
        return reference::GapReason::SpotLiveForwardGap;
    case bmd::GapReason::FuturesBootstrapRangeMiss:
        return reference::GapReason::FuturesBootstrapRangeMiss;
    case bmd::GapReason::FuturesMissingPreviousFinal:
        return reference::GapReason::FuturesMissingPreviousFinal;
    case bmd::GapReason::FuturesPreviousFinalMismatch:
        return reference::GapReason::FuturesPreviousFinalMismatch;
    }
    return reference::GapReason::SpotLiveForwardGap;
}

[[nodiscard]] std::vector<reference::RawLevel> raw_levels(const bmd::OrderBook& book,
                                                          bmd::BookSide side) {
    std::vector<reference::RawLevel> result;
    for (const auto& level : book.all_levels(side)) {
        result.push_back({side == bmd::BookSide::Bid, level.price.value(), level.quantity.value()});
    }
    return result;
}

void check_state(const bmd::BookProjection& production,
                 const reference::ReferenceProjection& model) {
    require(reference_status(production.status()) == model.status());
    require(production.synchronized_book().has_value() == model.synchronized_visible());
    require(raw_levels(production.diagnostic_book(), bmd::BookSide::Bid) == model.bids());
    require(raw_levels(production.diagnostic_book(), bmd::BookSide::Ask) == model.asks());

    const auto production_id = production.last_update_id();
    require(production_id.has_value() == model.last().has_value());
    if (production_id.has_value()) {
        require(production_id->value() == model.last().value());
    }

    const auto production_gap = production.last_gap();
    const auto model_gap = model.last_gap();
    require(production_gap.has_value() == model_gap.has_value());
    if (production_gap.has_value()) {
        require(production_gap->last_accepted_final.value() == model_gap->last);
        require(production_gap->incoming_range.first().value() == model_gap->first);
        require(production_gap->incoming_range.final().value() == model_gap->final);
        require(production_gap->incoming_previous_final.has_value() ==
                model_gap->previous.has_value());
        if (production_gap->incoming_previous_final.has_value()) {
            require(production_gap->incoming_previous_final->value() ==
                    model_gap->previous.value());
        }
        require(reference_gap_reason(production_gap->reason) == model_gap->reason);
        require(reference_policy(production_gap->policy) == model_gap->policy);
    }
}

void check_result(const bmd::ApplyResult& production, const reference::Result& model) {
    require(reference_disposition(production.disposition) == model.disposition);
    require(reference_status(production.status_after) == model.status);
    require(production.last_update_id_after.has_value() == model.last.has_value());
    if (production.last_update_id_after.has_value()) {
        require(production.last_update_id_after->value() == model.last.value());
    }
    require(production.gap.has_value() == model.gap.has_value());
    if (production.gap.has_value()) {
        require(production.gap->last_accepted_final.value() == model.gap->last);
        require(production.gap->incoming_range.first().value() == model.gap->first);
        require(production.gap->incoming_range.final().value() == model.gap->final);
        require(production.gap->incoming_previous_final.has_value() ==
                model.gap->previous.has_value());
        if (production.gap->incoming_previous_final.has_value()) {
            require(production.gap->incoming_previous_final->value() ==
                    model.gap->previous.value());
        }
        require(reference_gap_reason(production.gap->reason) == model.gap->reason);
        require(reference_policy(production.gap->policy) == model.gap->policy);
    }
}

[[nodiscard]] std::optional<std::uint64_t>
decode_previous(std::uint8_t mode, const reference::ReferenceProjection& model) {
    if (mode % 3U == 0) {
        return std::nullopt;
    }
    const auto current = model.last().value_or(0);
    return mode % 3U == 1 ? current : current + 1U;
}

void apply_valid(bmd::BookProjection& production, reference::ReferenceProjection& model,
                 std::uint64_t first, std::uint64_t final, std::optional<std::uint64_t> previous,
                 const std::vector<reference::RawLevel>& raw) {
    require(first <= final);
    const auto range = bmd::UpdateRange::try_create(bmd::UpdateId{first}, bmd::UpdateId{final});
    require(range.has_value());
    std::optional<bmd::UpdateId> production_previous;
    if (previous.has_value()) {
        production_previous = bmd::UpdateId{previous.value()};
    }
    const auto updates = to_updates(raw);
    const auto production_result = production.apply({range.value(), production_previous, updates});
    const auto model_result = model.apply(first, final, previous, raw);
    check_result(production_result, model_result);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size < 1) {
        return 0;
    }

    const bool spot = data[0] == static_cast<std::uint8_t>('S') || (data[0] & 1U) == 0;
    const auto production_policy =
        spot ? bmd::SequencePolicyKind::Spot : bmd::SequencePolicyKind::UsdMPerpetual;
    const auto model_policy = spot ? reference::Policy::Spot : reference::Policy::UsdM;
    const bmd::NumericSpec numeric_spec{scale(), scale()};
    bmd::BookProjection production{numeric_spec, production_policy};
    reference::ReferenceProjection model{model_policy};

    constexpr std::size_t kOperationWidth = 8;
    constexpr std::size_t kMaxOperations = 256;
    const auto available_operations = (size - 1U) / kOperationWidth;
    const auto operation_count = std::min(available_operations, kMaxOperations);
    for (std::size_t index = 0; index < operation_count; ++index) {
        const auto* operation = data + 1U + index * kOperationWidth;
        const auto type = operation[0] % 9U;
        const auto raw = decode_levels(operation, type == 0);

        switch (type) {
        case 0: {
            const auto last = decode_id(operation[1]);
            const auto bids = to_baseline(raw, true);
            const auto asks = to_baseline(raw, false);
            const auto production_result =
                production.install_baseline({bmd::UpdateId{last}, bids, asks});
            const auto model_result = model.install(last, raw);
            require((production_result.disposition == bmd::InstallDisposition::Installed) ==
                    (model_result.disposition == reference::InstallDisposition::Installed));
            break;
        }
        case 1: {
            auto first = decode_id(operation[1]);
            auto final = decode_id(operation[2]);
            if (first > final) {
                std::swap(first, final);
            }
            apply_valid(production, model, first, final, decode_previous(operation[3], model), raw);
            break;
        }
        case 2:
            production.reset();
            model.reset();
            break;
        case 3:
            static_cast<void>(production.diagnostic_book().best_bid());
            static_cast<void>(production.synchronized_book());
            break;
        case 4:
            if (model.last().has_value()) {
                apply_valid(production, model, 0, model.last().value(), std::nullopt, raw);
            }
            break;
        case 5:
            if (model.last().has_value() && model.last().value() > 0) {
                apply_valid(production, model, 0, model.last().value() - 1U, std::nullopt, raw);
            }
            break;
        case 6: {
            const auto before_status = production.status();
            const auto invalid = bmd::UpdateRange::try_create(bmd::UpdateId{2}, bmd::UpdateId{1});
            require(!invalid.has_value());
            require(production.status() == before_status);
            break;
        }
        case 7:
            if (model.last().has_value() && model.last().value() < UINT64_MAX - 2U) {
                const auto current = model.last().value();
                const auto previous = spot ? std::optional<std::uint64_t>{}
                                           : std::optional<std::uint64_t>{current + 1U};
                apply_valid(production, model, current + 2U, current + 2U, previous, raw);
            }
            break;
        case 8:
            if (model.last().has_value()) {
                const auto current = model.last().value();
                if (spot) {
                    if (current < UINT64_MAX) {
                        const bool exact_next = (operation[7] & 1U) != 0;
                        const auto first = model.status() == reference::Status::AwaitingBridge
                                               ? (exact_next ? current + 1U : current)
                                               : current + 1U;
                        apply_valid(production, model, first, current + 1U, std::nullopt, raw);
                    }
                } else if (model.status() == reference::Status::AwaitingBridge) {
                    apply_valid(production, model, current, current, current, raw);
                } else if (current < UINT64_MAX - 6U) {
                    apply_valid(production, model, current + 5U, current + 6U, current, raw);
                }
            }
            break;
        }
        check_state(production, model);
    }

    return 0;
}

#include "../projection_state/test_helpers.hpp"

#include <binance_market_data/common/v1/enums.pb.h>
#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace allocation_control {

// These controls are confined to this dedicated single-threaded test executable.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
thread_local bool enabled = false;
thread_local std::size_t count = 0;
thread_local std::size_t fail_at = std::numeric_limits<std::size_t>::max();
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

class Scope final {
  public:
    explicit Scope(std::size_t failure_index) noexcept {
        count = 0;
        fail_at = failure_index;
        enabled = true;
    }
    ~Scope() { enabled = false; }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    [[nodiscard]] static std::size_t allocations() noexcept { return count; }
};

[[nodiscard]] void* allocate(std::size_t size) {
    if (enabled) {
        const auto index = count++;
        if (index == fail_at) {
            throw std::bad_alloc{};
        }
    }
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    if (void* memory = std::malloc(size == 0 ? 1 : size); memory != nullptr) {
        return memory;
    }
    throw std::bad_alloc{};
}

} // namespace allocation_control

void* operator new(std::size_t size) { return allocation_control::allocate(size); }
void* operator new[](std::size_t size) { return allocation_control::allocate(size); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocation_control::allocate(size);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocation_control::allocate(size);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void operator delete(void* memory) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}
void operator delete[](void* memory) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}
void operator delete[](void* memory, std::size_t) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}
void operator delete(void* memory, const std::nothrow_t&) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}
void operator delete[](void* memory, const std::nothrow_t&) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}

namespace {

namespace adapter = binance_market_data::projection_adapter::v1;
namespace common_wire = binance_market_data::common::v1;
namespace core = binance_market_data::projection::v1;
namespace market_wire = binance_market_data::market::v1;
namespace helper = bmd_projection_test;

enum class Scenario : std::uint8_t {
    AdaptSnapshot,
    AdaptUpdate,
    CheckedInstall,
    CheckedApply,
    SnapshotOutput,
    GapOutput,
};

[[nodiscard]] market_wire::ExchangeDepthSnapshot make_baseline() {
    market_wire::ExchangeDepthSnapshot wire;
    wire.set_venue(common_wire::VENUE_BINANCE);
    wire.set_market(common_wire::MARKET_SPOT);
    wire.set_symbol("BTCUSDT");
    wire.set_schema_version("exchange-depth-snapshot.v1");
    wire.set_producer("allocation-test");
    wire.set_producer_version("1.0");
    wire.set_request_id("allocation-request");
    wire.set_last_update_id(100);
    wire.add_quality_flags(common_wire::QUALITY_FLAG_OUT_OF_ORDER);
    for (int index = 0; index < 4; ++index) {
        auto* bid = wire.add_bids();
        bid->set_price(std::to_string(100 - index) + ".00");
        bid->set_quantity(std::to_string(index + 1) + ".000");
        auto* ask = wire.add_asks();
        ask->set_price(std::to_string(101 + index) + ".00");
        ask->set_quantity(std::to_string(index + 2) + ".000");
    }
    return wire;
}

[[nodiscard]] market_wire::DepthUpdate make_update(std::uint64_t first = 99,
                                                   std::uint64_t final = 101) {
    market_wire::DepthUpdate wire;
    auto* metadata = wire.mutable_metadata();
    metadata->set_venue(common_wire::VENUE_BINANCE);
    metadata->set_market(common_wire::MARKET_SPOT);
    metadata->set_symbol("BTCUSDT");
    metadata->set_producer("allocation-test");
    metadata->set_producer_version("1.0");
    metadata->set_connection_id("allocation-connection");
    metadata->set_stream(common_wire::STREAM_DIFF_DEPTH);
    metadata->set_schema_version("depth-update.v1");
    metadata->add_quality_flags(common_wire::QUALITY_FLAG_DUPLICATE);
    wire.set_first_update_id(first);
    wire.set_final_update_id(final);
    for (int index = 0; index < 4; ++index) {
        auto* bid = wire.add_bids();
        bid->set_price(std::to_string(100 - index) + ".00");
        bid->set_quantity(std::to_string(index + 5) + ".000");
    }
    return wire;
}

class PreparedScenario final {
  public:
    explicit PreparedScenario(Scenario scenario)
        : scenario_{scenario}, baseline_{make_baseline()}, update_{make_update()},
          projection_{helper::spec(), core::SequencePolicyKind::Spot} {
        if (scenario == Scenario::CheckedInstall) {
            baseline_owner_.emplace(take_baseline_owner());
        } else if (scenario == Scenario::CheckedApply || scenario == Scenario::SnapshotOutput ||
                   scenario == Scenario::GapOutput) {
            auto owner = take_baseline_owner();
            static_cast<void>(owner.install_into(projection_));
            if (scenario != Scenario::SnapshotOutput) {
                update_owner_.emplace(take_update_owner(update_));
            }
            if (scenario == Scenario::GapOutput) {
                static_cast<void>(update_owner_->apply_to(projection_));
                auto gap_wire = make_update(200, 201);
                auto gap_owner = take_update_owner(gap_wire);
                static_cast<void>(gap_owner.apply_to(projection_));
            }
        }
    }

    [[nodiscard]] bool invoke() {
        switch (scenario_) {
        case Scenario::AdaptSnapshot:
            return std::holds_alternative<adapter::AdaptedBookBaseline>(
                adapter::adapt_exchange_depth_snapshot(baseline_, helper::spec(), identity()));
        case Scenario::AdaptUpdate:
            return std::holds_alternative<adapter::AdaptedDepthBatch>(
                adapter::adapt_depth_update(update_, helper::spec(), identity()));
        case Scenario::CheckedInstall:
            return std::holds_alternative<core::InstallResult>(
                baseline_owner_->install_into(projection_));
        case Scenario::CheckedApply:
            return std::holds_alternative<core::ApplyResult>(update_owner_->apply_to(projection_));
        case Scenario::SnapshotOutput:
            return std::holds_alternative<core::LocalOrderBookSnapshot>(
                adapter::make_local_order_book_snapshot(projection_, snapshot_context(),
                                                        options()));
        case Scenario::GapOutput: {
            auto context = snapshot_context();
            context.current_gap =
                adapter::CurrentGapContext{123456, adapter::GapRecoveryState::ResyncRequired};
            return std::holds_alternative<core::LocalOrderBookSnapshot>(
                adapter::make_local_order_book_snapshot(projection_, context, options()));
        }
        }
        return false;
    }

    [[nodiscard]] helper::ProjectionCheckpoint checkpoint() const {
        return helper::checkpoint(projection_);
    }

    [[nodiscard]] std::string baseline_bytes() const { return baseline_.SerializeAsString(); }
    [[nodiscard]] std::string update_bytes() const { return update_.SerializeAsString(); }

  private:
    [[nodiscard]] static adapter::ExpectedIdentity identity() {
        return {"BTCUSDT", core::SequencePolicyKind::Spot};
    }

    [[nodiscard]] static adapter::SnapshotContext snapshot_context() {
        return {identity(), "allocation-test",  "1.0",       adapter::SnapshotOrigin::HistoryReplay,
                123456,     std::uint64_t{789}, std::nullopt};
    }

    [[nodiscard]] static adapter::SnapshotOptions options() {
        adapter::SnapshotOptions value;
        value.depth_limit = std::get<adapter::DepthLimit>(adapter::DepthLimit::create(3));
        value.host_quality_facts = {adapter::HostQualityFact::Duplicate,
                                    adapter::HostQualityFact::OutOfOrder};
        return value;
    }

    [[nodiscard]] adapter::AdaptedBookBaseline take_baseline_owner() {
        auto result = adapter::adapt_exchange_depth_snapshot(baseline_, helper::spec(), identity());
        return std::move(std::get<adapter::AdaptedBookBaseline>(result));
    }

    [[nodiscard]] static adapter::AdaptedDepthBatch
    take_update_owner(const market_wire::DepthUpdate& wire) {
        auto result = adapter::adapt_depth_update(wire, helper::spec(), identity());
        return std::move(std::get<adapter::AdaptedDepthBatch>(result));
    }

    Scenario scenario_;
    market_wire::ExchangeDepthSnapshot baseline_;
    market_wire::DepthUpdate update_;
    core::BookProjection projection_;
    std::optional<adapter::AdaptedBookBaseline> baseline_owner_;
    std::optional<adapter::AdaptedDepthBatch> update_owner_;
};

// Assertions are kept outside the measured region so their allocations cannot affect the sweep.
void verify_strong_guarantee(Scenario scenario) {
    PreparedScenario counting{scenario};
    std::size_t allocation_count = 0;
    bool counting_success = false;
    {
        allocation_control::Scope scope{std::numeric_limits<std::size_t>::max()};
        counting_success = counting.invoke();
        allocation_count = allocation_control::Scope::allocations();
    }
    ASSERT_TRUE(counting_success);
    ASSERT_GT(allocation_count, 0U);

    for (std::size_t failure_index = 0; failure_index < allocation_count; ++failure_index) {
        PreparedScenario failing{scenario};
        const auto checkpoint = failing.checkpoint();
        const auto baseline_bytes = failing.baseline_bytes();
        const auto update_bytes = failing.update_bytes();
        bool threw = false;
        {
            allocation_control::Scope scope{failure_index};
            try {
                static_cast<void>(failing.invoke());
            } catch (const std::bad_alloc&) {
                threw = true;
            }
        }
        ASSERT_TRUE(threw) << "allocation index=" << failure_index;
        EXPECT_EQ(failing.checkpoint(), checkpoint) << "allocation index=" << failure_index;
        EXPECT_EQ(failing.baseline_bytes(), baseline_bytes) << "allocation index=" << failure_index;
        EXPECT_EQ(failing.update_bytes(), update_bytes) << "allocation index=" << failure_index;
    }

    PreparedScenario final_success{scenario};
    EXPECT_TRUE(final_success.invoke());
}

} // namespace

TEST(ProtoAdapterAllocationFailureTest, SnapshotAdaptationHasNoPartialResult) {
    verify_strong_guarantee(Scenario::AdaptSnapshot);
}
TEST(ProtoAdapterAllocationFailureTest, UpdateAdaptationHasNoPartialResult) {
    verify_strong_guarantee(Scenario::AdaptUpdate);
}
TEST(ProtoAdapterAllocationFailureTest, CheckedInstallPreservesProjection) {
    verify_strong_guarantee(Scenario::CheckedInstall);
}
TEST(ProtoAdapterAllocationFailureTest, CheckedApplyPreservesProjection) {
    verify_strong_guarantee(Scenario::CheckedApply);
}
TEST(ProtoAdapterAllocationFailureTest, SnapshotOutputPreservesProjection) {
    verify_strong_guarantee(Scenario::SnapshotOutput);
}
TEST(ProtoAdapterAllocationFailureTest, GapOutputPreservesProjection) {
    verify_strong_guarantee(Scenario::GapOutput);
}

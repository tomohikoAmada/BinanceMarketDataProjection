#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <optional>
#include <vector>

namespace allocation_control {

// These thread-local controls are confined to the dedicated single-threaded test executable.
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
        const auto index = count;
        ++count;
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

void* operator new(std::size_t size, const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
    try {
        return allocation_control::allocate(size);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
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

void operator delete(void* memory, std::size_t size) noexcept {
    static_cast<void>(size);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}

void operator delete[](void* memory, std::size_t size) noexcept {
    static_cast<void>(size);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}

void operator delete(void* memory, const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}

void operator delete[](void* memory, const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(memory);
}

namespace bmd = binance_market_data::projection::v1;
namespace helper = bmd_projection_test;

namespace {

enum class Scenario : std::uint8_t {
    BaselineInstall,
    SpotBridge,
    SpotLive,
    UsdMAdvancingBridge,
    UsdMEqualityBridge,
    UsdMLive,
};

[[nodiscard]] bmd::SequencePolicyKind policy_for(Scenario scenario) noexcept {
    switch (scenario) {
    case Scenario::BaselineInstall:
    case Scenario::SpotBridge:
    case Scenario::SpotLive:
        return bmd::SequencePolicyKind::Spot;
    case Scenario::UsdMAdvancingBridge:
    case Scenario::UsdMEqualityBridge:
    case Scenario::UsdMLive:
        return bmd::SequencePolicyKind::UsdMPerpetual;
    }
    return bmd::SequencePolicyKind::Spot;
}

class PreparedScenario final {
  public:
    explicit PreparedScenario(Scenario scenario)
        : scenario_{scenario}, projection_{helper::spec(), policy_for(scenario)},
          baseline_bids_{{helper::price(100), helper::quantity(5)},
                         {helper::price(99), helper::quantity(3)},
                         {helper::price(98), helper::quantity(2)}},
          baseline_asks_{{helper::price(101), helper::quantity(4)},
                         {helper::price(102), helper::quantity(6)}},
          updates_{{bmd::BookSide::Bid, helper::price(100), helper::quantity(7)},
                   {bmd::BookSide::Bid, helper::price(97), helper::quantity(8)},
                   {bmd::BookSide::Ask, helper::price(103), helper::quantity(9)}} {
        prepare_state();
    }

    [[nodiscard]] bool invoke() {
        if (scenario_ == Scenario::BaselineInstall) {
            const auto result = helper::install(projection_, 500, baseline_bids_, baseline_asks_);
            return result.disposition == bmd::InstallDisposition::Installed;
        }

        switch (scenario_) {
        case Scenario::BaselineInstall:
            return false;
        case Scenario::SpotBridge:
            return helper::apply(projection_, 499, 501, std::nullopt, updates_).disposition ==
                   bmd::ApplyDisposition::Applied;
        case Scenario::SpotLive:
            return helper::apply(projection_, 502, 502, std::nullopt, updates_).disposition ==
                   bmd::ApplyDisposition::Applied;
        case Scenario::UsdMAdvancingBridge:
            return helper::apply(projection_, 499, 501, 450, updates_).disposition ==
                   bmd::ApplyDisposition::Applied;
        case Scenario::UsdMEqualityBridge:
            return helper::apply(projection_, 499, 500, 450, updates_).disposition ==
                   bmd::ApplyDisposition::Applied;
        case Scenario::UsdMLive:
            return helper::apply(projection_, 502, 502, 501, updates_).disposition ==
                   bmd::ApplyDisposition::Applied;
        }
        return false;
    }

    [[nodiscard]] helper::ProjectionCheckpoint checkpoint() const {
        return helper::checkpoint(projection_);
    }

  private:
    void prepare_state() {
        if (scenario_ == Scenario::BaselineInstall) {
            return;
        }
        static_cast<void>(helper::install(projection_, 500, baseline_bids_, baseline_asks_));
        if (scenario_ == Scenario::SpotLive) {
            static_cast<void>(helper::apply(projection_, 499, 501));
        } else if (scenario_ == Scenario::UsdMLive) {
            static_cast<void>(helper::apply(projection_, 499, 501, 450));
        }
    }

    Scenario scenario_;
    bmd::BookProjection projection_;
    std::vector<bmd::BookLevel> baseline_bids_;
    std::vector<bmd::BookLevel> baseline_asks_;
    std::vector<bmd::LevelUpdate> updates_;
};

// GoogleTest assertion macros inside the allocation sweep inflate the measured score.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
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
        const auto before = failing.checkpoint();
        bool threw_bad_alloc = false;
        {
            allocation_control::Scope scope{failure_index};
            try {
                static_cast<void>(failing.invoke());
            } catch (const std::bad_alloc&) {
                threw_bad_alloc = true;
            }
        }
        ASSERT_TRUE(threw_bad_alloc) << "allocation index=" << failure_index;
        EXPECT_EQ(failing.checkpoint(), before) << "allocation index=" << failure_index;
    }

    PreparedScenario final_success{scenario};
    EXPECT_TRUE(final_success.invoke());
}

} // namespace

TEST(BookProjectionAllocationFailureTest, BaselineInstallHasStrongGuarantee) {
    verify_strong_guarantee(Scenario::BaselineInstall);
}

TEST(BookProjectionAllocationFailureTest, SpotBridgeHasStrongGuarantee) {
    verify_strong_guarantee(Scenario::SpotBridge);
}

TEST(BookProjectionAllocationFailureTest, SpotLiveHasStrongGuarantee) {
    verify_strong_guarantee(Scenario::SpotLive);
}

TEST(BookProjectionAllocationFailureTest, UsdMAdvancingBridgeHasStrongGuarantee) {
    verify_strong_guarantee(Scenario::UsdMAdvancingBridge);
}

TEST(BookProjectionAllocationFailureTest, UsdMEqualityBridgeHasStrongGuarantee) {
    verify_strong_guarantee(Scenario::UsdMEqualityBridge);
}

TEST(BookProjectionAllocationFailureTest, UsdMLiveHasStrongGuarantee) {
    verify_strong_guarantee(Scenario::UsdMLive);
}

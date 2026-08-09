#pragma once

// Shared helpers for M5 Phase-2 differential tests: fixture loading and driver
// invocation. Fixture names are compile-time constants verified by identity checks
// in load_fixture; a failure here is a repository integrity error.

#include "replay_driver.hpp"
#include "replay_fixture.hpp"
#include "replay_types.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace bmd_projection::m5::oracle::test {

[[nodiscard]] inline std::filesystem::path fixture_root() {
    return std::filesystem::path{BMD_M5_FIXTURE_ROOT};
}

[[nodiscard]] inline replay::ReplayFixture load_fixture(std::string_view name) {
    const auto loaded = replay::load_fixture(fixture_root() / std::string{name});
    if (!std::holds_alternative<replay::ReplayFixture>(loaded)) {
        std::abort();
    }
    return std::get<replay::ReplayFixture>(loaded);
}

} // namespace bmd_projection::m5::oracle::test

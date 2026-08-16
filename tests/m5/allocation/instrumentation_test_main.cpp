#include "instrumentation_test_main.hpp"
#include "allocation_instrumentation.hpp"
#include "instrumentation_test_seams.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <iostream>
#include <new>
#include <sstream>
#include <string_view>

namespace bmd_projection::m5::allocation_test {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
const char* g_test_binary_path = nullptr;

const char* test_binary_path() noexcept { return g_test_binary_path; }

// Fixed deterministic profile for the repeated-process normalized-metric
// determinism check (OD-M5-P7-020 case 28). The parent test asserts the exact
// hand-computed normalized values printed here:
//   allocation_count == 3; total_allocated_bytes == 56
//   deallocation_count == 1; deallocated_bytes == 16
//   persistent_live_delta == {positive, 40}
//   peak_above_entry == 48; transient_excess_over_persistent == 8
// (A == 0: the child resets the instrumentation state before the profile.)
int run_determinism_child() {
    using bmd_projection::m5::allocation::MeasurementScope;
    using bmd_projection::m5::allocation::test::reset_instrumentation_state_for_test;

    reset_instrumentation_state_for_test();
    MeasurementScope scope;
    void* p16 = ::operator new(16);
    void* p32 = ::operator new(32);
    ::operator delete(p16);
    void* p8 = ::operator new(8);
    scope.finish();
    const auto& result = scope.result();
    static_cast<void>(p32);
    static_cast<void>(p8);
    std::ostringstream output;
    output << result.allocation_count << ' ' << result.total_allocated_bytes << ' '
           << result.deallocation_count << ' ' << result.deallocated_bytes << ' '
           << static_cast<unsigned int>(result.persistent_live_delta.sign) << ' '
           << result.persistent_live_delta.magnitude << ' ' << result.peak_above_entry << ' '
           << result.transient_excess_over_persistent << '\n';
    std::cout << output.str();
    return 0;
}

} // namespace bmd_projection::m5::allocation_test

int main(int argc, char** argv) {
    if (argc >= 1) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        bmd_projection::m5::allocation_test::g_test_binary_path = argv[0];
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (argc == 2 && std::string_view(argv[1]) == "--p7-determinism-child") {
        return bmd_projection::m5::allocation_test::run_determinism_child();
    }
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

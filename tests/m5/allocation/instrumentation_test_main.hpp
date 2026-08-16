#pragma once

// Shared entrypoint plumbing for the Phase-7 allocation instrumentation test
// executable: the binary-path accessor for the subprocess determinism test
// (OD-M5-P7-020 case 28) and the child-mode entrypoint declaration.

namespace bmd_projection::m5::allocation_test {

// argv[0] recorded by the custom test main; nullptr if unavailable.
[[nodiscard]] const char* test_binary_path() noexcept;

} // namespace bmd_projection::m5::allocation_test

#pragma once

// Structured fuzz fixture builder.
//
// Constructs a ReplayFixture from a FuzzCase so the ReplayDriver can consume it.
// No canonical replay log exists; the log-SHA fields are left empty. The
// fixture identity is truthful about the structured fuzz origin.

#include "replay_fixture.hpp"
#include "replay_fuzz_decoder.hpp"
#include "replay_types.hpp"

#include <string>

namespace bmd_projection::m5::replay::fuzz_decoder {

// Builds a ReplayFixture from a structured FuzzCase.
// The structured fuzz case has no canonical replay log; identity SHA fields
// are empty. The ReplayDriver uses them only for diagnostic messages.
[[nodiscard]] ReplayFixture build_structured_fixture(const FuzzCase& fuzz_case);

} // namespace bmd_projection::m5::replay::fuzz_decoder

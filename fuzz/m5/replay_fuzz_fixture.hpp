#pragma once

// Structured fuzz fixture builder.
//
// Constructs a ReplayFixture from a FuzzCase so the ReplayDriver can consume it.
// The identity fields are truthful about the structured fuzz origin; no canonical
// text log is fabricated.

#include "replay_fixture.hpp"
#include "replay_fuzz_decoder.hpp"
#include "replay_types.hpp"

#include <string>
#include <string_view>

namespace bmd_projection::m5::replay::fuzz_decoder {

// Builds a ReplayFixture from a structured FuzzCase.
// The `input_hash_hex` should be the SHA-256 of the raw fuzz input bytes.
// If not available, an empty string is truthful (the bytes came from fuzz, not a log).
[[nodiscard]] ReplayFixture build_structured_fixture(const FuzzCase& fuzz_case,
                                                     std::string_view input_hash_hex);

} // namespace bmd_projection::m5::replay::fuzz_decoder

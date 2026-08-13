#include "replay_fuzz_fixture.hpp"

#include <string>

namespace bmd_projection::m5::replay::fuzz_decoder {

ReplayFixture build_structured_fixture(const FuzzCase& fuzz_case) {
    ReplayFixture fixture;

    fixture.identity.schema_version = kReplaySchemaVersion;
    fixture.identity.market = fuzz_case.market;
    fixture.identity.symbol = fuzz_case.symbol;
    fixture.identity.numeric_spec = fuzz_case.numeric_spec;
    fixture.identity.sequence_policy = fuzz_case.sequence_policy;
    fixture.identity.fixture_id = "structured-fuzz";

    // Structured fuzz cases do not originate from a canonical replay log. The
    // replay-log-SHA and canonical-log-SHA fields are left empty because
    // placing raw fuzz bytes or a synthetic sentinel into them would be a
    // false claim of canonical replay provenance. ReplayDriver consumes these
    // fields only for diagnostic messages.
    fixture.identity.replay_log_sha256.clear();
    fixture.canonical_log_sha256.clear();

    fixture.manifest.identity = fixture.identity;
    fixture.manifest.event_count = fuzz_case.operations.size();

    fixture.replay.header.schema_version = kReplaySchemaVersion;
    fixture.replay.header.market = fuzz_case.market;
    fixture.replay.header.symbol = fuzz_case.symbol;
    fixture.replay.header.numeric_spec = fuzz_case.numeric_spec;
    fixture.replay.header.sequence_policy = fuzz_case.sequence_policy;
    fixture.replay.header.fixture_id = "structured-fuzz";
    fixture.replay.operations = fuzz_case.operations;

    return fixture;
}

} // namespace bmd_projection::m5::replay::fuzz_decoder

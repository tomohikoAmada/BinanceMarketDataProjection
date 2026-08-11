#include "medium_validity.hpp"

#include "divergence.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::phase3 {
namespace {

namespace minimal_json {

struct Null final {};
struct Object;
struct Array;

using Value = std::variant<Null, bool, std::uint64_t, std::string, Array, Object>;

struct Array final {
    std::vector<Value> items;
};

struct Object final {
    std::map<std::string, Value, std::less<>> fields;
};

// Strict parser for the canonical object subset emitted by the materializer
// (json.dumps(sort_keys=True, separators=(",", ":"))). Integers only; floats,
// surrogate pairs, and duplicate object keys are rejected so the validator
// fails closed instead of guessing.
class Parser final {
  public:
    explicit Parser(std::string_view text) : text_{text} {}

    [[nodiscard]] std::optional<Value> parse() {
        skip_whitespace();
        auto value = parse_value();
        if (!value.has_value()) {
            return std::nullopt;
        }
        skip_whitespace();
        if (offset_ != text_.size()) {
            return std::nullopt;
        }
        return value;
    }

  private:
    void skip_whitespace() noexcept {
        while (offset_ < text_.size() && (text_[offset_] == ' ' || text_[offset_] == '\t' ||
                                          text_[offset_] == '\n' || text_[offset_] == '\r')) {
            ++offset_;
        }
    }

    [[nodiscard]] bool take(char expected) noexcept {
        if (offset_ < text_.size() && text_[offset_] == expected) {
            ++offset_;
            return true;
        }
        return false;
    }

    [[nodiscard]] std::optional<Value> parse_value() {
        skip_whitespace();
        if (offset_ >= text_.size()) {
            return std::nullopt;
        }
        switch (text_[offset_]) {
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case '"':
            return parse_string();
        case 't':
            return parse_literal("true", Value{true});
        case 'f':
            return parse_literal("false", Value{false});
        case 'n':
            return parse_literal("null", Value{Null{}});
        default:
            return parse_number();
        }
    }

    [[nodiscard]] std::optional<Value> parse_literal(std::string_view literal, Value value) {
        if (text_.substr(offset_, literal.size()) == literal) {
            offset_ += literal.size();
            return value;
        }
        return std::nullopt;
    }

    [[nodiscard]] static int hex_value(char character) noexcept {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F') {
            return character - 'A' + 10;
        }
        return -1;
    }

    static void append_utf8(std::string& out, std::uint32_t code_point) {
        if (code_point < 0x80) {
            out.push_back(static_cast<char>(code_point));
        } else if (code_point < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
            out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
        }
    }

    // Decodes one backslash escape into out. Returns false on a malformed
    // escape; the caller then fails closed.
    [[nodiscard]] bool parse_escape(std::string& out) {
        if (offset_ >= text_.size()) {
            return false;
        }
        const char escape = text_[offset_++];
        switch (escape) {
        case '"':
            out.push_back('"');
            return true;
        case '\\':
            out.push_back('\\');
            return true;
        case '/':
            out.push_back('/');
            return true;
        case 'b':
            out.push_back('\b');
            return true;
        case 'f':
            out.push_back('\f');
            return true;
        case 'n':
            out.push_back('\n');
            return true;
        case 'r':
            out.push_back('\r');
            return true;
        case 't':
            out.push_back('\t');
            return true;
        case 'u': {
            if (offset_ + 4 > text_.size()) {
                return false;
            }
            std::uint32_t code_point = 0;
            for (int index = 0; index < 4; ++index) {
                const int digit = hex_value(text_[offset_++]);
                if (digit < 0) {
                    return false;
                }
                code_point = code_point * 16 + static_cast<std::uint32_t>(digit);
            }
            if (code_point >= 0xD800 && code_point <= 0xDFFF) {
                return false;
            }
            append_utf8(out, code_point);
            return true;
        }
        default:
            return false;
        }
    }

    [[nodiscard]] std::optional<Value> parse_string() {
        if (!take('"')) {
            return std::nullopt;
        }
        std::string out;
        while (offset_ < text_.size()) {
            const char character = text_[offset_++];
            if (character == '"') {
                return Value{std::move(out)};
            }
            if (character != '\\') {
                if (static_cast<unsigned char>(character) < 0x20) {
                    return std::nullopt;
                }
                out.push_back(character);
                continue;
            }
            if (!parse_escape(out)) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<Value> parse_number() {
        if (offset_ >= text_.size()) {
            return std::nullopt;
        }
        start_offset_ = offset_;
        bool negative = false;
        if (text_[offset_] == '-') {
            negative = true;
            ++offset_;
        }
        if (offset_ >= text_.size() || text_[offset_] < '0' || text_[offset_] > '9') {
            return std::nullopt;
        }
        if (text_[offset_] == '0') {
            ++offset_;
            if (offset_ < text_.size() && text_[offset_] >= '0' && text_[offset_] <= '9') {
                return std::nullopt;
            }
        } else {
            while (offset_ < text_.size() && text_[offset_] >= '0' && text_[offset_] <= '9') {
                ++offset_;
            }
        }
        if (offset_ < text_.size() &&
            (text_[offset_] == '.' || text_[offset_] == 'e' || text_[offset_] == 'E')) {
            return std::nullopt;
        }
        const std::string_view digits = text_.substr(start_offset_, offset_ - start_offset_);
        std::uint64_t magnitude = 0;
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        for (const char digit : digits.substr(digits[0] == '-' ? 1 : 0)) {
            const auto value = static_cast<std::uint64_t>(digit - '0');
            if (magnitude > (maximum - value) / 10) {
                return std::nullopt;
            }
            magnitude = magnitude * 10 + value;
        }
        if (negative) {
            return std::nullopt;
        }
        return Value{magnitude};
    }

    [[nodiscard]] std::optional<Value> parse_array() {
        if (!take('[')) {
            return std::nullopt;
        }
        skip_whitespace();
        Array array;
        if (take(']')) {
            return Value{std::move(array)};
        }
        while (true) {
            skip_whitespace();
            auto item = parse_value();
            if (!item.has_value()) {
                return std::nullopt;
            }
            array.items.push_back(std::move(*item));
            skip_whitespace();
            if (take(']')) {
                return Value{std::move(array)};
            }
            if (!take(',')) {
                return std::nullopt;
            }
        }
    }

    [[nodiscard]] std::optional<Value> parse_object() {
        if (!take('{')) {
            return std::nullopt;
        }
        skip_whitespace();
        Object object;
        if (take('}')) {
            return Value{std::move(object)};
        }
        while (true) {
            skip_whitespace();
            auto key = parse_string();
            if (!key.has_value()) {
                return std::nullopt;
            }
            skip_whitespace();
            if (!take(':')) {
                return std::nullopt;
            }
            auto value = parse_value();
            if (!value.has_value()) {
                return std::nullopt;
            }
            std::string key_text = std::move(std::get<std::string>(*key));
            const auto inserted = object.fields.emplace(key_text, std::move(*value));
            if (!inserted.second) {
                return std::nullopt;
            }
            skip_whitespace();
            if (take('}')) {
                return Value{std::move(object)};
            }
            if (!take(',')) {
                return std::nullopt;
            }
        }
    }

    std::string_view text_;
    std::size_t offset_{0};
    std::size_t start_offset_{0};
};

[[nodiscard]] std::optional<Value> parse_document(std::string_view text) {
    return Parser{text}.parse();
}

} // namespace minimal_json

[[nodiscard]] std::optional<std::uint64_t>
last_selected_diff_final_update_id(const replay::ReplayFixture& fixture) {
    const auto found =
        std::find_if(fixture.replay.operations.rbegin(), fixture.replay.operations.rend(),
                     [](const replay::Operation& operation) {
                         return std::holds_alternative<replay::DepthUpdateOp>(operation);
                     });
    if (found == fixture.replay.operations.rend()) {
        return std::nullopt;
    }
    return std::get<replay::DepthUpdateOp>(*found).final_update_id;
}

// First emitted depth result that violates the medium contract (not Applied
// while Synchronized); the caller reports the stable reason and event index.
[[nodiscard]] std::optional<oracle::CompactDepthResult>
first_depth_violation(const oracle::ExecutionSummary& summary) {
    for (const auto& depth : summary.depth_results) {
        if (depth.disposition != oracle::CanonicalDisposition::Applied ||
            depth.status_after != oracle::CanonicalStatus::Synchronized) {
            return depth;
        }
    }
    return std::nullopt;
}

// Tail checks: the final state and final accepted ID must match the emitted
// contract. Returns a stable failure reason and event index, or nullopt when
// the tail is valid.
[[nodiscard]] std::optional<std::pair<std::string, std::size_t>>
final_state_failure(const MediumValidityReport& report, const oracle::ReplayOutcome& outcome) {
    if (!outcome.final_observation.has_value()) {
        return std::make_pair(std::string{"no-final-observation"}, std::size_t{0});
    }
    if (report.final_status != oracle::CanonicalStatus::Synchronized) {
        return std::make_pair("final-status-" + std::string(oracle::to_text(report.final_status)),
                              std::size_t{0});
    }
    if (!report.final_accepted_update_id.has_value()) {
        return std::make_pair(std::string{"final-id-missing"}, std::size_t{0});
    }
    if (!report.last_selected_diff_final_update_id.has_value() ||
        report.final_accepted_update_id != report.last_selected_diff_final_update_id) {
        return std::make_pair(std::string{"final-id-mismatch"}, std::size_t{0});
    }
    return std::nullopt;
}

} // namespace

std::optional<std::uint64_t> read_target_live_updates(const std::filesystem::path& directory) {
    const auto path = directory / "corpus_provenance.json";
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        return std::nullopt;
    }
    const auto document = minimal_json::parse_document(buffer.str());
    if (!document.has_value()) {
        return std::nullopt;
    }
    const auto* object = std::get_if<minimal_json::Object>(&*document);
    if (object == nullptr) {
        return std::nullopt;
    }
    const auto found = object->fields.find("selected_live_updates_after_synchronization");
    if (found == object->fields.end()) {
        return std::nullopt;
    }
    const auto* count = std::get_if<std::uint64_t>(&found->second);
    if (count == nullptr) {
        return std::nullopt;
    }
    return *count;
}

MediumValidityReport check_medium_validity(const oracle::ReplayOutcome& outcome,
                                           const replay::ReplayFixture& fixture,
                                           std::uint64_t target_live_updates) {
    using oracle::CanonicalDisposition;
    using oracle::CanonicalStatus;
    namespace oracle = bmd_projection::m5::oracle;

    MediumValidityReport report;
    report.target_live_updates = target_live_updates;
    const auto& summary = outcome.summary;
    report.install_events = summary.install_events;
    report.depth_events = summary.depth_events;
    report.installed_count = summary.installed_count;
    report.applied_count = summary.applied_count;
    report.ignored_stale_count = summary.ignored_stale_count;
    report.ignored_duplicate_count = summary.ignored_duplicate_count;
    report.gap_detected_count = summary.gap_detected_count;
    report.rejected_wrong_state_count = summary.rejected_wrong_state_count;
    report.adapter_error_count = summary.adapter_error_count;
    report.decimal_error_count = summary.decimal_error_count;
    report.other_events_count = summary.other_events_count;
    report.first_install = summary.first_install;
    report.first_depth_update = summary.first_depth_update;
    report.last_selected_diff_final_update_id = last_selected_diff_final_update_id(fixture);
    if (outcome.final_observation.has_value()) {
        report.final_status = outcome.final_observation->checkpoint.status;
        report.final_accepted_update_id = outcome.final_observation->checkpoint.last_update_id;
    }

    const auto fail = [&report](std::string reason, std::size_t event_index = 0) {
        report.valid = false;
        report.reason = std::move(reason);
        report.event_index = event_index;
        return report;
    };

    if (outcome.first_divergence.has_value()) {
        return fail("differential-divergence", outcome.first_divergence->event_index);
    }
    if (outcome.processed_events == 0) {
        return fail("no-events");
    }
    if (report.other_events_count != 0) {
        return fail("unexpected-event-kind", summary.first_other_event_index.value_or(0));
    }
    if (report.install_events != 1) {
        return fail("unexpected-baseline-count");
    }
    if (!report.first_install.has_value() ||
        report.first_install->disposition != CanonicalDisposition::Installed) {
        return fail("baseline-not-installed",
                    report.first_install.has_value() ? report.first_install->event_index : 0);
    }
    if (report.first_install->status_after != CanonicalStatus::AwaitingBridge) {
        return fail("baseline-status-unexpected", report.first_install->event_index);
    }
    constexpr auto kMaxTarget = std::numeric_limits<std::uint64_t>::max();
    if (target_live_updates > kMaxTarget - 2U) {
        return fail("invalid-target");
    }
    const std::uint64_t expected_depth_events = target_live_updates + 1;
    if (summary.depth_events != expected_depth_events) {
        return fail("unexpected-depth-event-count");
    }
    if (const auto violation = first_depth_violation(summary); violation.has_value()) {
        const bool is_bridge = report.first_depth_update.has_value() &&
                               violation->event_index == report.first_depth_update->event_index;
        return fail(is_bridge ? "bridge-not-applied" : "depth-update-not-applied",
                    violation->event_index);
    }
    if (report.gap_detected_count != 0) {
        return fail("gap-detected");
    }
    if (report.rejected_wrong_state_count != 0) {
        return fail("wrong-state-rejected");
    }
    if (report.adapter_error_count != 0) {
        return fail("adapter-error", summary.first_adapter_error_index.value_or(0));
    }
    if (report.decimal_error_count != 0) {
        return fail("decimal-error");
    }
    const std::uint64_t expected_event_count = target_live_updates + 2;
    if (outcome.processed_events != expected_event_count) {
        return fail("unexpected-event-count");
    }
    if (const auto failure = final_state_failure(report, outcome); failure.has_value()) {
        return fail(failure->first, failure->second);
    }
    const std::uint64_t expected_applied = target_live_updates + 1;
    if (summary.applied_count != expected_applied) {
        return fail("unexpected-applied-count");
    }
    report.valid = true;
    report.reason = "valid";
    return report;
}

} // namespace bmd_projection::m5::phase3

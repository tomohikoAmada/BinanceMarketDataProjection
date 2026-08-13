#include "phase6_json.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace bmd_projection::m5::benchmark::json {
namespace {

// Container stack states:
//   'o'  object awaiting its first key
//   'K'  object with entries awaiting the next key
//   'O'  object with a key awaiting its value
//   'a'  array awaiting its first value
//   'A'  array with at least one value written
constexpr char kObjectAwaitingFirstKey = 'o';
constexpr char kObjectAwaitingKey = 'K';
constexpr char kObjectAwaitingValue = 'O';
constexpr char kArrayAwaitingValue = 'a';
constexpr char kArrayHasValue = 'A';

} // namespace

void Writer::raw_string(std::string_view value) {
    out_.reserve(out_.size() + value.size() + 2);
    out_.push_back('"');
    for (const char character : value) {
        switch (character) {
        case '"':
            out_ += "\\\"";
            break;
        case '\\':
            out_ += "\\\\";
            break;
        case '\b':
            out_ += "\\b";
            break;
        case '\f':
            out_ += "\\f";
            break;
        case '\n':
            out_ += "\\n";
            break;
        case '\r':
            out_ += "\\r";
            break;
        case '\t':
            out_ += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20U) {
                constexpr std::string_view kHexDigits{"0123456789abcdef"};
                const auto code =
                    static_cast<unsigned>(static_cast<unsigned char>(character));
                out_ += "\\u00";
                out_.push_back(kHexDigits[code >> 4U]);
                out_.push_back(kHexDigits[code & 0xFU]);
            } else {
                out_.push_back(character);
            }
            break;
        }
    }
    out_.push_back('"');
}

// Advances the parent container state for a value that is about to be written.
void Writer::advance_for_value() {
    if (stack_.empty()) {
        std::abort();
    }
    const char top = stack_.back();
    if (top == kObjectAwaitingValue) {
        stack_.back() = kObjectAwaitingKey;
    } else if (top == kArrayAwaitingValue) {
        stack_.back() = kArrayHasValue;
    } else if (top == kArrayHasValue) {
        out_.push_back(',');
    } else {
        std::abort();
    }
}

// Completes a just-written container value against its parent state.
void Writer::advance_for_container_end() {
    if (!stack_.empty() && stack_.back() == kArrayAwaitingValue) {
        stack_.back() = kArrayHasValue;
    }
}

void Writer::begin_object() {
    if (!stack_.empty()) {
        advance_for_value();
    }
    out_.push_back('{');
    stack_.push_back(kObjectAwaitingFirstKey);
}

void Writer::end_object() {
    out_.push_back('}');
    stack_.pop_back();
    advance_for_container_end();
}

void Writer::key(std::string_view key_value) {
    if (stack_.empty()) {
        std::abort();
    }
    const char top = stack_.back();
    if (top == kObjectAwaitingKey) {
        out_.push_back(',');
    } else if (top != kObjectAwaitingFirstKey) {
        std::abort();
    }
    raw_string(key_value);
    out_.push_back(':');
    stack_.back() = kObjectAwaitingValue;
}

void Writer::value(std::string_view value) {
    advance_for_value();
    raw_string(value);
}

void Writer::value(const char* value) { this->value(std::string_view{value}); }

void Writer::value(const std::string& value) { this->value(std::string_view{value}); }

void Writer::value(std::uint64_t value) {
    advance_for_value();
    out_ += std::to_string(value);
}

void Writer::value(bool value) {
    advance_for_value();
    out_ += value ? "true" : "false";
}

void Writer::value(double value) {
    advance_for_value();
    std::array<char, 40> buffer{};
    const auto result =
        std::to_chars(buffer.begin(), buffer.end(), value, std::chars_format::general, 17);
    out_.append(buffer.begin(), result.ptr);
}

void Writer::value_null() {
    advance_for_value();
    out_ += "null";
}

void Writer::begin_array() {
    if (!stack_.empty()) {
        advance_for_value();
    }
    out_.push_back('[');
    stack_.push_back(kArrayAwaitingValue);
}

void Writer::end_array() {
    out_.push_back(']');
    stack_.pop_back();
    advance_for_container_end();
}

} // namespace bmd_projection::m5::benchmark::json

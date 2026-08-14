#pragma once

// Minimal deterministic JSON writer for Phase-6 machine-readable outputs.
// The repository has no JSON dependency; this writer covers exactly the
// scalar/object/array shapes the metadata schemas require.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bmd_projection::m5::benchmark::json {

class Writer final {
  public:
    void begin_object();
    void end_object();
    void key(std::string_view key);
    void value(std::string_view value);
    void value(const char* value);
    void value(const std::string& value);
    void value(std::uint64_t value);
    void value(double value);
    void value(bool value);
    void value_null();
    void begin_array();
    void end_array();

    [[nodiscard]] std::string str() const { return out_; }

  private:
    void advance_for_value();
    void advance_for_container_end();
    void raw_string(std::string_view value);

    std::string out_;
    std::vector<char> stack_;
};

} // namespace bmd_projection::m5::benchmark::json

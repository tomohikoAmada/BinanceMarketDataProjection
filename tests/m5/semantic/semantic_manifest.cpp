#include "semantic_manifest.hpp"

#include "../replay/canonical_text.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>

namespace bmd_projection::m5::semantic {
namespace {

void append_json_string(std::string& out, const std::string& value) {
    static constexpr std::string_view kHex = "0123456789ABCDEF";
    out += '"';
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        switch (byte) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (byte <= 0x1FU) {
                out += "\\u00";
                out += kHex.at(static_cast<std::size_t>(byte >> 4U));
                out += kHex.at(static_cast<std::size_t>(byte & 0x0FU));
            } else {
                out += static_cast<char>(byte);
            }
            break;
        }
    }
    out += '"';
}

void append_indent(std::string& out, int level) {
    for (int i = 0; i < level; ++i) {
        out += "  ";
    }
}

} // namespace

std::string render_manifest_json(const SemanticManifest& manifest) {
    std::string out;
    out.reserve(8192);
    out += "{\n";
    append_indent(out, 1);
    append_json_string(out, "schema_version");
    out += ": ";
    append_json_string(out, manifest.schema_version);
    out += ",\n";
    append_indent(out, 1);
    append_json_string(out, "observation_schema_version");
    out += ": ";
    append_json_string(out, manifest.observation_schema_version);
    out += ",\n";
    append_indent(out, 1);
    append_json_string(out, "head_sha");
    out += ": ";
    append_json_string(out, manifest.head_sha);
    out += ",\n";

    append_indent(out, 1);
    out += "\"toolchain\": {\n";
    append_indent(out, 2);
    append_json_string(out, "compiler");
    out += ": ";
    append_json_string(out, manifest.toolchain.compiler);
    out += ",\n";
    append_indent(out, 2);
    append_json_string(out, "compiler_version");
    out += ": ";
    append_json_string(out, manifest.toolchain.compiler_version);
    out += ",\n";
    append_indent(out, 2);
    append_json_string(out, "os");
    out += ": ";
    append_json_string(out, manifest.toolchain.os);
    out += ",\n";
    append_indent(out, 2);
    append_json_string(out, "architecture");
    out += ": ";
    append_json_string(out, manifest.toolchain.architecture);
    out += "\n";
    append_indent(out, 1);
    out += "},\n";

    append_indent(out, 1);
    append_json_string(out, "build_type");
    out += ": ";
    append_json_string(out, manifest.build_type);
    out += ",\n";

    append_indent(out, 1);
    append_json_string(out, "fixture_set_id");
    out += ": ";
    append_json_string(out, manifest.fixture_set_id);
    out += ",\n";

    append_indent(out, 1);
    out += "\"workloads\": [\n";
    for (std::size_t i = 0; i < manifest.workloads.size(); ++i) {
        const auto& w = manifest.workloads[i];
        append_indent(out, 2);
        out += "{\n";
        append_indent(out, 3);
        append_json_string(out, "workload_id");
        out += ": ";
        append_json_string(out, w.workload_id);
        out += ",\n";
        append_indent(out, 3);
        append_json_string(out, "fixture_id");
        out += ": ";
        append_json_string(out, w.fixture_id);
        out += ",\n";
        append_indent(out, 3);
        append_json_string(out, "fixture_hash");
        out += ": ";
        append_json_string(out, w.fixture_hash);
        out += ",\n";
        append_indent(out, 3);
        append_json_string(out, "semantic_digest");
        out += ": ";
        append_json_string(out, w.semantic_digest);
        out += "\n";
        append_indent(out, 2);
        out += "}";
        if (i + 1 < manifest.workloads.size()) {
            out += ",";
        }
        out += "\n";
    }
    append_indent(out, 1);
    out += "]\n";
    out += "}\n";
    return out;
}

std::string compute_fixture_set_id(const std::vector<ManifestWorkloadEntry>& workloads) {
    std::string canonical;
    for (const auto& w : workloads) {
        canonical += w.workload_id;
        canonical += '\n';
        canonical += w.fixture_id;
        canonical += '\n';
        canonical += w.fixture_hash;
        canonical += '\n';
    }
    const auto hash_result = replay::sha256_hex(canonical);
    if (!std::holds_alternative<std::string>(hash_result)) {
        return {};
    }
    return std::get<std::string>(hash_result);
}

bool is_valid_evidence_sha(std::string_view value) noexcept {
    return value.size() == 40 && std::all_of(value.begin(), value.end(), [](char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

} // namespace bmd_projection::m5::semantic

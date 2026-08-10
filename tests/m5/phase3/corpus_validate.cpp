#include "corpus_validation_common.hpp"

#include "core_production_side.hpp"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <span>

namespace {

int validate(const std::filesystem::path& directory) {
    namespace oracle = bmd_projection::m5::oracle;
    namespace replay = bmd_projection::m5::replay;
    return bmd_projection::m5::phase3::run_corpus_validation_impl(
        directory,
        [](const replay::ReplayFixture& fixture) {
            return oracle::make_core_production_side(fixture);
        },
        "M5_CORPUS_VALIDATION_V1", oracle::ReplayMode::CoreOnly);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: bmd_projection_m5_corpus_validate FIXTURE_DIRECTORY\n";
        return 2;
    }
    try {
        const std::span<char*> arguments{argv, static_cast<std::size_t>(argc)};
        return validate(std::filesystem::path{arguments[1]});
    } catch (const std::exception& error) {
        std::cerr << "fixture_validation=FAIL message=" << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "fixture_validation=FAIL message=unknown-exception\n";
        return 1;
    }
}

#include "environment_identity.hpp"

#include "canonical_text.hpp"

#include <sys/utsname.h>

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <thread>
#include <variant>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include <filesystem>

namespace bmd_projection::m5::benchmark {
namespace {

#if defined(__APPLE__)
[[nodiscard]] std::string sysctl_string(const char* name) {
    std::size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0) {
        return "unavailable";
    }
    std::string value(size, '\0');
    if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0) {
        return "unavailable";
    }
    while (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value;
}
#elif defined(__linux__)
[[nodiscard]] std::string cpu_model_linux() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo.is_open()) {
        return "unavailable";
    }
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.starts_with("model name")) {
            const auto colon = line.find(':');
            if (colon == std::string::npos) {
                return "unavailable";
            }
            auto value = line.substr(colon + 1);
            const auto first = value.find_first_not_of(" \t");
            if (first != std::string::npos) {
                value = value.substr(first);
            }
            return value;
        }
    }
    return "unavailable";
}
#endif

} // namespace

EnvironmentIdentity collect_environment_identity() {
    EnvironmentIdentity identity;
    utsname info{};
    if (uname(&info) == 0) {
        identity.os_name = static_cast<const char*>(info.sysname);
        identity.os_version = static_cast<const char*>(info.release);
        identity.architecture = static_cast<const char*>(info.machine);
    } else {
        identity.os_name = "unavailable";
        identity.os_version = "unavailable";
        identity.architecture = "unavailable";
    }
#if defined(__APPLE__)
    identity.cpu_model = sysctl_string("machdep.cpu.brand_string");
#elif defined(__linux__)
    identity.cpu_model = cpu_model_linux();
#else
    identity.cpu_model = "unavailable";
#endif
    const auto cores = std::thread::hardware_concurrency();
    identity.logical_core_count = cores == 0 ? "unavailable" : std::to_string(cores);
    return identity;
}

std::string current_executable_path() {
#if defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        path.resize(size);
        if (_NSGetExecutablePath(path.data(), &size) != 0) {
            return {};
        }
    }
    while (!path.empty() && path.back() == '\0') {
        path.pop_back();
    }
    return path;
#elif defined(__linux__)
    std::error_code error;
    const auto resolved = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) {
        return {};
    }
    return resolved.string();
#else
    return {};
#endif
}

std::string sha256_file_hex(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return {};
    }
    std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const auto hash = replay::sha256_hex(bytes);
    if (!std::holds_alternative<std::string>(hash)) {
        return {};
    }
    return std::get<std::string>(hash);
}

} // namespace bmd_projection::m5::benchmark

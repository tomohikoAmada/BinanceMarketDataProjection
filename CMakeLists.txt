cmake_minimum_required(VERSION 3.24)

project(
    BinanceMarketDataProjection
    VERSION 0.1.0
    LANGUAGES CXX
)

set(BMD_PROJECTION_PRERELEASE "alpha.0")
set(
    BMD_PROJECTION_FULL_VERSION
    "${PROJECT_VERSION}-${BMD_PROJECTION_PRERELEASE}"
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/binance_market_data/projection/v1/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/binance_market_data/projection/v1/version.hpp"
    @ONLY
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(BMD_PROJECTION_BUILD_TESTS "Build projection foundation tests" ${PROJECT_IS_TOP_LEVEL})
option(BMD_PROJECTION_BUILD_BENCHMARKS "Build projection foundation benchmarks" OFF)
option(BMD_PROJECTION_BUILD_FUZZERS "Build Clang libFuzzer targets" OFF)
option(BMD_PROJECTION_ENABLE_WARNINGS "Enable strict warnings for project targets" ON)
option(BMD_PROJECTION_ENABLE_WERROR "Treat project warnings as errors" OFF)
option(BMD_PROJECTION_ENABLE_CLANG_TIDY "Run clang-tidy on project targets" OFF)
option(BMD_PROJECTION_ENABLE_ASAN "Enable AddressSanitizer for project targets" OFF)
option(BMD_PROJECTION_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for project targets" OFF)
option(BMD_PROJECTION_ENABLE_TSAN "Enable ThreadSanitizer for project targets" OFF)
option(BMD_PROJECTION_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)

if(BMD_PROJECTION_ENABLE_ASAN AND BMD_PROJECTION_ENABLE_TSAN)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer cannot be enabled together")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(StaticAnalysis)
include(Coverage)

function(bmd_projection_apply_project_options target)
    bmd_projection_set_warnings(${target})
    bmd_projection_enable_sanitizers(${target})
    bmd_projection_enable_static_analysis(${target})
    bmd_projection_enable_coverage(${target})
endfunction()

add_library(
    bmd_projection_core
    src/numeric/decimal_format.cpp
    src/numeric/decimal_parse.cpp
    src/version.cpp
)
add_library(BinanceMarketDataProjection::Core ALIAS bmd_projection_core)

set_target_properties(
    bmd_projection_core
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE YES
        EXPORT_NAME Core
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_include_directories(
    bmd_projection_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(bmd_projection_core PUBLIC cxx_std_20)

bmd_projection_apply_project_options(bmd_projection_core)

if(BMD_PROJECTION_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "BMD_PROJECTION_BUILD_FUZZERS requires upstream Clang")
    endif()
    target_compile_options(
        bmd_projection_core
        PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer
    )
endif()

if(BMD_PROJECTION_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BMD_PROJECTION_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(BMD_PROJECTION_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

install(
    TARGETS bmd_projection_core
    EXPORT BinanceMarketDataProjectionTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PATTERN "*.in" EXCLUDE
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/include/binance_market_data/projection/v1/version.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/binance_market_data/projection/v1"
)

set(BMD_PROJECTION_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/BinanceMarketDataProjection"
)

configure_package_config_file(
    cmake/BinanceMarketDataProjectionConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/BinanceMarketDataProjectionConfig.cmake"
    INSTALL_DESTINATION "${BMD_PROJECTION_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/BinanceMarketDataProjectionConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

install(
    EXPORT BinanceMarketDataProjectionTargets
    FILE BinanceMarketDataProjectionTargets.cmake
    NAMESPACE BinanceMarketDataProjection::
    DESTINATION "${BMD_PROJECTION_INSTALL_CMAKEDIR}"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/BinanceMarketDataProjectionConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/BinanceMarketDataProjectionConfigVersion.cmake"
    DESTINATION "${BMD_PROJECTION_INSTALL_CMAKEDIR}"
)

# M5 Phase-6 benchmark build identity generation.
#
# Computes the configure-time source/build state and generates
# benchmark_build_identity.hpp for the benchmark and latency executables. The
# dirty bit and git SHA are captured at CONFIGURE time; the Conan recipe
# revisions come from the repository conan.lock and the Contracts binary
# package ID is the SHA-1 of the exact consumed package's conaninfo.txt.
#
# Guarded target definitions allow this file to be included from
# benchmarks/CMakeLists.txt and tests/CMakeLists.txt.

if(NOT TARGET bmd_projection_m5_benchmark_support)
  add_library(
      bmd_projection_m5_benchmark_support
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/book_state.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/core_replay_executor.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/environment_identity.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/latency_stats.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/m2_cells.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/m3_cells.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/phase6_json.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/phase7_record.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/replay_checksum.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/workload_spec.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/wrapper.cpp"
  )
  target_include_directories(
      bmd_projection_m5_benchmark_support
      PUBLIC
          "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support"
          "${CMAKE_CURRENT_BINARY_DIR}/generated"
  )
  target_link_libraries(
      bmd_projection_m5_benchmark_support
      PUBLIC
          bmd_projection_m5_replay_support
          BinanceMarketDataProjection::Core
  )
  set_target_properties(
      bmd_projection_m5_benchmark_support
      PROPERTIES
          CXX_STANDARD 20
          CXX_STANDARD_REQUIRED YES
          CXX_EXTENSIONS NO
  )
  bmd_projection_apply_project_options(bmd_projection_m5_benchmark_support)
endif()

function(bmd_projection_generate_benchmark_build_identity)
  find_package(Git QUIET)
  if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE BMD_P6_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _git_sha_result
    )
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" status --porcelain
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE _git_status_output
        ERROR_QUIET
        RESULT_VARIABLE _git_status_result
    )
    string(LENGTH "${BMD_P6_GIT_SHA}" _git_sha_length)
    if(_git_sha_result EQUAL 0
       AND (_git_sha_length EQUAL 40 OR _git_sha_length EQUAL 64)
       AND BMD_P6_GIT_SHA MATCHES "^[0-9a-fA-F]+$"
       AND _git_status_result EQUAL 0)
      set(BMD_P6_GIT_PROVENANCE_STATUS "known")
    else()
      set(BMD_P6_GIT_SHA "unavailable")
      set(BMD_P6_GIT_PROVENANCE_STATUS "unavailable")
    endif()
    if(BMD_P6_GIT_PROVENANCE_STATUS STREQUAL "known" AND _git_status_output STREQUAL "")
      set(BMD_P6_GIT_DIRTY_AT_CONFIGURE "false")
    elseif(BMD_P6_GIT_PROVENANCE_STATUS STREQUAL "known")
      set(BMD_P6_GIT_DIRTY_AT_CONFIGURE "true")
    else()
      set(BMD_P6_GIT_DIRTY_AT_CONFIGURE "unavailable")
    endif()
  else()
    set(BMD_P6_GIT_SHA "unavailable")
    set(BMD_P6_GIT_DIRTY_AT_CONFIGURE "unavailable")
    set(BMD_P6_GIT_PROVENANCE_STATUS "unavailable")
  endif()

  set(BMD_P6_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}")
  set(BMD_P6_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
  set(BMD_P6_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
  set(BMD_P6_BUILD_TYPE "${CMAKE_BUILD_TYPE}")

  set(_sanitizers)
  foreach(_flag IN ITEMS ASAN UBSAN TSAN)
    if(BMD_PROJECTION_ENABLE_${_flag})
      list(APPEND _sanitizers "${_flag}")
    endif()
  endforeach()
  if(BMD_PROJECTION_ENABLE_COVERAGE)
    list(APPEND _sanitizers "coverage")
  endif()
  if(_sanitizers)
    string(JOIN "," BMD_P6_SANITIZER_STATE ${_sanitizers})
  else()
    set(BMD_P6_SANITIZER_STATE "off")
  endif()

  if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    set(BMD_P6_LTO_STATE "on")
  else()
    set(BMD_P6_LTO_STATE "off")
  endif()

  file(SHA256 "${PROJECT_SOURCE_DIR}/conan.lock" BMD_P6_CONAN_LOCK_SHA256)

  # Conan recipe revisions from the repository lockfile.
  file(READ "${PROJECT_SOURCE_DIR}/conan.lock" _lock_text)
  set(_references)
  set(_remainder "${_lock_text}")
  set(_lock_pattern "\"([^%\"]+)/([0-9][^%\"]*)#([^%\"]+)(%[0-9.]+)?\"")
  while(_remainder MATCHES "${_lock_pattern}")
    string(REGEX MATCH "${_lock_pattern}" _first_match "${_remainder}")
    string(REGEX REPLACE "^\"([^%\"]+)/.*$" "\\1" _name "${_first_match}")
    string(REGEX REPLACE "^\"[^%\"]+/([0-9][^%\"]*)#.*$" "\\1" _version "${_first_match}")
    string(REGEX REPLACE "^\"[^%\"]+/[0-9][^%\"]*#([^%\"]+).*$" "\\1" _rrev "${_first_match}")
    if(_name MATCHES "^(benchmark|gtest|protobuf|binance-market-data-contracts-cpp)$")
      list(APPEND _references "${_name}/${_version}#${_rrev}")
      if(_name STREQUAL "binance-market-data-contracts-cpp")
        set(BMD_P6_CONTRACTS_RECIPE_REVISION "${_rrev}")
      elseif(_name STREQUAL "benchmark")
        set(BMD_P6_GOOGLE_BENCHMARK_VERSION "${_version}")
      endif()
    endif()
    string(REPLACE "${_first_match}" "" _remainder "${_remainder}")
  endwhile()
  list(REMOVE_DUPLICATES _references)
  list(SORT _references)
  string(JOIN ";" BMD_P6_CONAN_REFERENCES ${_references})

  # The Conan generators directory sits next to the CMake binary directory
  # (cmake_layout: <conan output folder>/build/<build_type>/generators).
  get_filename_component(_conan_output_dir "${CMAKE_BINARY_DIR}" DIRECTORY)

  # Real Conan binary package ID of the exact consumed Contracts package.
  # Conan package IDs are the SHA-1 of the package's conaninfo.txt content.
  # BinanceMarketDataContracts_DIR points into
  # <cache>/p/<locator>/p/lib/cmake/BinanceMarketDataContracts; the package
  # directory holding conaninfo.txt is three levels up. A missing/unreadable
  # conaninfo.txt leaves the state unavailable, which formal validation
  # rejects (REQ-004/005).
  set(BMD_P6_CONTRACTS_PACKAGE_ID "unavailable")
  if(DEFINED BinanceMarketDataContracts_DIR)
    set(_contracts_dir "${BinanceMarketDataContracts_DIR}")
    get_filename_component(_contracts_dir "${_contracts_dir}" DIRECTORY)
    get_filename_component(_contracts_dir "${_contracts_dir}" DIRECTORY)
    get_filename_component(_contracts_dir "${_contracts_dir}" DIRECTORY)
    set(_contracts_conaninfo "${_contracts_dir}/conaninfo.txt")
    if(EXISTS "${_contracts_conaninfo}")
      file(SHA1 "${_contracts_conaninfo}" BMD_P6_CONTRACTS_PACKAGE_ID)
    endif()
  endif()

  # Google Benchmark version: primary source is the repository conan.lock
  # (parsed above); fall back to the CMakeDeps config-version file.
  if(NOT DEFINED BMD_P6_GOOGLE_BENCHMARK_VERSION)
    set(BMD_P6_GOOGLE_BENCHMARK_VERSION "unavailable")
  file(
      GLOB _generator_version_files
      "${_conan_output_dir}/build/${CMAKE_BUILD_TYPE}/generators/*config-version.cmake"
  )
  foreach(_version_file IN LISTS _generator_version_files)
    get_filename_component(_version_file_name "${_version_file}" NAME)
    if(_version_file_name MATCHES "^benchmark-config-version.cmake$")
      file(READ "${_version_file}" _version_text)
      if(_version_text MATCHES "set\\(PACKAGE_VERSION \"([0-9.]+)\"\\)")
        set(BMD_P6_GOOGLE_BENCHMARK_VERSION "${CMAKE_MATCH_1}")
      endif()
    endif()
  endforeach()
  endif()

  if(BMD_PROJECTION_BUILD_PROTO_ADAPTER)
    set(BMD_P6_ADAPTER_ENABLED "ON")
    set(BMD_P6_CONTRACTS_SOURCE_REVISION "${BinanceMarketDataContracts_CONTRACTS_SOURCE_REVISION}")
    set(BMD_P6_CONTRACTS_CONAN_REFERENCE "binance-market-data-contracts-cpp/0.1.0")
    if(NOT DEFINED BMD_P6_CONTRACTS_RECIPE_REVISION)
      set(BMD_P6_CONTRACTS_RECIPE_REVISION "unavailable")
    endif()
    set(BMD_P6_PROTOBUF_RUNTIME_VERSION "${BinanceMarketDataContracts_PROTOBUF_RUNTIME_VERSION}")
    set(BMD_P6_PROTOBUF_RUNTIME_RREV "${BinanceMarketDataContracts_PROTOBUF_RUNTIME_RREV}")
  else()
    set(BMD_P6_ADAPTER_ENABLED "OFF")
    set(BMD_P6_CONTRACTS_SOURCE_REVISION "not_applicable")
    set(BMD_P6_CONTRACTS_CONAN_REFERENCE "not_applicable")
    set(BMD_P6_CONTRACTS_RECIPE_REVISION "not_applicable")
    set(BMD_P6_CONTRACTS_PACKAGE_ID "not_applicable")
    set(BMD_P6_PROTOBUF_RUNTIME_VERSION "not_applicable")
    set(BMD_P6_PROTOBUF_RUNTIME_RREV "not_applicable")
  endif()

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
  configure_file(
      "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/benchmark_build_identity.hpp.in"
      "${CMAKE_CURRENT_BINARY_DIR}/generated/benchmark_build_identity.hpp"
      @ONLY
  )
endfunction()

find_package(benchmark CONFIG REQUIRED)

include("${PROJECT_SOURCE_DIR}/cmake/M5Support.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/M5BenchmarkSupport.cmake")
bmd_projection_generate_benchmark_build_identity()

add_executable(
    bmd_projection_benchmarks
    foundation_benchmark.cpp
    benchmark_main.cpp
    m1_benchmarks.cpp
    m2_benchmarks.cpp
    m3_benchmarks.cpp
    replay_benchmarks.cpp
    benchmark_support/replay_workload_specs.cpp
)
target_link_libraries(
    bmd_projection_benchmarks
    PRIVATE
        BinanceMarketDataProjection::Core
        bmd_projection_m5_benchmark_support
        bmd_projection_m5_phase3_support
        bmd_projection_m5_oracle_support
        bmd_projection_m5_reference_support
        benchmark::benchmark
)
set_target_properties(
    bmd_projection_benchmarks
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_benchmarks)

if(BMD_PROJECTION_BUILD_PROTO_ADAPTER)
    target_sources(
        bmd_projection_benchmarks
        PRIVATE
            m4_adapter_benchmarks.cpp
            benchmark_support/adapter_wire_support.cpp
            benchmark_support/adapter_replay_executor.cpp
            benchmark_support/m4_workload_specs.cpp
            "${PROJECT_SOURCE_DIR}/tests/m5/oracle/adapter_production_side.cpp"
    )
    target_compile_definitions(
        bmd_projection_benchmarks
        PRIVATE BMD_PROJECTION_PHASE6_ADAPTER_ENABLED=1
    )
    target_link_libraries(
        bmd_projection_benchmarks
        PRIVATE BinanceMarketDataProjection::ProtoAdapter
    )
endif()

add_executable(
    bmd_projection_replay_latency
    replay_latency_main.cpp
)
target_link_libraries(
    bmd_projection_replay_latency
    PRIVATE
        BinanceMarketDataProjection::Core
        bmd_projection_m5_benchmark_support
        bmd_projection_m5_phase3_support
)
set_target_properties(
    bmd_projection_replay_latency
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_replay_latency)

# ---------------------------------------------------------------------------
# M5 Phase-7 allocation measurement executables (OD-M5-P7-002/008/009/010).
# Each executable owns the replacement global new/delete surface by compiling
# allocation_global_new.cpp and the measurement harness directly (never via a
# library), so the replacement definitions and their users (the harness)
# cannot leak into Core, ProtoAdapter, the Phase-6 benchmark executable, or
# normal consumers — including in shared-library builds. The provenance
# capacity is sized per executable from the actual prepared-state live sets:
# the M2/M3 pools hold up to ~2.05M live map nodes (insert/delete pools at
# depth 1000: 1024 books x 2000 nodes), so 1<<22 slots with ~2x headroom.
# ---------------------------------------------------------------------------
function(bmd_projection_add_allocation_executable target)
    add_executable(${target} ${ARGN})
    target_sources(
        ${target}
        PRIVATE
            "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/allocation_instrumentation.cpp"
            "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/allocation_global_new.cpp"
            "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support/phase7_harness.cpp"
    )
    target_include_directories(
        ${target}
        PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/benchmark_support"
    )
    target_link_libraries(
        ${target}
        PRIVATE
            BinanceMarketDataProjection::Core
            bmd_projection_m5_benchmark_support
    )
    set_target_properties(
        ${target}
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    bmd_projection_apply_project_options(${target})
endfunction()

bmd_projection_add_allocation_executable(
    bmd_projection_allocation_m2_m3
    allocation_m2_m3_main.cpp
)
target_compile_definitions(
    bmd_projection_allocation_m2_m3
    PRIVATE BMD_P7_PROVENANCE_CAPACITY=4194304
)

bmd_projection_add_allocation_executable(
    bmd_projection_allocation_footprint
    allocation_footprint_main.cpp
)
target_compile_definitions(
    bmd_projection_allocation_footprint
    PRIVATE BMD_P7_PROVENANCE_CAPACITY=1048576
)

bmd_projection_add_allocation_executable(
    bmd_projection_allocation_replay
    allocation_replay_main.cpp
    benchmark_support/replay_workload_specs.cpp
)
target_compile_definitions(
    bmd_projection_allocation_replay
    PRIVATE BMD_P7_PROVENANCE_CAPACITY=1048576
)
target_link_libraries(
    bmd_projection_allocation_replay
    PRIVATE
        bmd_projection_m5_phase3_support
        bmd_projection_m5_oracle_support
        bmd_projection_m5_reference_support
)
if(BMD_PROJECTION_BUILD_PROTO_ADAPTER)
    target_sources(
        bmd_projection_allocation_replay
        PRIVATE
            benchmark_support/adapter_wire_support.cpp
            benchmark_support/adapter_replay_executor.cpp
            "${PROJECT_SOURCE_DIR}/tests/m5/oracle/adapter_production_side.cpp"
    )
    target_compile_definitions(
        bmd_projection_allocation_replay
        PRIVATE BMD_PROJECTION_PHASE6_ADAPTER_ENABLED=1
    )
    target_link_libraries(
        bmd_projection_allocation_replay
        PRIVATE BinanceMarketDataProjection::ProtoAdapter
    )
endif()

if(BMD_PROJECTION_BUILD_PROTO_ADAPTER)
    bmd_projection_add_allocation_executable(
        bmd_projection_allocation_m4
        allocation_m4_main.cpp
        benchmark_support/adapter_wire_support.cpp
        benchmark_support/m4_workload_specs.cpp
    )
    target_compile_definitions(
        bmd_projection_allocation_m4
        PRIVATE
            BMD_P7_PROVENANCE_CAPACITY=4194304
            BMD_PROJECTION_PHASE6_ADAPTER_ENABLED=1
    )
    target_link_libraries(
        bmd_projection_allocation_m4
        PRIVATE BinanceMarketDataProjection::ProtoAdapter
    )
endif()

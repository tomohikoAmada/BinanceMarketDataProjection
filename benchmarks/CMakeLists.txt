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

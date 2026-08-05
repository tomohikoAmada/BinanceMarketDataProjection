find_package(benchmark CONFIG REQUIRED)

add_executable(bmd_projection_benchmarks foundation_benchmark.cpp)
target_link_libraries(
    bmd_projection_benchmarks
    PRIVATE
        BinanceMarketDataProjection::Core
        benchmark::benchmark_main
)
set_target_properties(
    bmd_projection_benchmarks
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_benchmarks)

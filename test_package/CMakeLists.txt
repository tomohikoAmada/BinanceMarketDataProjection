cmake_minimum_required(VERSION 3.24)

project(BinanceMarketDataProjectionTestPackage LANGUAGES CXX)

find_package(
    BinanceMarketDataProjection
    0.1.0
    EXACT
    CONFIG
    REQUIRED
    COMPONENTS ProtoAdapter
)

add_executable(projection_test_package src/test_package.cpp)
target_compile_features(projection_test_package PRIVATE cxx_std_20)
target_link_libraries(
    projection_test_package
    PRIVATE BinanceMarketDataProjection::ProtoAdapter
)

cmake_minimum_required(VERSION 3.24)

project(BinanceMarketDataProjectionDownstreamConsumer LANGUAGES CXX)

find_package(BinanceMarketDataProjection CONFIG REQUIRED COMPONENTS Core)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE BinanceMarketDataProjection::Core)
set_target_properties(
    consumer
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

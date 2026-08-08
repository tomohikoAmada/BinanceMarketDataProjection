cmake_minimum_required(VERSION 3.24)

project(BinanceMarketDataProjectionProtoAdapterConsumer LANGUAGES CXX)

find_package(BinanceMarketDataProjection CONFIG REQUIRED COMPONENTS ProtoAdapter)

add_executable(proto_adapter_consumer main.cpp)
target_link_libraries(
    proto_adapter_consumer
    PRIVATE BinanceMarketDataProjection::ProtoAdapter
)
set_target_properties(
    proto_adapter_consumer
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

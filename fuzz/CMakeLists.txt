add_executable(
    bmd_projection_decimal_parser_fuzz
    EXCLUDE_FROM_ALL
    decimal_parser_fuzz.cpp
)

target_link_libraries(
    bmd_projection_decimal_parser_fuzz
    PRIVATE BinanceMarketDataProjection::Core
)

set_target_properties(
    bmd_projection_decimal_parser_fuzz
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

bmd_projection_apply_project_options(bmd_projection_decimal_parser_fuzz)
target_compile_options(
    bmd_projection_decimal_parser_fuzz
    PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
)
target_link_options(
    bmd_projection_decimal_parser_fuzz
    PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
)

add_executable(
    bmd_projection_order_book_fuzz
    EXCLUDE_FROM_ALL
    order_book_fuzz.cpp
)

target_link_libraries(
    bmd_projection_order_book_fuzz
    PRIVATE BinanceMarketDataProjection::Core
)

set_target_properties(
    bmd_projection_order_book_fuzz
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

bmd_projection_apply_project_options(bmd_projection_order_book_fuzz)
target_compile_options(
    bmd_projection_order_book_fuzz
    PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
)
target_link_options(
    bmd_projection_order_book_fuzz
    PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
)

add_executable(
    bmd_projection_book_projection_fuzz
    EXCLUDE_FROM_ALL
    book_projection_fuzz.cpp
)

target_link_libraries(
    bmd_projection_book_projection_fuzz
    PRIVATE BinanceMarketDataProjection::Core
)
target_include_directories(
    bmd_projection_book_projection_fuzz
    PRIVATE "${PROJECT_SOURCE_DIR}/tests/projection_state"
)

set_target_properties(
    bmd_projection_book_projection_fuzz
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

bmd_projection_apply_project_options(bmd_projection_book_projection_fuzz)
target_compile_options(
    bmd_projection_book_projection_fuzz
    PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
)
target_link_options(
    bmd_projection_book_projection_fuzz
    PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
)

# M5 Phase-5 differential replay fuzzer: reuses the same replay/oracle/reference
# source as the deterministic tests via cmake/M5Support.cmake.
include("${PROJECT_SOURCE_DIR}/cmake/M5Support.cmake")

add_executable(
    bmd_projection_replay_fuzz
    EXCLUDE_FROM_ALL
    replay_fuzz.cpp
    m5/replay_fuzz_decoder.cpp
    m5/replay_fuzz_fixture.cpp
)
target_link_libraries(
    bmd_projection_replay_fuzz
    PRIVATE
        bmd_projection_m5_replay_support
        bmd_projection_m5_oracle_support
        bmd_projection_m5_reference_support
        BinanceMarketDataProjection::Core
)
target_include_directories(
    bmd_projection_replay_fuzz
    PRIVATE
        "${PROJECT_SOURCE_DIR}/fuzz"
        "${PROJECT_SOURCE_DIR}/fuzz/m5"
        "${PROJECT_SOURCE_DIR}/tests/m5/oracle"
        "${PROJECT_SOURCE_DIR}/tests/m5/reference"
        "${PROJECT_SOURCE_DIR}/tests/projection_state"
        "${PROJECT_SOURCE_DIR}/tests/order_book"
)
set_target_properties(
    bmd_projection_replay_fuzz
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_replay_fuzz)
target_compile_options(
    bmd_projection_replay_fuzz
    PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
)
target_link_options(
    bmd_projection_replay_fuzz
    PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
)

if(BMD_PROJECTION_BUILD_PROTO_ADAPTER)
    # Adapter production side: compiled directly as a source in the fuzz target
    # to keep adapter-enabled differential fuzzing reachable without a separate
    # library target.
    target_sources(
        bmd_projection_replay_fuzz
        PRIVATE "${PROJECT_SOURCE_DIR}/tests/m5/oracle/adapter_production_side.cpp"
    )
    target_link_libraries(
        bmd_projection_replay_fuzz
        PRIVATE BinanceMarketDataProjection::ProtoAdapter
    )
    target_compile_definitions(
        bmd_projection_replay_fuzz
        PRIVATE BMD_PROJECTION_FUZZ_ADAPTER_ENABLED=1
    )
endif()

add_executable(
    bmd_projection_proto_adapter_fuzz
    EXCLUDE_FROM_ALL
    proto_adapter_fuzz.cpp
)
    target_link_libraries(
        bmd_projection_proto_adapter_fuzz
        PRIVATE BinanceMarketDataProjection::ProtoAdapter
    )
    target_include_directories(
        bmd_projection_proto_adapter_fuzz
        PRIVATE "${PROJECT_SOURCE_DIR}/tests/projection_state"
    )
    set_target_properties(
        bmd_projection_proto_adapter_fuzz
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    bmd_projection_apply_project_options(bmd_projection_proto_adapter_fuzz)
    target_compile_options(
        bmd_projection_proto_adapter_fuzz
        PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
    )
    target_link_options(
        bmd_projection_proto_adapter_fuzz
        PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
    )
endif()

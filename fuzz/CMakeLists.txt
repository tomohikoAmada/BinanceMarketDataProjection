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

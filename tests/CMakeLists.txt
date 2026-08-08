find_package(GTest CONFIG REQUIRED)

add_executable(
    bmd_projection_tests
    numeric/decimal_format_test.cpp
    numeric/decimal_overflow_test.cpp
    numeric/decimal_parse_invalid_test.cpp
    numeric/decimal_parse_valid_test.cpp
    numeric/decimal_property_test.cpp
    numeric/decimal_roundtrip_test.cpp
    numeric/decimal_scale_test.cpp
    numeric/units_test.cpp
    order_book/book_side_test.cpp
    order_book/order_book_basic_test.cpp
    order_book/order_book_update_test.cpp
    order_book/order_book_batch_test.cpp
    order_book/order_book_replace_test.cpp
    order_book/order_book_query_test.cpp
    order_book/order_book_crossed_test.cpp
    order_book/order_book_determinism_test.cpp
    order_book/order_book_property_test.cpp
    projection_state/update_types_test.cpp
    projection_state/book_projection_test.cpp
    projection_state/book_projection_property_test.cpp
    projection_state/book_projection_replay_test.cpp
    public_header_test.cpp
    version_test.cpp
)

target_link_libraries(
    bmd_projection_tests
    PRIVATE
        BinanceMarketDataProjection::Core
        GTest::gtest_main
)

set_target_properties(
    bmd_projection_tests
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

bmd_projection_apply_project_options(bmd_projection_tests)

add_library(
    bmd_projection_public_header_self_containment
    OBJECT
    public_header_compile_test.cpp
    public_headers/numeric/decimal_error_compile_test.cpp
    public_headers/numeric/decimal_format_compile_test.cpp
    public_headers/numeric/decimal_parse_compile_test.cpp
    public_headers/numeric/decimal_scale_compile_test.cpp
    public_headers/numeric/numeric_spec_compile_test.cpp
    public_headers/numeric/price_units_compile_test.cpp
    public_headers/numeric/quantity_units_compile_test.cpp
    public_headers/order_book/book_side_compile_test.cpp
    public_headers/order_book/book_level_compile_test.cpp
    public_headers/order_book/level_update_compile_test.cpp
    public_headers/order_book/order_book_compile_test.cpp
    public_headers/projection_state/book_projection_compile_test.cpp
)
target_link_libraries(
    bmd_projection_public_header_self_containment
    PRIVATE BinanceMarketDataProjection::Core
)
set_target_properties(
    bmd_projection_public_header_self_containment
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_public_header_self_containment)

if(BMD_PROJECTION_BUILD_PROTO_ADAPTER)
    add_executable(
        bmd_projection_proto_adapter_tests
        proto_adapter/proto_adapter_test.cpp
        proto_adapter/proto_adapter_property_test.cpp
    )
    target_link_libraries(
        bmd_projection_proto_adapter_tests
        PRIVATE
            BinanceMarketDataProjection::ProtoAdapter
            GTest::gtest_main
    )
    set_target_properties(
        bmd_projection_proto_adapter_tests
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    bmd_projection_apply_project_options(bmd_projection_proto_adapter_tests)

    add_executable(
        bmd_projection_proto_adapter_allocation_failure_tests
        proto_adapter/proto_adapter_allocation_failure_test.cpp
    )
    target_link_libraries(
        bmd_projection_proto_adapter_allocation_failure_tests
        PRIVATE
            BinanceMarketDataProjection::ProtoAdapter
            GTest::gtest_main
    )
    set_target_properties(
        bmd_projection_proto_adapter_allocation_failure_tests
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    bmd_projection_apply_project_options(bmd_projection_proto_adapter_allocation_failure_tests)

    add_library(
        bmd_projection_proto_adapter_header_self_containment
        OBJECT
        public_headers/proto_adapter/proto_adapter_compile_test.cpp
    )
    target_link_libraries(
        bmd_projection_proto_adapter_header_self_containment
        PRIVATE BinanceMarketDataProjection::ProtoAdapter
    )
    set_target_properties(
        bmd_projection_proto_adapter_header_self_containment
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    bmd_projection_apply_project_options(bmd_projection_proto_adapter_header_self_containment)

    add_test(
        NAME m4_contracts_metadata_valid
        COMMAND ${CMAKE_COMMAND}
            -DCASE=valid
            -DVALIDATOR=${PROJECT_SOURCE_DIR}/cmake/ValidateContractsMetadata.cmake
            -P ${PROJECT_SOURCE_DIR}/tests/cmake/contracts_metadata_case.cmake
    )
    foreach(_case IN ITEMS baseline fingerprint algorithm source package_version
                           package_revision protoc runtime rrev flavor linkage missing)
        add_test(
            NAME m4_contracts_metadata_rejects_${_case}
            COMMAND ${CMAKE_COMMAND}
                -DCASE=${_case}
                -DVALIDATOR=${PROJECT_SOURCE_DIR}/cmake/ValidateContractsMetadata.cmake
                -P ${PROJECT_SOURCE_DIR}/tests/cmake/contracts_metadata_case.cmake
        )
        set_tests_properties(m4_contracts_metadata_rejects_${_case} PROPERTIES WILL_FAIL TRUE)
    endforeach()
    add_test(
        NAME m4_conan_lock_identity
        COMMAND ${CMAKE_COMMAND}
            -DLOCKFILE=${PROJECT_SOURCE_DIR}/conan.lock
            -P ${PROJECT_SOURCE_DIR}/tests/cmake/check_m4_lock.cmake
    )
endif()

add_executable(
    bmd_projection_m3_allocation_failure_tests
    projection_state/book_projection_allocation_failure_test.cpp
)
target_link_libraries(
    bmd_projection_m3_allocation_failure_tests
    PRIVATE
        BinanceMarketDataProjection::Core
        GTest::gtest_main
)
set_target_properties(
    bmd_projection_m3_allocation_failure_tests
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_m3_allocation_failure_tests)

include(GoogleTest)
gtest_discover_tests(bmd_projection_tests)
gtest_discover_tests(bmd_projection_m3_allocation_failure_tests)
if(BMD_PROJECTION_BUILD_PROTO_ADAPTER)
    gtest_discover_tests(bmd_projection_proto_adapter_tests)
    gtest_discover_tests(bmd_projection_proto_adapter_allocation_failure_tests)
endif()

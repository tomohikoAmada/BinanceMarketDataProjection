find_package(GTest CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_library(
    bmd_projection_m5_replay_support
    m5/replay/canonical_text.cpp
    m5/replay/replay_fixture.cpp
    m5/replay/replay_manifest.cpp
    m5/replay/replay_parser.cpp
    m5/replay/replay_types.cpp
)
target_include_directories(
    bmd_projection_m5_replay_support
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/m5/replay
)
set_target_properties(
    bmd_projection_m5_replay_support
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_m5_replay_support)

add_library(
    bmd_projection_m5_phase3_support
    m5/phase3/medium_validity.cpp
    m5/phase3/small_workload.cpp
)
target_include_directories(
    bmd_projection_m5_phase3_support
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/m5/phase3
)
target_link_libraries(
    bmd_projection_m5_phase3_support
    PUBLIC bmd_projection_m5_oracle_support
)
set_target_properties(
    bmd_projection_m5_phase3_support
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_m5_phase3_support)

add_executable(
    bmd_projection_m5_replay_tests
    m5/replay/replay_fixture_test.cpp
    m5/replay/replay_manifest_test.cpp
    m5/replay/replay_parser_test.cpp
)
target_compile_definitions(
    bmd_projection_m5_replay_tests
    PRIVATE BMD_M5_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/m5/fixtures/replay_v1"
)
target_link_libraries(
    bmd_projection_m5_replay_tests
    PRIVATE
        bmd_projection_m5_replay_support
        GTest::gtest_main
)
set_target_properties(
    bmd_projection_m5_replay_tests
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_m5_replay_tests)

# M5 Phase-2 reference layers: R1 (reference decimal), R4 (reference adapter), and
# the reference pipeline side composing R1/R3/R4. R2 (ReferenceOrderBook) and R3
# (ReferenceProjection) are reused in place from their reviewed M2/M3 test homes.
add_library(
    bmd_projection_m5_reference_support
    m5/reference/reference_adapter.cpp
    m5/reference/reference_decimal.cpp
    m5/oracle/reference_side.cpp
)
target_include_directories(
    bmd_projection_m5_reference_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/m5/reference
        ${CMAKE_CURRENT_SOURCE_DIR}/m5/oracle
        ${CMAKE_CURRENT_SOURCE_DIR}/projection_state
        ${CMAKE_CURRENT_SOURCE_DIR}/order_book
)
target_link_libraries(
    bmd_projection_m5_reference_support
    PUBLIC bmd_projection_m5_replay_support
)
set_target_properties(
    bmd_projection_m5_reference_support
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_m5_reference_support)

# M5 Phase-2 oracle support: canonical observation model, first-divergence
# comparison, the neutral ReplayDriver, and the Core-only production side. This
# target links Core but never ProtoAdapter/Contracts.
add_library(
    bmd_projection_m5_oracle_support
    m5/oracle/core_production_side.cpp
    m5/oracle/divergence.cpp
    m5/oracle/production_decimal_observation.cpp
    m5/oracle/replay_driver.cpp
)
target_include_directories(
    bmd_projection_m5_oracle_support
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/m5/oracle
)
target_link_libraries(
    bmd_projection_m5_oracle_support
    PUBLIC
        bmd_projection_m5_replay_support
        BinanceMarketDataProjection::Core
)
set_target_properties(
    bmd_projection_m5_oracle_support
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_m5_oracle_support)

add_executable(
    bmd_projection_m5_differential_tests
    m5/oracle/differential_replay_test.cpp
    m5/oracle/fault_injection_test.cpp
    m5/oracle/operation_observation_test.cpp
    m5/oracle/reference_adapter_test.cpp
    m5/oracle/reference_decimal_test.cpp
    m5/phase3/medium_validity_test.cpp
    m5/phase3/small_replay_test.cpp
)
target_compile_definitions(
    bmd_projection_m5_differential_tests
    PRIVATE BMD_M5_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/m5/fixtures/replay_v1"
)
target_link_libraries(
    bmd_projection_m5_differential_tests
    PRIVATE
        bmd_projection_m5_oracle_support
        bmd_projection_m5_phase3_support
        bmd_projection_m5_reference_support
        GTest::gtest_main
)
set_target_properties(
    bmd_projection_m5_differential_tests
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_m5_differential_tests)

add_executable(
    bmd_projection_m5_corpus_validate
    m5/phase3/corpus_validate.cpp
)
target_link_libraries(
    bmd_projection_m5_corpus_validate
    PRIVATE
        bmd_projection_m5_oracle_support
        bmd_projection_m5_phase3_support
        bmd_projection_m5_reference_support
)
set_target_properties(
    bmd_projection_m5_corpus_validate
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
bmd_projection_apply_project_options(bmd_projection_m5_corpus_validate)

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

    add_executable(
        bmd_projection_m5_adapter_differential_tests
        m5/oracle/adapter_differential_test.cpp
        m5/oracle/adapter_production_side.cpp
        m5/phase3/medium_validity_adapter_test.cpp
        m5/phase3/small_adapter_replay_test.cpp
    )
    target_compile_definitions(
        bmd_projection_m5_adapter_differential_tests
        PRIVATE BMD_M5_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/m5/fixtures/replay_v1"
    )
    target_link_libraries(
        bmd_projection_m5_adapter_differential_tests
        PRIVATE
            BinanceMarketDataProjection::ProtoAdapter
            bmd_projection_m5_oracle_support
            bmd_projection_m5_phase3_support
            bmd_projection_m5_reference_support
            GTest::gtest_main
    )
    set_target_properties(
        bmd_projection_m5_adapter_differential_tests
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    bmd_projection_apply_project_options(bmd_projection_m5_adapter_differential_tests)

    add_executable(
        bmd_projection_m5_adapter_corpus_validate
        m5/phase3/adapter_corpus_validate.cpp
        m5/oracle/adapter_production_side.cpp
    )
    target_link_libraries(
        bmd_projection_m5_adapter_corpus_validate
        PRIVATE
            BinanceMarketDataProjection::ProtoAdapter
            bmd_projection_m5_oracle_support
            bmd_projection_m5_phase3_support
            bmd_projection_m5_reference_support
    )
    set_target_properties(
        bmd_projection_m5_adapter_corpus_validate
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
    bmd_projection_apply_project_options(bmd_projection_m5_adapter_corpus_validate)
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
gtest_discover_tests(bmd_projection_m5_replay_tests)
gtest_discover_tests(bmd_projection_m5_differential_tests)
add_test(
    NAME m5_materializer_tests
    COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_CURRENT_SOURCE_DIR}/m5/materializer
            -p test_*.py
)
set_tests_properties(
    m5_materializer_tests
    PROPERTIES
        ENVIRONMENT
            "PYTHONPATH=${PROJECT_SOURCE_DIR};BMD_M5_CORPUS_VALIDATOR=$<TARGET_FILE:bmd_projection_m5_corpus_validate>"
)
if(BMD_PROJECTION_BUILD_PROTO_ADAPTER)
    set_property(
        TEST m5_materializer_tests
        APPEND PROPERTY ENVIRONMENT
            "BMD_M5_ADAPTER_CORPUS_VALIDATOR=$<TARGET_FILE:bmd_projection_m5_adapter_corpus_validate>"
    )
    gtest_discover_tests(bmd_projection_proto_adapter_tests)
    gtest_discover_tests(bmd_projection_proto_adapter_allocation_failure_tests)
    gtest_discover_tests(bmd_projection_m5_adapter_differential_tests)
endif()

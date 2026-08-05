find_package(GTest CONFIG REQUIRED)

add_executable(
    bmd_projection_tests
    version_test.cpp
    public_header_test.cpp
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

include(GoogleTest)
gtest_discover_tests(bmd_projection_tests)

# M5 replay/oracle/reference support targets shared between test and fuzz builds.
# Defines the libraries once; can be included by tests/CMakeLists.txt and
# fuzz/CMakeLists.txt. Targets are protected from double-definition.

if(NOT TARGET bmd_projection_m5_replay_support)
  add_library(
      bmd_projection_m5_replay_support
      "${PROJECT_SOURCE_DIR}/tests/m5/replay/canonical_text.cpp"
      "${PROJECT_SOURCE_DIR}/tests/m5/replay/replay_fixture.cpp"
      "${PROJECT_SOURCE_DIR}/tests/m5/replay/replay_manifest.cpp"
      "${PROJECT_SOURCE_DIR}/tests/m5/replay/replay_parser.cpp"
      "${PROJECT_SOURCE_DIR}/tests/m5/replay/replay_types.cpp"
  )
  target_include_directories(
      bmd_projection_m5_replay_support
      PUBLIC "${PROJECT_SOURCE_DIR}/tests/m5/replay"
  )
  set_target_properties(
      bmd_projection_m5_replay_support
      PROPERTIES
          CXX_STANDARD 20
          CXX_STANDARD_REQUIRED YES
          CXX_EXTENSIONS NO
  )
  bmd_projection_apply_project_options(bmd_projection_m5_replay_support)
endif()

if(NOT TARGET bmd_projection_m5_oracle_support)
  add_library(
      bmd_projection_m5_oracle_support
      "${PROJECT_SOURCE_DIR}/tests/m5/oracle/core_production_side.cpp"
      "${PROJECT_SOURCE_DIR}/tests/m5/oracle/divergence.cpp"
      "${PROJECT_SOURCE_DIR}/tests/m5/oracle/production_decimal_observation.cpp"
      "${PROJECT_SOURCE_DIR}/tests/m5/oracle/replay_driver.cpp"
  )
  target_include_directories(
      bmd_projection_m5_oracle_support
      PUBLIC "${PROJECT_SOURCE_DIR}/tests/m5/oracle"
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
endif()

if(NOT TARGET bmd_projection_m5_reference_support)
  add_library(
      bmd_projection_m5_reference_support
      "${PROJECT_SOURCE_DIR}/tests/m5/reference/reference_adapter.cpp"
      "${PROJECT_SOURCE_DIR}/tests/m5/reference/reference_decimal.cpp"
      "${PROJECT_SOURCE_DIR}/tests/m5/oracle/reference_side.cpp"
  )
  target_include_directories(
      bmd_projection_m5_reference_support
      PUBLIC
          "${PROJECT_SOURCE_DIR}/tests/m5/reference"
          "${PROJECT_SOURCE_DIR}/tests/m5/oracle"
          "${PROJECT_SOURCE_DIR}/tests/projection_state"
          "${PROJECT_SOURCE_DIR}/tests/order_book"
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
endif()

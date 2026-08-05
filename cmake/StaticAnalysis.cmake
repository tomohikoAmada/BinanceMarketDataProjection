function(bmd_projection_enable_static_analysis target)
    if(NOT BMD_PROJECTION_ENABLE_CLANG_TIDY)
        return()
    endif()

    find_program(BMD_PROJECTION_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    set_target_properties(
        ${target}
        PROPERTIES CXX_CLANG_TIDY
                   "${BMD_PROJECTION_CLANG_TIDY_EXECUTABLE};--config-file=${PROJECT_SOURCE_DIR}/.clang-tidy"
    )
endfunction()

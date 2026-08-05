function(bmd_projection_set_warnings target)
    if(NOT BMD_PROJECTION_ENABLE_WARNINGS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wshadow
                -Wconversion
                -Wsign-conversion
                -Wformat=2
                -Wundef
                -Wnon-virtual-dtor
                -Wold-style-cast
                -Woverloaded-virtual
        )
        if(BMD_PROJECTION_ENABLE_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(BMD_PROJECTION_ENABLE_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    endif()
endfunction()

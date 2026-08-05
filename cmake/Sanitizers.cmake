function(bmd_projection_enable_sanitizers target)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(BMD_PROJECTION_ENABLE_ASAN OR BMD_PROJECTION_ENABLE_UBSAN OR
           BMD_PROJECTION_ENABLE_TSAN)
            message(FATAL_ERROR "Configured sanitizers require GCC, Clang, or AppleClang")
        endif()
        return()
    endif()

    set(sanitizers "")
    if(BMD_PROJECTION_ENABLE_ASAN)
        list(APPEND sanitizers address)
    endif()
    if(BMD_PROJECTION_ENABLE_UBSAN)
        list(APPEND sanitizers undefined)
    endif()
    if(BMD_PROJECTION_ENABLE_TSAN)
        list(APPEND sanitizers thread)
    endif()

    if(sanitizers)
        list(JOIN sanitizers "," sanitizer_list)
        target_compile_options(
            ${target}
            PRIVATE -fsanitize=${sanitizer_list} -fno-omit-frame-pointer
        )
        target_link_options(
            ${target}
            PRIVATE -fsanitize=${sanitizer_list} -fno-omit-frame-pointer
        )
    endif()
endfunction()

file(READ "${LOCKFILE}" _lock)
foreach(_identity IN ITEMS
        "binance-market-data-contracts-cpp/0.1.0#53713fb82c27cf6ea0395f6b8a853006"
        "protobuf/6.33.5#ca5ff466767b31a1b496ec60247e105c")
    string(FIND "${_lock}" "${_identity}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Projection lock is missing ${_identity}")
    endif()
endforeach()

set(_bootstrap "${CMAKE_CURRENT_LIST_DIR}/../../scripts/bootstrap-contracts.sh")
file(READ "${_bootstrap}" _bootstrap_source)
foreach(_required_build_mode IN ITEMS
        "--build=missing"
        "--build='binance-market-data-contracts-cpp/*'")
    string(FIND "${_bootstrap_source}" "${_required_build_mode}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR
            "Contracts bootstrap is missing required create mode ${_required_build_mode}")
    endif()
endforeach()

foreach(_forbidden_identity IN ITEMS "grpc/" "re2/" "openssl/" "c-ares/")
    string(FIND "${_lock}" "${_forbidden_identity}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Projection message-only lock unexpectedly contains ${_forbidden_identity}")
    endif()
endforeach()

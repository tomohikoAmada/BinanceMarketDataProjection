file(READ "${LOCKFILE}" _lock)
foreach(_identity IN ITEMS
        "binance-market-data-contracts-cpp/0.1.0#53713fb82c27cf6ea0395f6b8a853006"
        "protobuf/6.33.5#ca5ff466767b31a1b496ec60247e105c")
    string(FIND "${_lock}" "${_identity}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Projection lock is missing ${_identity}")
    endif()
endforeach()

foreach(_forbidden_identity IN ITEMS "grpc/" "re2/" "openssl/" "c-ares/")
    string(FIND "${_lock}" "${_forbidden_identity}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR
            "Projection message-only lock unexpectedly contains ${_forbidden_identity}")
    endif()
endforeach()

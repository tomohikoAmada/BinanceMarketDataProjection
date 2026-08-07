file(READ "${LOCKFILE}" _lock)
foreach(_identity IN ITEMS
        "binance-market-data-contracts-cpp/0.1.0#7fd3efe3d289462fb16c78ffeced1682"
        "protobuf/6.33.5#ca5ff466767b31a1b496ec60247e105c")
    string(FIND "${_lock}" "${_identity}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "Projection lock is missing ${_identity}")
    endif()
endforeach()

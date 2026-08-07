function(bmd_projection_validate_contracts_metadata)
    set(_required_metadata
        SCHEMA_BASELINE
        SCHEMA_FINGERPRINT
        SCHEMA_FINGERPRINT_ALGORITHM_VERSION
        PACKAGE_VERSION
        PACKAGE_REVISION
        PROTOC_VERSION
        CPP_GENERATOR_OPTIONS
        PROTOBUF_RUNTIME_VERSION
        PROTOBUF_RUNTIME_RREV
        PROTOBUF_RUNTIME_COMPATIBILITY
        PROTOBUF_RUNTIME_FLAVOR
        PROTOBUF_RUNTIME_LINKAGE
        CONTRACTS_SOURCE_REVISION)
    foreach(_name IN LISTS _required_metadata)
        if(NOT DEFINED BinanceMarketDataContracts_${_name} OR
           BinanceMarketDataContracts_${_name} STREQUAL "")
            message(FATAL_ERROR "Contracts metadata ${_name} is missing")
        endif()
    endforeach()

    set(_expected_pairs
        SCHEMA_BASELINE "01d76a41929f36d89573159f5f458f9f1e378ada"
        SCHEMA_FINGERPRINT "33286fb1d624f4dd0c827010e93113f523c7f37dc4f6ae526361d2b0c61626c0"
        SCHEMA_FINGERPRINT_ALGORITHM_VERSION "1"
        PACKAGE_VERSION "0.1.0"
        PACKAGE_REVISION "NOT_FORMALLY_ASSIGNED"
        PROTOC_VERSION "libprotoc 33.5"
        CPP_GENERATOR_OPTIONS "cpp_out=dllexport_decl=BMD_CONTRACTS_PROTOBUF_API"
        PROTOBUF_RUNTIME_VERSION "6.33.5"
        PROTOBUF_RUNTIME_RREV "ca5ff466767b31a1b496ec60247e105c"
        PROTOBUF_RUNTIME_COMPATIBILITY "exactly 6.33.5 for this implementation candidate"
        PROTOBUF_RUNTIME_FLAVOR "full"
        CONTRACTS_SOURCE_REVISION "67ee1bf69fad980d114cfa278c3a6ffe310a4d7a")
    list(LENGTH _expected_pairs _pair_length)
    math(EXPR _pair_last "${_pair_length} - 1")
    foreach(_index RANGE 0 ${_pair_last} 2)
        math(EXPR _value_index "${_index} + 1")
        list(GET _expected_pairs ${_index} _name)
        list(GET _expected_pairs ${_value_index} _expected)
        if(NOT BinanceMarketDataContracts_${_name} STREQUAL _expected)
            message(FATAL_ERROR
                "Contracts metadata ${_name} mismatch: expected '${_expected}', got '${BinanceMarketDataContracts_${_name}}'")
        endif()
    endforeach()

    if(NOT BinanceMarketDataContracts_PROTOBUF_RUNTIME_LINKAGE STREQUAL "static" AND
       NOT BinanceMarketDataContracts_PROTOBUF_RUNTIME_LINKAGE STREQUAL "shared")
        message(FATAL_ERROR "Contracts metadata PROTOBUF_RUNTIME_LINKAGE is incompatible")
    endif()
endfunction()

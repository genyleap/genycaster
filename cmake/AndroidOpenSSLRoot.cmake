# Normalize Qt Creator's cached Android OpenSSL prefix.
#
# A PATH cache entry entered as "Users/name/..." (without leading slash) is
# silently absolutized by CMake relative to the checkout. For a macOS
# checkout under /Users/name/project this becomes
# /Users/name/project/Users/name/... . Repair only this recognizable case,
# and only if a complete target-ABI OpenSSL prefix exists at the candidate.
function(genycaster_resolve_android_openssl_root output requested source_root)
    if(IS_ABSOLUTE "${requested}")
        set(_root "${requested}")
    else()
        get_filename_component(_root "${requested}" ABSOLUTE
            BASE_DIR "${source_root}")
    endif()
    cmake_path(NORMAL_PATH _root)

    set(_source_prefix "${source_root}/")
    string(FIND "${_root}" "${_source_prefix}" _source_position)
    if(_source_position EQUAL 0
            AND NOT EXISTS "${_root}/include/openssl/opensslv.h")
        string(LENGTH "${_source_prefix}" _prefix_length)
        string(SUBSTRING "${_root}" ${_prefix_length} -1 _relative_tail)

        # These are known Unix absolute path roots, not an attempt
        # to reinterpret arbitrary relative paths as a host SDK location.
        if(_relative_tail MATCHES "^(Users|home|opt|tmp|var|mnt)/")
            set(_candidate "/${_relative_tail}")
            if(EXISTS "${_candidate}/include/openssl/opensslv.h"
                    AND EXISTS "${_candidate}/lib/libcrypto_3.so"
                    AND EXISTS "${_candidate}/lib/libssl_3.so")
                message(WARNING
                    "Corrected GENYCASTER_ANDROID_OPENSSL_ROOT from "
                    "'${_root}' to '${_candidate}'. "
                    "Update the Qt Creator Release kit CMake variable to "
                    "the absolute path with its leading '/'.")
                set(_root "${_candidate}")
                get_property(_cache_help CACHE GENYCASTER_ANDROID_OPENSSL_ROOT
                    PROPERTY HELPSTRING)
                set(GENYCASTER_ANDROID_OPENSSL_ROOT "${_root}" CACHE PATH
                    "${_cache_help}" FORCE)
            else()
                message(FATAL_ERROR
                    "GENYCASTER_ANDROID_OPENSSL_ROOT appears to have lost its "
                    "leading '/': '${_root}'. "
                    "Set the Qt Creator Release CMake variable to '${_candidate}'. "
                    "If its include/ and lib/ files do not exist, run "
                    "tools/build_android_openssl.sh for arm64-v8a first.")
            endif()
        endif()
    endif()

    set(${output} "${_root}" PARENT_SCOPE)
endfunction()

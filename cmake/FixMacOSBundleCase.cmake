# Normalize a pre-existing macOS bundle's physical directory spelling.
#
# OUTPUT_NAME=Genycaster already produces the canonical link path. On macOS
# case-insensitive volumes, an older genycaster.app directory may remain on
# disk under its original lowercase spelling even after rebuilding into the
# new spelling. A two-step rename is required for a reliable case-only change.
#
# This script runs only POST_BUILD for a macOS .app target, not Android, iOS
# or WebAssembly. It changes no executable, entitlements, identifiers or data.
if(NOT DEFINED GENYCASTER_BUNDLE_PATH)
    message(FATAL_ERROR "GENYCASTER_BUNDLE_PATH is required")
endif()

get_filename_component(_bundle_name "${GENYCASTER_BUNDLE_PATH}" NAME)
if(NOT _bundle_name STREQUAL "Genycaster.app")
    message(FATAL_ERROR "Unexpected GenyCaster bundle path: ${GENYCASTER_BUNDLE_PATH}")
endif()

get_filename_component(_parent "${GENYCASTER_BUNDLE_PATH}" DIRECTORY)
file(GLOB _actual_bundles LIST_DIRECTORIES TRUE RELATIVE "${_parent}" "${_parent}/*.app")
list(FIND _actual_bundles "genycaster.app" _legacy_index)
if(_legacy_index EQUAL -1)
    return()
endif()

list(FIND _actual_bundles "Genycaster.app" _canonical_index)
if(NOT _canonical_index EQUAL -1)
    # On case-sensitive volumes, both names are distinct. Never replace a
    # valid canonical bundle with a stale legacy one.
    message(STATUS "Genycaster.app exists; legacy genycaster.app is left untouched")
    return()
endif()

set(_legacy "${_parent}/genycaster.app")
set(_temporary "${_parent}/.Genycaster-case-rename.app")
if(EXISTS "${_temporary}" OR IS_SYMLINK "${_temporary}")
    message(FATAL_ERROR "Cannot normalize bundle; temporary path already exists: ${_temporary}")
endif()

file(RENAME "${_legacy}" "${_temporary}" RESULT _first_result)
if(NOT _first_result STREQUAL "0")
    message(FATAL_ERROR "Cannot rename legacy macOS bundle: ${_first_result}")
endif()

file(RENAME "${_temporary}" "${GENYCASTER_BUNDLE_PATH}" RESULT _second_result)
if(NOT _second_result STREQUAL "0")
    file(RENAME "${_temporary}" "${_legacy}" RESULT _rollback_result)
    message(FATAL_ERROR "Cannot normalize Genycaster.app casing: ${_second_result} (rollback: ${_rollback_result})")
endif()
message(STATUS "Normalized legacy macOS bundle directory to Genycaster.app")

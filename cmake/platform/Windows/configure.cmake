# Platform-scoped GenyCaster build behavior; not used on other targets.
# Windows vcpkg's shared Boost package defines BOOST_ALL_DYN_LINK. Our
# provider adapters instead compile one Boost.JSON implementation from
# boost/json/src.hpp into their own library targets. Do not mark the JSON
# functions as dllimport, or lld-link looks for a separately linked Boost.JSON
# DLL and fails when building the Snapchain adapters. Other Boost components
# retain their normal dynamic-link behavior.
if(WIN32)
    add_compile_definitions(BOOST_JSON_STATIC_LINK)
endif()

# Configure only the packaging boundary for the active target platform.
# The shared C++/Qt runtime remains composed by ui/CMakeLists.txt.
if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/Windows/Configure.cmake")
elseif(APPLE AND IOS)
    include("${CMAKE_CURRENT_LIST_DIR}/iOS/Configure.cmake")
elseif(APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/macOS/Configure.cmake")
elseif(ANDROID)
    include("${CMAKE_CURRENT_LIST_DIR}/Android/Configure.cmake")
elseif(UNIX AND NOT EMSCRIPTEN)
    include("${CMAKE_CURRENT_LIST_DIR}/Linux/Configure.cmake")
endif()

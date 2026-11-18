cmake_minimum_required(VERSION 3.24)
if(DEFINED QT_QMLFORMAT_EXECUTABLE AND EXISTS "${QT_QMLFORMAT_EXECUTABLE}")
    set(qmlformat "${QT_QMLFORMAT_EXECUTABLE}")
else()
    find_program(qmlformat NAMES qmlformat
        HINTS "$ENV{QT_HOST_PATH}/bin" "/opt/Qt/6.12.0/gcc_64/bin"
              "/opt/genycaster/Qt/6.12.0/gcc_64/bin")
endif()
if(NOT qmlformat)
    message(FATAL_ERROR "Qt qmlformat not found; set QT_QMLFORMAT_EXECUTABLE")
endif()
foreach(file IN ITEMS
    ui/components/apps/MiniAppSwapPreflight.qml
    ui/components/shell/AppShell.qml
    ui/components/profile/GenyProDetailsDrawer.qml
    ui/components/settings/SettingsPopup.qml
    ui/components/setup/InitialSetup.qml
    ui/components/feed/FeedSurface.qml
    ui/components/feed/DetailSurface.qml
    ui/components/content/RichCastRenderer.qml
    ui/components/home/StatsSkeleton.qml
    ui/components/feed/SourceClientBadge.qml
    ui/components/feed/PullToRefresh.qml
    ui/components/setup/SetupStepIndicator.qml)
    execute_process(COMMAND "${qmlformat}" "${CMAKE_CURRENT_LIST_DIR}/../${file}"
        RESULT_VARIABLE status ERROR_VARIABLE err OUTPUT_QUIET)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "QML syntax failed in ${file}: ${err}")
    endif()
endforeach()
message(STATUS "Critical QML parser verification passed")
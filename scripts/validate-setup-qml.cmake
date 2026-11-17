# Syntax-only check: Qt's QML formatter parses the files and writes the
# formatted result to captured stdout, leaving the sources untouched.
cmake_minimum_required(VERSION 3.24)
if(DEFINED QT_QMLFORMAT_EXECUTABLE AND EXISTS "${QT_QMLFORMAT_EXECUTABLE}")
    set(qmlformat "${QT_QMLFORMAT_EXECUTABLE}")
else()
    find_program(qmlformat NAMES qmlformat
        HINTS "/opt/Qt/6.12.0/gcc_64/bin"
              "/opt/Qt/current/gcc_64/bin" "$ENV{QT_HOST_PATH}/bin")
endif()
if(NOT qmlformat)
    message(FATAL_ERROR "Qt 6.12 qmlformat not found")
endif()
foreach(file IN ITEMS InitialSetup.qml SetupStepIndicator.qml SetupPrimaryButton.qml)
    execute_process(
        COMMAND "${qmlformat}"
            "${CMAKE_CURRENT_LIST_DIR}/../ui/components/setup/${file}"
        RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "QML parse failure in ${file}: ${stderr} ${stdout}")
    endif()
endforeach()
message(STATUS "Setup QML syntax parse: OK")

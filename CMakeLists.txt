cmake_minimum_required(VERSION 3.16)
project(lingmo-oobe VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(TS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/translations)
set(QM_FILE ${TS_DIR}/lingmo-oobe_zh_CN.qm)

# Try to compile .ts → .qm if lrelease is available
find_program(LRELEASE
    NAMES lrelease-qt6 lrelease
    PATHS /usr/lib/qt6/bin /usr/lib/x86_64-linux-gnu/qt6/bin
)
if(LRELEASE AND EXISTS ${TS_DIR}/lingmo-oobe_zh_CN.ts)
    execute_process(COMMAND ${LRELEASE} ${TS_DIR}/lingmo-oobe_zh_CN.ts
        -qm ${QM_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE LRELEASE_RESULT
    )
    if(LRELEASE_RESULT EQUAL 0)
        message(STATUS "Compiled translations: lingmo-oobe_zh_CN.ts → .qm")
    else()
        message(WARNING "lrelease failed to compile .ts file")
    endif()
endif()

# Embed .qm if available
if(EXISTS ${QM_FILE})
    qt6_add_resources(lingmo-oobe "translations"
        PREFIX "/i18n"
        FILES ${QM_FILE}
    )
else()
    message(STATUS "No .qm file found -- building without translations")
endif()

add_executable(lingmo-oobe
    main.cpp
    OobeWindow.cpp
)

target_link_libraries(lingmo-oobe
    Qt6::Widgets
)

install(TARGETS lingmo-oobe RUNTIME DESTINATION bin)
install(FILES oobe.desktop DESTINATION /etc/xdg/autostart)
install(FILES firstboot.service DESTINATION /lib/systemd/system)
install(PROGRAMS oobe.sh DESTINATION /usr/lib/lingmo-oobe)
install(FILES xsessions/lingmo-oobe.desktop DESTINATION /usr/share/xsessions)
install(FILES wayland-sessions/lingmo-oobe.desktop DESTINATION /usr/share/wayland-sessions)
install(FILES lingmo-oobe.rules DESTINATION /etc/polkit-1/rules.d RENAME 10-lingmo-oobe.rules)

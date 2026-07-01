cmake_minimum_required(VERSION 3.16)
project(lingmo-oobe VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/lingmo-oobe_zh_CN.ts
)

add_executable(lingmo-oobe
    main.cpp
    OobeWindow.cpp
)

target_link_libraries(lingmo-oobe
    Qt6::Widgets
)

qt6_add_translations(lingmo-oobe
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/i18n"
)

install(TARGETS lingmo-oobe RUNTIME DESTINATION bin)
install(FILES oobe.desktop DESTINATION /etc/xdg/autostart)
install(FILES firstboot.service DESTINATION /lib/systemd/system)
install(PROGRAMS oobe.sh DESTINATION /usr/lib/lingmo-oobe)
install(FILES xsessions/lingmo-oobe.desktop DESTINATION /usr/share/xsessions)
install(FILES wayland-sessions/lingmo-oobe.desktop DESTINATION /usr/share/wayland-sessions)
install(FILES lingmo-oobe.rules DESTINATION /etc/polkit-1/rules.d RENAME 10-lingmo-oobe.rules)

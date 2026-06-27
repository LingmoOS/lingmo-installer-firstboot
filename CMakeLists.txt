cmake_minimum_required(VERSION 3.16)
project(lingmo-oobe VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES
    translations/lingmo-oobe_zh_CN.ts
)

qt6_add_translations(lingmo-oobe
    TS_FILES
        ${TS_FILES}
    RESOURCE_PREFIX "/i18n"
)

add_executable(lingmo-oobe
    main.cpp
    OobeWindow.cpp
    ${QM_FILES}
)

target_link_libraries(lingmo-oobe
    Qt6::Widgets
)

install(TARGETS lingmo-oobe RUNTIME DESTINATION bin)
install(FILES oobe.desktop DESTINATION /etc/xdg/autostart)
install(FILES firstboot.service DESTINATION /lib/systemd/system)

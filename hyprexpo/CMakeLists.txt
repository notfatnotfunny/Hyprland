cmake_minimum_required(VERSION 3.27)

project(hyprexpo
    DESCRIPTION "hyprexpo plugin for Hyprland"
    VERSION 0.1
)

set(CMAKE_CXX_STANDARD 23)

# Only compile main.cpp, exclude main_fallback.cpp
set(SRC 
    main.cpp
    OverviewPassElement.cpp
    overview.cpp
)

add_library(hyprexpo SHARED ${SRC})

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
    libdrm
    libinput
    libudev
    pangocairo
    pixman-1
    wayland-server
    xkbcommon
)

# Add include directories for the hook system
target_include_directories(hyprexpo PRIVATE 
    ${CMAKE_SOURCE_DIR}/../src/plugins
    ${CMAKE_SOURCE_DIR}/../src
)

target_link_libraries(hyprexpo PRIVATE rt PkgConfig::deps)

install(TARGETS hyprexpo)

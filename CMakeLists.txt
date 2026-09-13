cmake_minimum_required(VERSION 3.16)
project(vibe_fi VERSION 1.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and warning flags
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type" FORCE)
    endif()
    add_compile_options("$<$<CONFIG:Release>:-O2>")
endif()

# macOS Homebrew include/library paths
if(APPLE)
    if(EXISTS "/opt/homebrew/include")
        include_directories(/opt/homebrew/include)
        link_directories(/opt/homebrew/lib)
    elseif(EXISTS "/usr/local/include")
        include_directories(/usr/local/include)
        link_directories(/usr/local/lib)
    endif()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED mpv)
pkg_check_modules(NCURSES REQUIRED ncurses)

# MPRIS / D-Bus support (enabled by default on Linux, disabled on macOS/Windows)
if(APPLE OR WIN32)
    option(ENABLE_MPRIS "Enable D-Bus MPRIS media key support" OFF)
else()
    option(ENABLE_MPRIS "Enable D-Bus MPRIS media key support" ON)
endif()

if(ENABLE_MPRIS)
    pkg_check_modules(DBUS dbus-1)
    if(DBUS_FOUND)
        message(STATUS "D-Bus found: MPRIS media controls enabled")
        add_compile_definitions(ENABLE_MPRIS=1)
    else()
        message(STATUS "D-Bus not found: MPRIS media controls disabled")
        set(ENABLE_MPRIS OFF)
    endif()
else()
    message(STATUS "MPRIS media controls disabled by configuration")
endif()

# Version definition for source files
add_compile_definitions(VIBE_FI_VERSION="${PROJECT_VERSION}")

# Source files organized by domain
set(SOURCES
    src/main.cpp
    src/core/player.cpp
    src/ui/ui.cpp
    src/ui/visualizer.cpp
    src/services/library.cpp
    src/services/lyrics.cpp
    src/services/playlist_manager.cpp
    src/services/search.cpp
    src/services/updater.cpp
    src/integrations/discord_rpc.cpp
    src/integrations/mpris.cpp
    src/utils/utils.cpp
)

add_executable(vibe_fi ${SOURCES})

target_include_directories(vibe_fi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${MPV_INCLUDE_DIRS}
    ${NCURSES_INCLUDE_DIRS}
)

target_link_libraries(vibe_fi PRIVATE
    ${MPV_LIBRARIES}
    ${NCURSES_LIBRARIES}
)

if(ENABLE_MPRIS AND DBUS_FOUND)
    target_include_directories(vibe_fi PRIVATE ${DBUS_INCLUDE_DIRS})
    target_link_libraries(vibe_fi PRIVATE ${DBUS_LIBRARIES})
endif()

find_package(Threads REQUIRED)
target_link_libraries(vibe_fi PRIVATE Threads::Threads)

# Installation targets
include(GNUInstallDirs)
install(TARGETS vibe_fi DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install convenience symlink 'vibe' -> 'vibe_fi'
install(CODE "
    execute_process(
        COMMAND \${CMAKE_COMMAND} -E create_symlink
        vibe_fi
        \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/vibe\"
    )
")

cmake_minimum_required(VERSION 3.10)
project(vibe_fi)

set(CMAKE_CXX_STANDARD 17)

if(APPLE)
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED mpv)
pkg_check_modules(NCURSES REQUIRED ncurses)

include_directories(${MPV_INCLUDE_DIRS} ${NCURSES_INCLUDE_DIRS} src)
link_directories(${MPV_LIBRARY_DIRS} ${NCURSES_LIBRARY_DIRS})

add_executable(vibe_fi
    src/main.cpp
    src/player.cpp
    src/ui.cpp
    src/utils.cpp
    src/search.cpp
    src/library.cpp
    src/playlist_manager.cpp
)

target_link_libraries(vibe_fi ${MPV_LIBRARIES} ${NCURSES_LIBRARIES})

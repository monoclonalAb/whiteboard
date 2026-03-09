cmake_minimum_required(VERSION 3.15)
project(whiteboard CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Curses REQUIRED)

add_executable(whiteboard
    src/main.cpp
    src/editor.cpp
    src/renderer.cpp
    src/serialization.cpp
)

target_include_directories(whiteboard PRIVATE src ${CURSES_INCLUDE_DIRS})
target_link_libraries(whiteboard PRIVATE ${CURSES_LIBRARIES})

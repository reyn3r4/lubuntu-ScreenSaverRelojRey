cmake_minimum_required(VERSION 3.16)

project(reloj_rey VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
find_package(X11 REQUIRED)

add_executable(reloj-rey
    src/main.cpp
    src/clockwidget.cpp
    src/clockwidget.h
)

target_link_libraries(reloj-rey PRIVATE Qt6::Widgets X11::X11)
target_compile_options(reloj-rey PRIVATE -Wall -Wextra -Wpedantic)

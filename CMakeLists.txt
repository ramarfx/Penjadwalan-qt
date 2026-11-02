cmake_minimum_required(VERSION 3.16)
project(PenjadwalanQt VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(penjadwalan-qt
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/coursedialog.cpp
    src/coursedialog.h
    src/lecturerdialog.cpp
    src/lecturerdialog.h
    src/roomdialog.cpp
    src/roomdialog.h
    src/scheduler_core.cpp
    src/scheduler_core.h
    src/jadwal_logic.h
)

target_link_libraries(penjadwalan-qt PRIVATE Qt6::Widgets)
target_compile_options(penjadwalan-qt PRIVATE -Wall -Wextra)

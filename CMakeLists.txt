cmake_minimum_required(VERSION 3.16)

project(QTNdiRecv VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Multimedia)
qt_standard_project_setup()

add_subdirectory(src)
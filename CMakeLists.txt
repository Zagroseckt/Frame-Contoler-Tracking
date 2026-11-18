cmake_minimum_required(VERSION 3.16)
project(SFTrackingViewer LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)

add_executable(tracking-viewer viewer/main.cpp)
target_compile_features(tracking-viewer PRIVATE cxx_std_17)
target_link_libraries(tracking-viewer PRIVATE PkgConfig::GTK3)
target_compile_options(tracking-viewer PRIVATE -Wall -Wextra -Wpedantic)

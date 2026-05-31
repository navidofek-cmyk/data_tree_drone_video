cmake_minimum_required(VERSION 3.16)
project(pointcloud_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)   # Qt meta-objekty (Q_OBJECT)

# Jen Widgets — 3D kreslíme softwarově přes QPainter (žádný OpenGL/GLX),
# aby viewer fungoval i přes vzdálené X11 (ssh -X / MobaXterm).
find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(viewer
    src/main.cpp
    src/PointCloudView.cpp
    src/MainWindow.cpp
)
target_link_libraries(viewer PRIVATE Qt5::Widgets)

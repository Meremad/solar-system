cmake_minimum_required(VERSION 3.10)
project(SolarSystem)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)


include_directories(
    include
    /opt/homebrew/include
    /opt/homebrew/opt/glfw/include
    /opt/homebrew/opt/glew/include
    ${CMAKE_SOURCE_DIR}/imgui
    ${CMAKE_SOURCE_DIR}/imgui/backends
)

link_directories(
    /opt/homebrew/lib
    /opt/homebrew/opt/glfw/lib
    /opt/homebrew/opt/glew/lib
)

set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(SolarSystem src/main.cpp ${IMGUI_SOURCES})

target_link_libraries(SolarSystem PRIVATE
    glfw
    ${OPENGL_LIBRARIES}
    /opt/homebrew/lib/libGLEW.dylib
    Threads::Threads
)


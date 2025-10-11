cmake_minimum_required(VERSION 3.10)
project(SolarSystem)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)

include_directories(
    include
    /opt/homebrew/include
    /opt/homebrew/opt/glfw/include
    /opt/homebrew/opt/glew/include
)


link_directories(
    /opt/homebrew/lib
    /opt/homebrew/opt/glfw/lib
    /opt/homebrew/opt/glew/lib
)

add_executable(SolarSystem src/main.cpp)

target_link_libraries(SolarSystem
    glfw
    ${OPENGL_LIBRARIES}
    /opt/homebrew/lib/libGLEW.dylib
)

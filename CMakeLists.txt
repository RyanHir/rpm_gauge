cmake_minimum_required(VERSION 3.22)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(gauge)

find_package(PkgConfig)
pkg_check_modules(GLFW3 REQUIRED glfw3)
find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)
find_package(Freetype REQUIRED)

add_library(glad glad/src/gl.c)
target_include_directories(glad PUBLIC glad/include)

add_library(imgui
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_opengl3.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui.cpp
)
target_link_libraries(imgui glad ${GLFW3_LIBRARIES} ${OPENGL_LIBRARIES})
target_link_directories(imgui PUBLIC ${GLFW3_LIBRARY_DIRS})
target_include_directories(imgui PUBLIC ${GLFW3_INCLUDE_DIRS} imgui)
target_compile_options(imgui PUBLIC ${GLFW3_CFLAGS_OTHER})
target_compile_features(imgui PUBLIC cxx_std_11)

add_executable(main main.cpp text_renderer.cpp)
target_compile_features(main PUBLIC cxx_std_20)

target_link_libraries(main PUBLIC imgui glm::glm glad ${GLFW3_LIBRARIES} ${OPENGL_LIBRARIES} ${FREETYPE_LIBRARIES})
target_link_directories(main PUBLIC ${GLFW3_LIBRARY_DIRS} ${FREETYPE_LIBRARY_DIRS})
target_include_directories(main PUBLIC ${GLFW3_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
target_compile_options(main PUBLIC ${GLFW3_CFLAGS_OTHER})
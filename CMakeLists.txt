cmake_minimum_required(VERSION 3.20)
project(spatial_navigator VERSION 0.2.0 LANGUAGES CXX)

find_package(SDL2 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp")
    message(FATAL_ERROR "Initialize Dear ImGui: git submodule update --init third_party/imgui")
endif()
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl2.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC third_party/imgui third_party/imgui/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2 PRIVATE ${CMAKE_DL_LIBS})

add_library(camera STATIC src/graphics/camera.cpp)
target_include_directories(camera PUBLIC src)
target_compile_features(camera PUBLIC cxx_std_20)
target_compile_options(camera PRIVATE -Wall -Wextra -Wpedantic)

add_library(mock-browser STATIC
    src/filesystem/mock_filesystem.cpp src/analysis/analysis.cpp
    src/layout/layout.cpp src/layout/hierarchy.cpp src/navigation/navigation.cpp
    src/graphics/picking.cpp src/ui/presentation.cpp src/app/browser.cpp)
target_link_libraries(mock-browser PUBLIC camera)
target_compile_options(mock-browser PRIVATE -Wall -Wextra -Wpedantic)

add_executable(spatial-navigator
    src/app/main.cpp src/graphics/renderer.cpp src/ui/overlay.cpp)
target_compile_features(spatial-navigator PRIVATE cxx_std_20)
target_compile_options(spatial-navigator PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(spatial-navigator PRIVATE mock-browser imgui OpenGL::GL)

include(CTest)
if(BUILD_TESTING)
    add_executable(camera-tests tests/camera_tests.cpp)
    target_link_libraries(camera-tests PRIVATE camera)
    add_test(NAME camera COMMAND camera-tests)
    add_executable(mock-tests tests/mock_tests.cpp)
    target_link_libraries(mock-tests PRIVATE mock-browser)
    target_compile_options(mock-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME mock-filesystem-analysis-layout-navigation COMMAND mock-tests)
endif()

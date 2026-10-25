cmake_minimum_required(VERSION 3.20)
project(spatial_navigator VERSION 0.5.0 LANGUAGES CXX)

find_package(SDL2 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp")
    message(FATAL_ERROR "Initialize Dear ImGui: git submodule update --init third_party/imgui")
endif()
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/misc/cpp/imgui_stdlib.cpp
    third_party/imgui/backends/imgui_impl_sdl2.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC third_party/imgui third_party/imgui/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2 PRIVATE ${CMAKE_DL_LIBS})

add_library(camera STATIC src/graphics/camera.cpp)
target_include_directories(camera PUBLIC src)
target_compile_features(camera PUBLIC cxx_std_20)
target_compile_options(camera PRIVATE -Wall -Wextra -Wpedantic)

add_library(browser STATIC
    src/filesystem/model.cpp src/filesystem/reader.cpp src/filesystem/mock_filesystem.cpp src/analysis/analysis.cpp src/analysis/query.cpp
    src/layout/layout.cpp src/layout/hierarchy.cpp src/navigation/navigation.cpp
    src/navigation/spatial_selection.cpp src/navigation/path.cpp src/navigation/history.cpp
    src/graphics/picking.cpp src/ui/presentation.cpp src/app/browser.cpp src/app/browser_navigation.cpp src/app/desktop.cpp)
target_link_libraries(browser PUBLIC camera Threads::Threads)
target_compile_options(browser PRIVATE -Wall -Wextra -Wpedantic)

add_executable(spatial-navigator
    src/app/main.cpp src/graphics/renderer.cpp src/ui/overlay.cpp)
target_compile_features(spatial-navigator PRIVATE cxx_std_20)
target_compile_options(spatial-navigator PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(spatial-navigator PRIVATE browser imgui OpenGL::GL)

include(CTest)
if(BUILD_TESTING)
    add_executable(camera-tests tests/camera_tests.cpp)
    target_link_libraries(camera-tests PRIVATE camera)
    add_test(NAME camera COMMAND camera-tests)
    add_executable(mock-tests tests/mock_tests.cpp)
    target_link_libraries(mock-tests PRIVATE browser)
    target_compile_options(mock-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME mock-filesystem-analysis-layout-navigation COMMAND mock-tests)
    add_executable(filesystem-tests tests/filesystem_tests.cpp)
    target_link_libraries(filesystem-tests PRIVATE browser)
    target_compile_options(filesystem-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME real-filesystem COMMAND filesystem-tests)
    set_tests_properties(real-filesystem PROPERTIES TIMEOUT 60)
    add_executable(analysis-tests tests/analysis_tests.cpp)
    target_link_libraries(analysis-tests PRIVATE browser)
    target_compile_options(analysis-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME salience-grouping-disclosure COMMAND analysis-tests)
    add_executable(browser-tests tests/browser_tests.cpp)
    target_link_libraries(browser-tests PRIVATE browser)
    target_compile_options(browser-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME practical-browser COMMAND browser-tests)
    set_tests_properties(practical-browser PROPERTIES TIMEOUT 60)
endif()

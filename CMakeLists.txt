cmake_minimum_required(VERSION 3.27)
project(mph_tetra)

option(ENABLE_NATIVE_OPTIMIZATION "Enable native CPU specific compiler optimizations" OFF)

if(ENABLE_NATIVE_OPTIMIZATION)
    add_compile_options(-march=native -mtune=native)
endif()


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SDL_VERSION 2)

find_package("SDL${SDL_VERSION}" REQUIRED)
find_package(OpenGL REQUIRED)

set(imgui_SRC
    util/imgui-1.91.1/imgui.cpp
    util/imgui-1.91.1/imgui_demo.cpp
    util/imgui-1.91.1/imgui_draw.cpp
    util/imgui-1.91.1/imgui_tables.cpp
    util/imgui-1.91.1/imgui_widgets.cpp
    util/imgui-1.91.1/misc/cpp/imgui_stdlib.cpp
    util/imgui-1.91.1/backends/imgui_impl_sdl${SDL_VERSION}.cpp
    util/imgui-1.91.1/backends/imgui_impl_opengl3.cpp
)

set(mph_tetra_SRC
    main.cpp
    util/misc.cpp
    util/styles.cpp
    util/convar.cpp
    util/console.cpp
    util/cli_parser.cpp
    util/gui_registrar.cpp
    util/overlay_loading.cpp
    util/overlay_performance.cpp
    ${imgui_SRC}
)

add_executable(mph_tetra ${mph_tetra_SRC})

target_include_directories(mph_tetra PUBLIC .)
target_compile_options(mph_tetra PUBLIC -Wall -Wextra)
target_link_libraries(mph_tetra ${OPENGL_LIBRARIES})

target_include_directories(mph_tetra PUBLIC ${SDL${SDL_VERSION}_INCLUDE_DIRS})
target_link_libraries(mph_tetra "SDL${SDL_VERSION}::SDL${SDL_VERSION}")

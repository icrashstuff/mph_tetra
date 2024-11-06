cmake_minimum_required(VERSION 3.27)

project(mph_tetra)

option(ENABLE_NATIVE_OPTIMIZATION "Enable native CPU specific compiler optimizations" OFF)

if(ENABLE_NATIVE_OPTIMIZATION AND NOT MSVC)
    add_compile_options(-march=native -mtune=native)
endif()

option(DISABLE_STACK_PROTECTOR "Useful for finding out where stack smashing is occurring (Debug builds only)" OFF )
if(DISABLE_STACK_PROTECTOR AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND NOT MSVC)
    message(WARNING "Disabling stack protector")
    add_compile_options(-fno-stack-protector)
endif()

# We don't necessarily have to disable most of the archivers but we also don't need them either
set(PHYSFS_BUILD_SHARED FALSE)
set(PHYSFS_ARCHIVE_GRP FALSE)
set(PHYSFS_ARCHIVE_WAD FALSE)
set(PHYSFS_ARCHIVE_CSM FALSE)
set(PHYSFS_ARCHIVE_HOG FALSE)
set(PHYSFS_ARCHIVE_MVL FALSE)
set(PHYSFS_ARCHIVE_QPAK FALSE)
set(PHYSFS_ARCHIVE_SLB FALSE)
set(PHYSFS_ARCHIVE_ISO9660 FALSE)
set(PHYSFS_ARCHIVE_VDF FALSE)
set(PHYSFS_ARCHIVE_LECARCHIVES FALSE)
add_subdirectory(util/physfs/physfs-74c3054/)

set(NFD_PORTAL TRUE)
add_subdirectory(util/nfd-388549a/)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SDL_VERSION 2)

find_package("SDL${SDL_VERSION}" REQUIRED)
find_package(OpenGL REQUIRED)

set(imgui_SRC
    gui/imgui-1.91.1/imgui.cpp
    gui/imgui-1.91.1/imgui_demo.cpp
    gui/imgui-1.91.1/imgui_draw.cpp
    gui/imgui-1.91.1/imgui_tables.cpp
    gui/imgui-1.91.1/imgui_widgets.cpp
    gui/imgui-1.91.1/misc/cpp/imgui_stdlib.cpp
    gui/imgui-1.91.1/backends/imgui_impl_sdl${SDL_VERSION}.cpp
    gui/imgui-1.91.1/backends/imgui_impl_opengl3.cpp
)

set(mph_tetra_SRC
    main.cpp
    
    gui/styles.cpp
    gui/console.cpp
    gui/file_picker.cpp
    gui/gui_registrar.cpp
    gui/imgui_extracts.cpp
    gui/physfs_browser.cpp
    gui/overlay_loading.cpp
    gui/overlay_performance.cpp
    
    util/nds.cpp
    util/misc.cpp
    util/convar.cpp
    util/cli_parser.cpp
    
    util/physfs/archiver_nds.cpp
    
    ${imgui_SRC}
)

add_executable(mph_tetra ${mph_tetra_SRC})

target_include_directories(mph_tetra PUBLIC .)
target_compile_options(mph_tetra PUBLIC -Wall -Wextra)
target_link_libraries(mph_tetra ${OPENGL_LIBRARIES})
target_link_libraries(mph_tetra PhysFS::PhysFS-static)
target_link_libraries(mph_tetra nfd::nfd)

target_include_directories(mph_tetra PUBLIC ${SDL${SDL_VERSION}_INCLUDE_DIRS})
target_link_libraries(mph_tetra "SDL${SDL_VERSION}::SDL${SDL_VERSION}")

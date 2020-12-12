cmake_minimum_required(VERSION 3.12)
project(cubeland)

# include all files in the CMake directory
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

# set global C++ options
set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(src)
include_directories(src/include)

# compile in the git version file
include(GetGitRevisionDescription)
git_describe(VERSION --tags --dirty=-d)

get_git_head_revision(GIT_REFSPEC GIT_HASH)

string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" VERSION_MAJOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" VERSION_MINOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" VERSION_PATCH "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.[0-9]+(.*)" "\\1" VERSION_SHA1 "${VERSION}")
set(VERSION_SHORT "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
set(version_file "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")

###################################################################################################
#### sources
# build main executable and all modules
add_executable(cubeland
    src/main.cpp
# logging
    src/logging/Logging.cpp
# io
    src/io/ConfigManager.cpp
    src/io/PrefsManager.cpp
    src/io/PathHelper.cpp
# UI
    src/gui/MainWindow.cpp
    src/gui/GameUI.cpp
# resources and other misc stuff
    ${version_file}
)

# platform-specific stuff
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_library(macos_support OBJECT
        src/io/PathHelper+Mac.mm
    )
    target_link_libraries(cubeland PRIVATE macos_support)
endif()

###################################################################################################
#### resources
include(CMakeRC)

# UI resources
cmrc_add_resource_library(cubeland-rsrc-ui
# fonts
    rsrc/fonts/SourceSansPro-Regular.ttf
    rsrc/fonts/SourceSansPro-Italic.ttf
    rsrc/fonts/SourceSansPro-Bold.ttf
    rsrc/fonts/SourceSansPro-BoldItalic.ttf
    rsrc/fonts/SpaceMono-Regular.ttf
    rsrc/fonts/SpaceMono-Bold.ttf
# define the namespace and alias we put this lib in
ALIAS cubeland::rsrc_ui WHENCE rsrc NAMESPACE ui)

target_link_libraries(cubeland PRIVATE cubeland::rsrc_ui)

###################################################################################################
#### libraries
# do NOT build shared libraries. thanks DLL hell
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# glbinding (loading OpenGL functions)
find_package(glbinding REQUIRED)
target_link_libraries(cubeland PUBLIC glbinding::glbinding)

# SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
target_link_libraries(cubeland PUBLIC ${SDL2_LIBRARIES})

# SQLite 3 (data file format)
find_package(SQLite3 REQUIRED)
target_link_libraries(cubeland PRIVATE SQLite::SQLite3)

# Lyra (command line parsing)
add_subdirectory(libs/Lyra)
target_link_libraries(cubeland PUBLIC bfg::Lyra)

# libfmt (modern C++ string formatting)
add_subdirectory(libs/fmt ${CMAKE_CURRENT_BINARY_DIR}/libs/fmt)
target_link_libraries(cubeland PRIVATE fmt::fmt)

# libconfig (reading configuration files)
set(BUILD_EXAMPLES OFF CACHE BOOL "Enable examples" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Enable tests" FORCE)
add_subdirectory(libs/libconfig ${CMAKE_CURRENT_BINARY_DIR}/libs/libconfig)

target_include_directories(cubeland PRIVATE libs/libconfig/lib)
target_link_libraries(cubeland PRIVATE config++)

# spdlog (logging to console/file)
find_package(spdlog REQUIRED)
target_link_libraries(cubeland PRIVATE spdlog::spdlog)

# Dear IMGui (graphics library for in-game UI)
add_library(dearimgui OBJECT
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/imgui_tables.cpp

    # use the generic OpenGL/SDL backends
    libs/imgui/backends/imgui_impl_sdl.cpp
    libs/imgui/backends/imgui_impl_opengl3.cpp
    )
target_include_directories(dearimgui PUBLIC ${GLEW_INCLUDE_DIRS})

target_include_directories(dearimgui PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(dearimgui PRIVATE src/gui/imgui)
target_include_directories(dearimgui PRIVATE libs/imgui)

target_link_libraries(dearimgui PUBLIC glbinding::glbinding)

target_include_directories(cubeland PRIVATE src/gui/imgui)
target_include_directories(cubeland PRIVATE libs/imgui)
target_link_libraries(cubeland PRIVATE dearimgui)

# Dear IMGui extensions
add_library(dearimgui_ext OBJECT
    # file browser dialog
    libs/ImGuiFileDialog/ImGuiFileDialog/ImGuiFileDialog.cpp
)

target_include_directories(dearimgui_ext PRIVATE src/gui/imgui)
target_include_directories(dearimgui_ext PRIVATE libs/imgui)

target_link_libraries(dearimgui_ext PUBLIC glbinding::glbinding)
target_link_libraries(cubeland PRIVATE dearimgui_ext)

###################################################################################################
#### platform-specific libraries
# macOS frameworks
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(cubeland PUBLIC "-framework Cocoa")
    target_link_libraries(cubeland PUBLIC "-framework ApplicationServices")
endif()

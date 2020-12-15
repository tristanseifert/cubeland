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
    src/gui/PreferencesWindow.cpp
# input
    src/input/InputManager.cpp
# GL wrappers
    src/gfx/gl/texture/Texture.cpp
    src/gfx/gl/texture/Texture2D.cpp
    src/gfx/gl/texture/TextureCube.cpp
    src/gfx/gl/texture/TextureDumper.cpp
    src/gfx/gl/program/Shader.cpp
    src/gfx/gl/program/ShaderProgram.cpp
    src/gfx/gl/buffer/Buffer.cpp
    src/gfx/gl/buffer/FrameBuffer.cpp
    src/gfx/gl/buffer/PixelBuffer.cpp
    src/gfx/gl/buffer/RenderBuffer.cpp
    src/gfx/gl/buffer/VertexArray.cpp
# model loading
    src/gfx/model/Mesh.cpp
    src/gfx/model/Model.cpp
# lighting
    src/gfx/lights/abstract/AbstractLight.cpp
    src/gfx/lights/abstract/LightPosition.cpp
    src/gfx/lights/abstract/LightAttenuation.cpp
    src/gfx/lights/abstract/LightDirection.cpp
    src/gfx/lights/SpotLight.cpp
    src/gfx/lights/PointLight.cpp
    src/gfx/lights/DirectionalLight.cpp
# rendering pipeline
    src/render/WorldRenderer.cpp
    src/render/Camera.cpp
    src/render/scene/SceneRenderer.cpp
    src/render/scene/WorldChunk.cpp
    src/render/steps/Lighting.cpp
    src/render/steps/HDR.cpp
    src/render/steps/FXAA.cpp
# world handling (IO/basic behaviors)
    src/world/FileWorldReader.cpp
    src/world/FileWorldReader+SQLite.cpp
    src/world/debug/WorldDebugger.cpp
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
    rsrc/fonts/SourceSansPro-Black.ttf
    rsrc/fonts/SpaceMono-Regular.ttf
    rsrc/fonts/SpaceMono-Bold.ttf
# define the namespace and alias we put this lib in
ALIAS cubeland::rsrc_ui WHENCE rsrc NAMESPACE ui)
target_link_libraries(cubeland PRIVATE cubeland::rsrc_ui)

# shaders
file(GLOB SHADER_FRAGMENT_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rsrc/shaders/*/*.frag)
file(GLOB SHADER_VERTEX_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rsrc/shaders/*/*.vert)

cmrc_add_resource_library(cubeland-rsrc-shaders
    ${SHADER_FRAGMENT_FILES}
    ${SHADER_VERTEX_FILES}
ALIAS cubeland::rsrc_shaders WHENCE rsrc/shaders NAMESPACE shaders)
target_link_libraries(cubeland PRIVATE cubeland::rsrc_shaders)

# models
file(GLOB MODEL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rsrc/models/*/*)

cmrc_add_resource_library(cubeland-rsrc-models
    ${MODEL_FILES}
ALIAS cubeland::rsrc_models WHENCE rsrc/models NAMESPACE models)
target_link_libraries(cubeland PRIVATE cubeland::rsrc_models)

# textures
cmrc_add_resource_library(cubeland-rsrc-textures
    rsrc/textures/cube/potato/top.jpg
    rsrc/textures/cube/potato/back.jpg
    rsrc/textures/cube/potato/left.jpg
    rsrc/textures/cube/potato/front.jpg
    rsrc/textures/cube/potato/right.jpg
    rsrc/textures/cube/potato/bottom.jpg
    rsrc/textures/test/empty/whitegreen.png
ALIAS cubeland::rsrc_textures WHENCE rsrc/textures NAMESPACE textures)
target_link_libraries(cubeland PRIVATE cubeland::rsrc_textures)

# SQL statements
cmrc_add_resource_library(cubeland-rsrc-sql
    rsrc/sql/world_v1.sql
ALIAS cubeland::rsrc_sql WHENCE rsrc/sql NAMESPACE sql)
target_link_libraries(cubeland PRIVATE cubeland::rsrc_sql)

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

target_include_directories(cubeland PRIVATE libs/ImGuiFileDialog)

# GLM
#find_package(glm REQUIRED)
#target_link_libraries(cubeland PRIVATE glm::glm)
target_include_directories(cubeland PRIVATE libs/glm)

# SOIL (image loading)
add_library(soil OBJECT
    libs/soil/src/soil.c
    libs/soil/src/image_dxt.c
    libs/soil/src/image_helper.c
    libs/soil/src/stb_image_aug.c
)
target_include_directories(soil PRIVATE libs/soil/include)
target_compile_definitions(soil PUBLIC GL_SILENCE_DEPRECATION)

target_link_libraries(cubeland PRIVATE soil)
target_include_directories(cubeland PRIVATE libs/soil/include)

# tinyobjloader (loading obj formatted models.. duh)
add_library(tinyobjloader OBJECT
    libs/tinyobjloader/tiny_obj_loader.cc
)
target_include_directories(tinyobjloader PRIVATE libs/tinyobjloader)

target_link_libraries(cubeland PRIVATE tinyobjloader)
target_include_directories(cubeland PRIVATE libs/tinyobjloader)

# MUtils (in source tree)
add_library(mutils OBJECT
    src/3rdparty/mutils/ui/dpi.cpp
    src/3rdparty/mutils/ui/theme.cpp
    src/3rdparty/mutils/string/murmur_hash.cpp
    src/3rdparty/mutils/math/math_utils.cpp
    src/3rdparty/mutils/time/timer.cpp
    src/3rdparty/mutils/time/profiler.cpp
)
target_include_directories(mutils PRIVATE src/3rdparty)
target_include_directories(mutils PRIVATE libs/glm)
target_include_directories(mutils PRIVATE libs/imgui)
target_include_directories(mutils PRIVATE src/gui/imgui)

target_link_libraries(mutils PRIVATE fmt::fmt)

target_link_libraries(cubeland PRIVATE mutils)
target_include_directories(cubeland PRIVATE src/3rdparty)

# lz4 (compression; used primarily for block data)
add_library(lz4 OBJECT
    libs/lz4/lib/lz4.c
    libs/lz4/lib/lz4hc.c
    libs/lz4/lib/lz4frame.c
    libs/lz4/lib/xxhash.c
)
target_include_directories(lz4 PRIVATE libs/lz4/lib)

target_link_libraries(cubeland PRIVATE lz4)
target_include_directories(cubeland PRIVATE libs/lz4/lib)

# stduuid (uuid support library)
# this has to be treated as a system library to prevent conflicts against system uuid libs
target_include_directories(cubeland SYSTEM BEFORE PRIVATE libs/stduuid/include)
# target_include_directories(cubeland PRIVATE libs/stduuid/gsl)

# concurrent queue
target_include_directories(cubeland PRIVATE libs/concurrentqueue)

###################################################################################################
#### platform-specific libraries
# macOS frameworks
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(cubeland PUBLIC "-framework Cocoa")
    target_link_libraries(cubeland PUBLIC "-framework ApplicationServices")
endif()

###################################################################################################
#### Secrete a macOS bundle
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(cubeland PROPERTIES
         MACOSX_BUNDLE ON
         MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/rsrc/macos/Info.plist.in)

     set(MACOSX_BUNDLE_BUNDLE_NAME "Cubeland")
     set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2020 Tristan Seifert")
     set(MACOSX_BUNDLE_GUI_IDENTIFIER "me.tseifert.cubeland")
     set(MACOSX_BUNDLE_ICON_FILE "")

     set(MACOSX_BUNDLE_BUNDLE_VERSION "weed")
     set(MACOSX_BUNDLE_SHORT_VERSION_STRING "420")

     set(BUNDLE_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
    #list(APPEND BUNDLE_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md)
    #list(APPEND BUNDLE_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/CONTRIBUTORS.MD)
     set_target_properties(cubeland PROPERTIES RESOURCE "${BUNDLE_RESOURCES}")
     target_sources(cubeland PUBLIC ${BUNDLE_RESOURCES})
endif()

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
    src/io/ResourceManager.cpp
    src/io/MetricsManager.cpp
# utilities
    src/util/LZ4.cpp
    src/util/CPUID.cpp
# UI
    src/gui/MainWindow.cpp
    src/gui/GameUI.cpp
    src/gui/PreferencesWindow.cpp
    src/gui/Loaders.cpp
    src/gui/MetricsDisplay.cpp
# input
    src/input/InputManager.cpp
    src/input/BlockInteractions.cpp
    src/input/PlayerPosPersistence.cpp
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
    src/gfx/gl/query/OcclusionQuery.cpp
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
    src/render/WorldRendererDebugger.cpp
    src/render/Camera.cpp
    src/render/scene/SceneRenderer.cpp
    src/render/scene/ChunkLoader.cpp
    src/render/chunk/WorldChunk.cpp
    src/render/chunk/Globule.cpp
    src/render/chunk/WorldChunkDebugger.cpp
    src/render/chunk/ChunkWorker.cpp
    src/render/steps/Lighting.cpp
    src/render/steps/HDR.cpp
    src/render/steps/FXAA.cpp
    src/render/steps/SSAO.cpp
# world handling (IO/basic behaviors)
    src/world/FileWorldReader.cpp
    src/world/FileWorldReader+SQLite.cpp
    src/world/FileWorldReader+Writing.cpp
    src/world/FileWorldReader+Reading.cpp
    src/world/WorldSource.cpp
    src/world/debug/WorldDebugger.cpp
    src/world/chunk/Chunk.cpp
    src/world/block/BlockRegistry.cpp
    src/world/block/BlockDataGenerator.cpp
    src/world/block/RegisterBuiltins.cpp
    src/world/block/TextureLoader.cpp
    src/world/generators/Terrain.cpp
    src/world/tick/TickHandler.cpp
# built in block types
    src/world/blockImpl/Dirt.cpp
    src/world/blockImpl/Stone.cpp
# inventory support
    src/inventory/Manager.cpp
    src/inventory/ItemDrawing.cpp
    src/inventory/UI.cpp
    src/inventory/UIBar.cpp
    src/inventory/UIDetail.cpp
# physics-y stuff (if you can call it that lol)
    src/physics/Engine.cpp
    src/physics/EngineDebugRenderer.cpp
    src/physics/BlockCollision.cpp
# resources and other misc stuff
    ${version_file}
)

# optimize to have at least AVX
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -mavx")

# warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-anon-enum-enum-conversion -Wno-defaulted-function-deleted -Wno-gnu-zero-variadic-macro-arguments -Wno-reorder-ctor")

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
    rsrc/fonts/Overpass-Regular.ttf
    rsrc/fonts/Overpass-Bold.ttf
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
target_link_directories(cubeland PRIVATE libs/glbinding/build)
target_link_libraries(cubeland PRIVATE glbinding)
target_include_directories(cubeland PRIVATE libs/glbinding/source/glbinding/include)
target_include_directories(cubeland PRIVATE libs/glbinding/build/source/glbinding/include)
target_include_directories(cubeland PRIVATE libs/glbinding/build/source/include)
target_include_directories(cubeland PRIVATE libs/glbinding/3rdparty/KHR/include)

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
    libs/imgui/imgui_demo.cpp
    # use the generic OpenGL/SDL backends
    libs/imgui/backends/imgui_impl_sdl.cpp
    libs/imgui/backends/imgui_impl_opengl3.cpp
    )
target_include_directories(dearimgui PUBLIC ${GLEW_INCLUDE_DIRS})

target_include_directories(dearimgui PRIVATE ${SDL2_INCLUDE_DIRS})
target_include_directories(dearimgui PRIVATE src/gui/imgui)
target_include_directories(dearimgui PRIVATE libs/imgui)
target_include_directories(dearimgui PRIVATE libs/glm)

target_include_directories(dearimgui PRIVATE libs/glbinding/source/glbinding/include)
target_include_directories(dearimgui PRIVATE libs/glbinding/build/source/glbinding/include)
target_include_directories(dearimgui PRIVATE libs/glbinding/build/source/include)
target_include_directories(dearimgui PRIVATE libs/glbinding/3rdparty/KHR/include)

target_include_directories(cubeland PRIVATE src/gui/imgui)
target_include_directories(cubeland PRIVATE libs/imgui)
target_link_libraries(cubeland PRIVATE dearimgui)

# Dear IMGui extensions
add_library(dearimgui_ext OBJECT
    # file browser dialog
    libs/ImGuiFileDialog/ImGuiFileDialog/ImGuiFileDialog.cpp
    # Metrics display
    src/3rdparty/metricsgui/metrics_gui.cpp
    src/3rdparty/metricsgui/ProggyTiny.cpp
)

target_include_directories(dearimgui_ext PRIVATE src/gui/imgui)
target_include_directories(dearimgui_ext PRIVATE libs/imgui)
target_include_directories(dearimgui_ext PRIVATE src/3rdparty/metricsgui)
target_include_directories(dearimgui_ext PRIVATE libs/glm)

target_link_libraries(cubeland PRIVATE dearimgui_ext)

target_include_directories(cubeland PRIVATE libs/ImGuiFileDialog)
target_include_directories(cubeland PRIVATE src/3rdparty)

# GLM
target_include_directories(cubeland PRIVATE libs/glm)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_ENABLE_EXPERIMENTAL")

# Eigen
target_include_directories(cubeland PRIVATE libs/eigen)

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

# Cereal (serialization)
target_include_directories(cubeland PRIVATE libs/cereal/include)

# fastnoise 2 (used primarily for terrain generation)
target_link_libraries(cubeland PRIVATE FastNoise)
target_link_directories(cubeland PRIVATE libs/fastnoise2/build/src)
target_include_directories(cubeland PRIVATE libs/fastnoise2/include)

# ReactPhysics (you must build this in its directory)
target_link_directories(cubeland PRIVATE libs/reactphysics3d/build)
target_include_directories(cubeland PRIVATE libs/reactphysics3d/include)

target_link_libraries(cubeland PRIVATE reactphysics3d)

###################################################################################################
#### platform-specific libraries
# macOS frameworks
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(cubeland PUBLIC "-framework Cocoa")
    target_link_libraries(cubeland PUBLIC "-framework ApplicationServices")
    target_link_libraries(cubeland PUBLIC "-framework OpenGL")
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
     list(APPEND BUNDLE_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/build/default.rsrc)
    #list(APPEND BUNDLE_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/CONTRIBUTORS.MD)
     set_target_properties(cubeland PROPERTIES RESOURCE "${BUNDLE_RESOURCES}")
     target_sources(cubeland PUBLIC ${BUNDLE_RESOURCES})
endif()

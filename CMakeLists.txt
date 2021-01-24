cmake_minimum_required(VERSION 3.12)
project(cubeland)

# include all files in the CMake directory
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

# set global C++ options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
# set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(shared/include)

# compile in the git version file
include(GetGitRevisionDescription)
git_describe(VERSION --tags --dirty=-d)

get_git_head_revision(GIT_REFSPEC GIT_HASH)

string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" VERSION_MAJOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" VERSION_MINOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" VERSION_PATCH "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.[0-9]+(.*)" "\\1" VERSION_SHA1 "${VERSION}")
set(VERSION_SHORT "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

execute_process(COMMAND git describe --tags --dirty=d OUTPUT_VARIABLE VERSION_TAG OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
set(version_file "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")

include(CMakeRC)

find_package(CURL COMPONENTS HTTPS SSL IPv6)

###################################################################################################
### code shared between the app and server
add_library(shared STATIC
    shared/logging/Logging.cpp
    shared/io/ConfigManager.cpp
    shared/io/PathHelper.cpp
    shared/util/LZ4.cpp
    shared/util/CPUID.cpp
    shared/util/Thread.cpp
    shared/world/FileWorldReader.cpp
    shared/world/FileWorldReader+Writing.cpp
    shared/world/FileWorldReader+Reading.cpp
    shared/world/WorldSource.cpp
    shared/world/chunk/Chunk.cpp
    shared/world/generators/Terrain.cpp
)

# platform-specific stuff
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_library(shared_platform OBJECT
        shared/io/PathHelper+Mac.mm
    )
endif()

# SQL statements (to compile into shared library)
cmrc_add_resource_library(cubeland-rsrc-sql
    rsrc/sql/world_v1.sql
    rsrc/sql/prefs_v1.sql
ALIAS cubeland::rsrc_sql WHENCE rsrc/sql NAMESPACE sql)
target_link_libraries(shared PRIVATE cubeland::rsrc_sql)

include_directories(shared)

# libfmt (modern C++ string formatting)
add_subdirectory(libs/fmt ${CMAKE_CURRENT_BINARY_DIR}/libs/fmt)
target_link_libraries(shared PRIVATE fmt::fmt)

# spdlog (logging to console/file)
find_package(spdlog REQUIRED)
target_link_libraries(shared PRIVATE spdlog::spdlog)

# libconfig (reading configuration files)
set(BUILD_EXAMPLES OFF CACHE BOOL "Enable examples" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Enable tests" FORCE)
add_subdirectory(libs/libconfig ${CMAKE_CURRENT_BINARY_DIR}/libs/libconfig)

target_include_directories(shared PRIVATE libs/libconfig/lib)
target_link_libraries(shared PRIVATE config++)

# lz4 (compression; used primarily for block data)
add_library(lz4 OBJECT
    libs/lz4/lib/lz4.c
    libs/lz4/lib/lz4hc.c
    libs/lz4/lib/lz4frame.c
    libs/lz4/lib/xxhash.c
)
target_include_directories(lz4 PRIVATE libs/lz4/lib)

target_link_libraries(shared PRIVATE lz4)
target_include_directories(shared PRIVATE libs/lz4/lib)

find_package(SQLite3 REQUIRED)
target_link_libraries(shared PRIVATE SQLite::SQLite3)

# fastnoise 2 (used primarily for terrain generation)
target_link_libraries(shared PRIVATE FastNoise)
target_link_directories(shared PRIVATE libs/fastnoise2/build/src)
target_include_directories(shared PRIVATE libs/fastnoise2/include)

# Lyra (command line parsing)
add_subdirectory(libs/Lyra)
target_link_libraries(shared PRIVATE bfg::Lyra)

# various header-only libraries
target_include_directories(shared PRIVATE libs/glm)
target_include_directories(shared SYSTEM BEFORE PRIVATE libs/stduuid/include)
target_include_directories(shared PRIVATE libs/stduuid/gsl)
target_include_directories(shared PRIVATE libs/concurrentqueue)
target_include_directories(shared PRIVATE libs/cereal/include)

###################################################################################################
#### sources for app
# build main executable and all modules
add_executable(cubeland
    src/main.cpp
# io
    src/io/PrefsManager.cpp
    src/io/ResourceManager.cpp
    src/io/MetricsManager.cpp
# UI
    src/gui/MainWindow.cpp
    src/gui/GameUI.cpp
    src/gui/PreferencesWindow.cpp
    src/gui/InGamePrefsWindow.cpp
    src/gui/Loaders.cpp
    src/gui/MetricsDisplay.cpp
    src/gui/MenuBarHandler.cpp
    src/gui/title/TitleScreen.cpp
    src/gui/title/PlasmaRenderer.cpp
    src/gui/title/AboutWindow.cpp
    src/gui/title/WorldSelector.cpp
    src/gui/title/ServerSelector.cpp
# input
    src/input/InputManager.cpp
    src/input/BlockInteractions.cpp
    src/input/PlayerPosPersistence.cpp
# GL wrappers
    src/gfx/gl/texture/Texture.cpp
    src/gfx/gl/texture/Texture1D.cpp
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
    src/render/RenderStep.cpp
    src/render/Camera.cpp
    src/render/scene/SceneRenderer.cpp
    src/render/scene/ChunkLoader.cpp
    src/render/chunk/WorldChunk.cpp
    src/render/chunk/Globule.cpp
    src/render/chunk/WorldChunkDebugger.cpp
    src/render/chunk/ChunkWorker.cpp
    src/render/chunk/VertexGenerator.cpp
    src/render/chunk/VertexGeneratorData.cpp
    src/render/steps/Lighting.cpp
    src/render/steps/HDR.cpp
    src/render/steps/FXAA.cpp
    src/render/steps/SSAO.cpp
# world handling (IO/basic behaviors)
    src/world/TimePersistence.cpp
    src/world/RemoteSource.cpp
    src/world/debug/WorldDebugger.cpp
    src/world/block/Block.cpp
    src/world/block/BlockRegistry.cpp
    src/world/block/BlockDataGenerator.cpp
    src/world/block/RegisterBuiltins.cpp
    src/world/block/TextureLoader.cpp
    src/world/tick/TickHandler.cpp
# built in block types
    src/world/blockImpl/Dirt.cpp
    src/world/blockImpl/Stone.cpp
    src/world/blockImpl/Torch.cpp
    src/world/blockImpl/Glass.cpp
    src/world/blockImpl/Brick.cpp
    src/world/blockImpl/Cobblestone.cpp
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
# particle systems
    src/particles/Renderer.cpp
    src/particles/System.cpp
# client authentication and web service integration
    src/web/AuthManager.cpp
    src/net/ServerConnection.cpp
    src/net/handlers/Auth.cpp
    src/net/handlers/BlockChange.cpp
    src/net/handlers/Chat.cpp
    src/net/handlers/Chunk.cpp
    src/net/handlers/PlayerInfo.cpp
    src/net/handlers/PlayerMovement.cpp
    src/net/handlers/Time.cpp
    src/net/handlers/WorldInfo.cpp
# resources and other misc stuff
    ${version_file}
)

target_compile_definitions(cubeland PRIVATE CUBE_CLIENT)

# optimize to have at least AVX
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -mavx")

# warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-anon-enum-enum-conversion -Wno-defaulted-function-deleted -Wno-gnu-zero-variadic-macro-arguments -Wno-reorder-ctor -Wno-c99-extensions")

# shared code
target_link_libraries(cubeland PRIVATE shared shared_platform)

target_include_directories(cubeland PUBLIC src)

###################################################################################################
#### source for server
add_executable(server
    server/main.cpp
    server/world/time/Clock.cpp
# network communication
    server/net/Listener.cpp
    server/net/ListenerClient.cpp
    server/net/handlers/Auth.cpp
    server/net/handlers/BlockChange.cpp
    server/net/handlers/Chat.cpp
    server/net/handlers/Chunk.cpp
    server/net/handlers/PlayerInfo.cpp
    server/net/handlers/PlayerMovement.cpp
    server/net/handlers/Time.cpp
    server/net/handlers/WorldInfo.cpp
# auth support
    server/auth/KeyCache.cpp
# resources
    ${version_file}
)

target_link_libraries(server PRIVATE cubeland::rsrc_sql)

target_include_directories(server PRIVATE server)

target_link_libraries(server PRIVATE shared shared_platform)

# SQL statements (to compile into server library)
cmrc_add_resource_library(server-sql
    rsrc/sql/keycache_v1.sql
ALIAS cubeland::server_sql WHENCE rsrc/sql NAMESPACE server_sql)
target_link_libraries(shared PRIVATE cubeland::server_sql)
# libraries
target_link_libraries(server PRIVATE FastNoise)
target_link_directories(server PRIVATE libs/fastnoise2/build/src)
target_include_directories(server PRIVATE libs/fastnoise2/include)

target_include_directories(server PRIVATE libs/libconfig/lib)

target_include_directories(server SYSTEM BEFORE PRIVATE libs/stduuid/include)
target_include_directories(server PRIVATE libs/stduuid/gsl)
target_include_directories(server PRIVATE libs/glm)
target_include_directories(server PRIVATE libs/cpptime)

# concurrent queue
target_include_directories(server PRIVATE libs/concurrentqueue)

# Cereal (serialization)
target_include_directories(server PRIVATE libs/cereal/include)

target_include_directories(server PRIVATE libs/rapidjson/include)
target_include_directories(server PRIVATE libs/jwt-cpp/include)

# LibreSSL
target_link_directories(server PRIVATE libs/libressl/crypto/.libs)
target_link_directories(server PRIVATE libs/libressl/ssl/.libs)
target_link_directories(server PRIVATE libs/libressl/tls/.libs)
target_include_directories(server PRIVATE libs/libressl/include)
target_link_libraries(server PRIVATE crypto ssl tls)

target_link_libraries(server PRIVATE fmt::fmt)
target_link_libraries(server PRIVATE spdlog::spdlog)
target_link_libraries(server PRIVATE bfg::Lyra)
target_link_libraries(server PRIVATE CURL::libcurl)

###################################################################################################
#### resources
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

# SDL2 (we build a dynamic version on macOS, static elsewhere)
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)

    # do not link yet. we'll do this further down when we build the bundle to get the correct rpath
    target_link_libraries(cubeland PUBLIC SDL2main SDL2)
else()
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)

    target_link_libraries(cubeland PUBLIC SDL2main SDL2 SDL2-static)
endif()

add_subdirectory(libs/sdl2)
include_directories(libs/sdl2/include)

# SQLite 3 (data file format)
target_link_libraries(cubeland PRIVATE SQLite::SQLite3)

# Lyra (command line parsing)
target_link_libraries(cubeland PUBLIC bfg::Lyra)

# config loading and string IO
target_link_libraries(cubeland PRIVATE fmt::fmt)
target_link_libraries(cubeland PRIVATE spdlog::spdlog)

target_include_directories(cubeland PRIVATE libs/libconfig/lib)

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

target_include_directories(dearimgui_ext PRIVATE src)
add_compile_definitions(CUSTOM_IMGUIFILEDIALOG_CONFIG="gui/imgui/ImGuiFileDialogConfig.h")

target_include_directories(dearimgui_ext PRIVATE src/gui/imgui)
target_include_directories(dearimgui_ext PRIVATE libs/imgui)
target_include_directories(dearimgui_ext PRIVATE src/3rdparty/metricsgui)
target_include_directories(dearimgui_ext PRIVATE libs/glm)

target_link_libraries(cubeland PRIVATE dearimgui_ext)

target_include_directories(cubeland PRIVATE libs/imgui_markdown)
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
target_link_libraries(mutils PRIVATE spdlog::spdlog)

target_link_libraries(cubeland PRIVATE mutils)
target_include_directories(cubeland PRIVATE src/3rdparty)

# stduuid (uuid support library)
# this has to be treated as a system library to prevent conflicts against system uuid libs
target_include_directories(cubeland SYSTEM BEFORE PRIVATE libs/stduuid/include)
target_include_directories(cubeland PRIVATE libs/stduuid/gsl)

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

# libcurl + RapidJSON (header only) for REST API stuff
target_link_libraries(cubeland PRIVATE CURL::libcurl)

target_include_directories(cubeland PRIVATE libs/rapidjson/include)
target_include_directories(cubeland PRIVATE libs/jwt-cpp/include)

# LibreSSL
target_link_directories(cubeland PRIVATE libs/libressl/crypto/.libs)
target_link_directories(cubeland PRIVATE libs/libressl/ssl/.libs)
target_link_directories(cubeland PRIVATE libs/libressl/tls/.libs)

target_include_directories(cubeland PRIVATE libs/libressl/include)
target_link_libraries(cubeland PRIVATE crypto ssl tls)

# LibJPEG Turbo
target_link_directories(cubeland PRIVATE libs/libjpeg-turbo/build_x86)
target_link_libraries(cubeland PRIVATE turbojpeg)

target_include_directories(cubeland PRIVATE libs/libjpeg-turbo)
target_include_directories(cubeland PRIVATE libs/libjpeg-turbo/build_x86)

# Avir (image scaling)
target_include_directories(cubeland PRIVATE libs/avir)

###################################################################################################
#### platform-specific libraries
# macOS frameworks
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(server PUBLIC "-framework Foundation")

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
    list(APPEND BUNDLE_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/rsrc/cacert.pem)
    list(APPEND BUNDLE_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/build/default.rsrc)
    set_target_properties(cubeland PROPERTIES RESOURCE "${BUNDLE_RESOURCES}")
    target_sources(cubeland PUBLIC ${BUNDLE_RESOURCES})

    # get some paths of the bundle
    set(BUNDLE_FRAMEWORK_PATH "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/Frameworks")
    set(BUNDLE_BINARY "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.app/Contents/MacOS/${PROJECT_NAME}")

    # set correctly the rpath
    add_custom_command(TARGET cubeland POST_BUILD COMMAND ${CMAKE_INSTALL_NAME_TOOL} -delete_rpath ${CMAKE_CURRENT_SOURCE_DIR}/libs/glbinding/build -delete_rpath ${CMAKE_CURRENT_SOURCE_DIR}/libs/fastnoise2/build/src -delete_rpath ${CMAKE_CURRENT_SOURCE_DIR}/libs/reactphysics3d/build -delete_rpath ${CMAKE_BINARY_DIR}/libs/sdl2 -delete_rpath ${CMAKE_CURRENT_SOURCE_DIR}/libs/libressl/ssl/.libs -delete_rpath ${CMAKE_CURRENT_SOURCE_DIR}/libs/libressl/tls/.libs -delete_rpath
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/libressl/crypto/.libs -add_rpath @loader_path/../Frameworks ${BUNDLE_BINARY})

    # copy the SDL dylib
    file(GLOB SDL_LIBRARY "${CMAKE_BINARY_DIR}/libs/sdl2/libSDL2*.dylib")
    message(STATUS "Copying SDL2 dylib: ${SDL_LIBRARY}")
    file(COPY ${SDL_LIBRARY} DESTINATION ${BUNDLE_FRAMEWORK_PATH} FOLLOW_SYMLINK_CHAIN)

    # llvm C++ library, in default Homebrew location
    # TODO: is there a better way to do this? if the version of the c++ lib changes, shit will die
    # add_custom_command(TARGET cubeland POST_BUILD COMMAND ${CMAKE_INSTALL_NAME_TOOL} -change /usr/local/opt/llvm/lib/libc++.1.dylib @rpath/libc++.1.dylib ${BUNDLE_BINARY})

    #file(COPY /usr/local/opt/llvm/lib/libc++.1.dylib DESTINATION ${BUNDLE_FRAMEWORK_PATH} FOLLOW_SYMLINK_CHAIN)
    #file(COPY /usr/local/opt/llvm/lib/libc++abi.1.dylib DESTINATION ${BUNDLE_FRAMEWORK_PATH} FOLLOW_SYMLINK_CHAIN)
endif()

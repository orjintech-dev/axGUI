cmake_minimum_required(VERSION 3.16)
project(axGUI LANGUAGES C CXX)

# axGUI: immediate-mode GUI library for raylib 6.0, plus demos.
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j
# Web (WebAssembly) build with Emscripten:
#   emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-web -j
#   python -m http.server 8080 -d build-web/demos   ->  http://localhost:8080/showcase/
# axGUI.pro (qmake) builds the same targets.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AXGUI_BUILD_DEMOS "Build the demos in demos/" ON)

if(EMSCRIPTEN)
    set(PLATFORM "Web" CACHE STRING "" FORCE)
    set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)   # WebGL 2 (axDrawBot shaders use #version 300 es)
endif()

# ---------------------------------------------------------------- raylib 6.0 (libs/raylib-6.0, its own CMake)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# Don't use CUSTOMIZE_BUILD: in that mode raylib 6.0's CMake misreads the config.h defaults and enables
# disabled features too (e.g. SUPPORT_CUSTOM_FRAME_CONTROL -> GetFrameTime() is always 0, animations freeze).
add_subdirectory(libs/raylib-6.0 EXCLUDE_FROM_ALL)
# JPG is off by default in 6.0; config.h guards it with #ifndef, so a define turns it on
target_compile_definitions(raylib PRIVATE SUPPORT_FILEFORMAT_JPG=1)

# ---------------------------------------------------------------- axGUI library
add_library(axgui STATIC
    src/stb_impl.cpp
    src/axGuiSystem.cpp
    src/axGUITextBox.cpp
    src/axGUITable.cpp
    src/axGUIStyle.cpp
    src/sdf2d/sdfRenderer2D.cpp
    src/rich/axRichFonts.cpp
    src/rich/axRichText.cpp
    src/rich/axRichEdit.cpp
    src/rich/axRichHtml.cpp
    src/rich/axRichClip.cpp
    src/rich/axGUIRichText.cpp
)
target_include_directories(axgui PUBLIC
    src
    src/sdf2d
    src/rich
    libs/nlohmann
    libs/raylib-6.0/src
)
# raylib's external/ only after the system headers (its dirent.h is Windows-only); stb_truetype.h comes from here
target_compile_options(axgui PUBLIC -idirafter ${CMAKE_CURRENT_SOURCE_DIR}/libs/raylib-6.0/src/external)
if(EMSCRIPTEN)
    target_compile_definitions(axgui PUBLIC PLATFORM_WEB GRAPHICS_API_OPENGL_ES3)
else()
    target_compile_definitions(axgui PUBLIC PLATFORM_DESKTOP_GLFW GRAPHICS_API_OPENGL_33)
endif()
target_link_libraries(axgui PUBLIC raylib)

# ---------------------------------------------------------------- demos
# axgui_add_demo(<target> <sources...>): an executable linked with axgui that finds resources/ next to itself
# (desktop) or in its .data package (web). On web it also writes index.html from web/shell.html.
set(AXGUI_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(AXGUI_WEB_SHELL ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
function(axgui_add_demo target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE axgui)
    if(EMSCRIPTEN)
        set_target_properties(${target} PROPERTIES SUFFIX ".js")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${AXGUI_WEB_SHELL})
        file(READ ${AXGUI_WEB_SHELL} shell)
        string(REPLACE "@AX_SCRIPT@" "<script async src=\"${target}.js\"></script>" shell "${shell}")
        string(REPLACE "@AX_TITLE@" "${target}" shell "${shell}")
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/index.html "${shell}")
        # 1 MB stack (Emscripten < 3.1.25 calls it TOTAL_STACK)
        if(EMSCRIPTEN_VERSION VERSION_LESS 3.1.25)
            set(stack -sTOTAL_STACK=1048576)
        else()
            set(stack -sSTACK_SIZE=1048576)
        endif()
        target_link_options(${target} PRIVATE
            -sUSE_GLFW=3 -sASYNCIFY -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1
            -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 ${stack}
            -sEXPORTED_RUNTIME_METHODS=ccall
            --preload-file ${AXGUI_RESOURCES}@/resources)
    else()
        target_compile_definitions(${target} PRIVATE DEMO_RES_DIR="${AXGUI_RESOURCES}")
        if(MINGW AND CMAKE_BUILD_TYPE STREQUAL "Release")
            target_link_options(${target} PRIVATE -mwindows -static)
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${AXGUI_RESOURCES} $<TARGET_FILE_DIR:${target}>/resources)
    endif()
endfunction()

if(AXGUI_BUILD_DEMOS)
    add_subdirectory(demos/showcase)
    add_subdirectory(demos/layouts)
endif()

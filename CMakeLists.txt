cmake_minimum_required(VERSION 3.16)
project(axGUI-demo LANGUAGES C CXX)

# axGUI-demo: showcase and development app for axGUI (raylib 6.0).
# axGUI-demo.pro (qmake) builds the same sources; this file is for CMake-based IDEs and the command line.
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Web (WebAssembly) build with Emscripten:
#   emcmake cmake -S . -B build-web -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-web -j
#   python -m http.server -d build-web 8080   ->  http://localhost:8080/
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

# ---------------------------------------------------------------- demo
add_executable(axGUI-demo
    src/main.cpp
    src/stb_impl.cpp
    src/axGUI/axGuiSystem.cpp
    src/axGUI/axGUITextBox.cpp
    src/axGUI/axGUITable.cpp
    src/axGUI/axGUIStyle.cpp
    src/axGUI/sdf2d/sdfRenderer2D.cpp
    src/axGUI/rich/axRichFonts.cpp
    src/axGUI/rich/axRichText.cpp
    src/axGUI/rich/axRichEdit.cpp
    src/axGUI/rich/axRichHtml.cpp
    src/axGUI/rich/axRichClip.cpp
    src/axGUI/rich/axGUIRichText.cpp
)

target_include_directories(axGUI-demo PRIVATE
    src
    src/axGUI
    src/axGUI/sdf2d
    src/axGUI/rich
    libs/nlohmann
    libs/raylib-6.0/src
    libs/raylib-6.0/src/external
)

if(EMSCRIPTEN)
    target_compile_definitions(axGUI-demo PRIVATE PLATFORM_WEB GRAPHICS_API_OPENGL_ES3)
    # output: axGUI-demo.js / .wasm / .data; the page index.html is generated from web/shell.html (loads axGUI-demo.js)
    set_target_properties(axGUI-demo PROPERTIES SUFFIX ".js")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html AX_SHELL)
    string(REPLACE "{{{ SCRIPT }}}" "<script async src=\"axGUI-demo.js\"></script>" AX_SHELL "${AX_SHELL}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/index.html "${AX_SHELL}")
    # 1 MB stack (Emscripten < 3.1.25 calls it TOTAL_STACK)
    if(EMSCRIPTEN_VERSION VERSION_LESS 3.1.25)
        set(AX_WEB_STACK -sTOTAL_STACK=1048576)
    else()
        set(AX_WEB_STACK -sSTACK_SIZE=1048576)
    endif()
    target_link_options(axGUI-demo PRIVATE
        -sUSE_GLFW=3 -sASYNCIFY -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1
        -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 ${AX_WEB_STACK}
        -sEXPORTED_RUNTIME_METHODS=ccall
        --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/resources@/resources)
else()
    target_compile_definitions(axGUI-demo PRIVATE
        DEMO_RES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
        PLATFORM_DESKTOP_GLFW
        GRAPHICS_API_OPENGL_33
    )
endif()

target_link_libraries(axGUI-demo PRIVATE raylib)

if(WIN32)
    if(MINGW AND CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(axGUI-demo PRIVATE -mwindows -static)
    endif()
endif()

# copy resources/ next to the executable (on web they go into the .data package via --preload-file)
if(NOT EMSCRIPTEN)
add_custom_command(TARGET axGUI-demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:axGUI-demo>/resources)
endif()

# raylib 6.0 compiled from source together with the app (no separate libraylib.a needed).

RL = $$PWD/raylib-6.0/src

DEFINES += SUPPORT_FILEFORMAT_JPG=1          # JPG is off by default in raylib 6.0
INCLUDEPATH += $$RL

# raylib/external must be searched AFTER the system headers, so that rcore.c's <dirent.h> doesn't pick up the Windows-only
# external/dirent.h (io.h error on Linux / macOS). axFont.h's stb_truetype.h comes from here.
QMAKE_CFLAGS += -idirafter $$RL/external
QMAKE_CXXFLAGS += -idirafter $$RL/external

# 6.0 has no utils.c (merged into rcore.c); these modules are enough.
RAYLIB_SOURCES = \
    $$RL/rcore.c \
    $$RL/rshapes.c \
    $$RL/rtextures.c \
    $$RL/rtext.c \
    $$RL/rmodels.c \
    $$RL/raudio.c

wasm {
    # Web (Emscripten, WebGL2) needs a Qt for WebAssembly kit; prefer the CMake web build (see README).
    DEFINES += PLATFORM_WEB GRAPHICS_API_OPENGL_ES3
    SOURCES += $$RAYLIB_SOURCES
    QMAKE_LFLAGS += -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
                    -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1
} else {
    DEFINES += PLATFORM_DESKTOP_GLFW GRAPHICS_API_OPENGL_33
    INCLUDEPATH += $$RL/external/glfw/include
    SOURCES += $$RAYLIB_SOURCES $$RL/rglfw.c

    win32 {
        LIBS += -lopengl32 -lgdi32 -lwinmm -lshell32
        # Release: no console window; MinGW runtime libraries are linked statically
        CONFIG(release, debug|release): QMAKE_LFLAGS += -mwindows -static
    }
    macx {
        # GLFW's Cocoa backend is Objective-C; rglfw.c is compiled with this flag
        QMAKE_CFLAGS += -x objective-c
        LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio -framework AudioToolbox
    }
    unix:!macx {
        DEFINES += _GLFW_X11     # for Wayland use _GLFW_WAYLAND (needs extra headers)
        LIBS += -lGL -lX11 -lpthread -ldl -lrt -lm
    }
}

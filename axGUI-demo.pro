# axGUI-demo - showcase and development app for axGUI (raylib 6.0)
# qmake is used only as the build system; there is NO dependency on Qt libraries.
# raylib 6.0 is compiled from source together with the app (no separate libraylib.a needed).

TEMPLATE = app
TARGET = axGUI-demo
CONFIG -= qt app_bundle
CONFIG += c++17 warn_off
CONFIG(debug, debug|release): CONFIG += console

RL = $$PWD/libs/raylib-6.0/src

DEFINES += DEMO_RES_DIR=\\\"$$PWD/resources\\\"
DEFINES += SUPPORT_FILEFORMAT_JPG=1          # JPG is off by default in raylib 6.0

INCLUDEPATH += \
    $$PWD/src \
    $$PWD/src/axGUI \
    $$PWD/src/axGUI/sdf2d \
    $$PWD/src/axGUI/rich \
    $$PWD/libs/nlohmann \
    $$RL

# raylib/external must be searched AFTER the system headers, so that rcore.c's <dirent.h> doesn't pick up the Windows-only
# external/dirent.h (io.h error on Linux / macOS). axFont.h's stb_truetype.h comes from here.
QMAKE_CFLAGS += -idirafter $$RL/external
QMAKE_CXXFLAGS += -idirafter $$RL/external

SOURCES += \
    src/main.cpp \
    src/stb_impl.cpp \
    src/axGUI/axGuiSystem.cpp \
    src/axGUI/axGUITextBox.cpp \
    src/axGUI/axGUITable.cpp \
    src/axGUI/axGUIStyle.cpp \
    src/axGUI/sdf2d/sdfRenderer2D.cpp \
    src/axGUI/rich/axRichFonts.cpp \
    src/axGUI/rich/axRichText.cpp \
    src/axGUI/rich/axRichEdit.cpp \
    src/axGUI/rich/axRichHtml.cpp \
    src/axGUI/rich/axRichClip.cpp \
    src/axGUI/rich/axGUIRichText.cpp

HEADERS += \
    src/parameter.h \
    src/axGUI/axDefs.h \
    src/axGUI/axDrawBot.h \
    src/axGUI/axFont.h \
    src/axGUI/axGuiSystem.h \
    src/axGUI/axGUITextBox.h \
    src/axGUI/axGUITable.h \
    src/axGUI/axUTF8.h \
    src/axGUI/sdf2d/SdfRenderer2D.h \
    src/axGUI/sdf2d/TileBatchSystem.h \
    src/axGUI/sdf2d/sdfShaders.h \
    src/axGUI/rich/axRichFonts.h \
    src/axGUI/rich/axRichText.h \
    src/axGUI/rich/axRichEdit.h \
    src/axGUI/rich/axRichHtml.h \
    src/axGUI/rich/axRichClip.h \
    src/axGUI/rich/axGUIRichText.h

# ---------------------------------------------------------------- raylib 6.0 modules
# 6.0 has no utils.c (merged into rcore.c); these 7 modules are enough.
RAYLIB_SOURCES = \
    $$RL/rcore.c \
    $$RL/rshapes.c \
    $$RL/rtextures.c \
    $$RL/rtext.c \
    $$RL/rmodels.c \
    $$RL/raudio.c

wasm {
    # Web (Emscripten, WebGL2) needs a Qt for WebAssembly kit; prefer the CMake web build (see README).
    # axDrawBot shaders use "#version 300 es" on ES.
    DEFINES += PLATFORM_WEB GRAPHICS_API_OPENGL_ES3
    SOURCES += $$RAYLIB_SOURCES
    QMAKE_LFLAGS += -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
                    -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 \
                    --preload-file $$PWD/resources@/resources
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

    # copy resources/ next to the build output (looked up next to the executable)
    copyres.commands = $(COPY_DIR) $$shell_path($$PWD/resources) $$shell_path($$OUT_PWD/resources)
    first.depends = $(first) copyres
    export(first.depends)
    export(copyres.commands)
    QMAKE_EXTRA_TARGETS += first copyres
}

DISTFILES += README.md CMakeLists.txt

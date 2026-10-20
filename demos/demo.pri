# Common settings for the demo apps: axGUI + raylib, shared resources/ folder.

TEMPLATE = app
CONFIG -= qt app_bundle
CONFIG += c++17 warn_off
CONFIG(debug, debug|release): CONFIG += console

include($$PWD/../src/axGUI.pri)
include($$PWD/../libs/raylib.pri)

RES = $$PWD/../resources
DEFINES += DEMO_RES_DIR=\\\"$$clean_path($$RES)\\\"

wasm {
    QMAKE_LFLAGS += --preload-file $$clean_path($$RES)@/resources
} else {
    # copy resources/ next to the build output (looked up next to the executable)
    copyres.commands = $(COPY_DIR) $$shell_path($$clean_path($$RES)) $$shell_path($$OUT_PWD/resources)
    first.depends = $(first) copyres
    export(first.depends)
    export(copyres.commands)
    QMAKE_EXTRA_TARGETS += first copyres
}

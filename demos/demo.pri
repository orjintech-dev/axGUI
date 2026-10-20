# Common settings for the demo apps: axGUI + raylib, shared resources/ folder.

TEMPLATE = app
CONFIG -= qt app_bundle
CONFIG += c++17 warn_off
CONFIG(debug, debug|release): CONFIG += console

include($$PWD/../src/axGUI.pri)
include($$PWD/../libs/raylib.pri)

RES = $$PWD/../resources

wasm {
    # resources/ go into the .data package (mounted at /resources)
    QMAKE_LFLAGS += -sASYNCIFY -sFULL_ES3=1 --preload-file $$clean_path($$RES)@/resources

    # The Qt kit's page (qtloader) expects a Qt app, and raylib needs a canvas, so the page is built from
    # web/shell.html instead. The kit links with MODULARIZE=1 and EXPORT_NAME=<target>_entry: the page calls
    # that factory with its Module settings. index.html is written here and copied over <target>.html after
    # linking, so Qt Creator's Run opens it too.
    AX_ENTRY = $$replace(TARGET, -, _)_entry
    AX_PAGE = $$cat($$PWD/../web/shell.html, blob)
    AX_PAGE = $$replace(AX_PAGE, @AX_TITLE@, $$TARGET)
    AX_PAGE = $$replace(AX_PAGE, @AX_SCRIPT@, "<script src=\"$${TARGET}.js\"></script><script>$${AX_ENTRY}(Module);</script>")
    write_file($$OUT_PWD/index.html, AX_PAGE)|error("cannot write $$OUT_PWD/index.html")
    QMAKE_POST_LINK += $(COPY_FILE) $$shell_path($$OUT_PWD/index.html) $$shell_path($$OUT_PWD/$${TARGET}.html)
} else {
    DEFINES += DEMO_RES_DIR=\\\"$$clean_path($$RES)\\\"

    # copy resources/ next to the build output (looked up next to the executable)
    copyres.commands = $(COPY_DIR) $$shell_path($$clean_path($$RES)) $$shell_path($$OUT_PWD/resources)
    first.depends = $(first) copyres
    export(first.depends)
    export(copyres.commands)
    QMAKE_EXTRA_TARGETS += first copyres
}

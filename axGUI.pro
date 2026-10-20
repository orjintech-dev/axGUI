# axGUI: immediate-mode GUI library for raylib 6.0, plus demos.
# qmake is used only as the build system; there is NO dependency on Qt libraries.
# Each demo compiles axGUI and raylib 6.0 from source (src/axGUI.pri, libs/raylib.pri).

TEMPLATE = subdirs
SUBDIRS += demos/showcase

DISTFILES += README.md CHANGELOG.md CMakeLists.txt

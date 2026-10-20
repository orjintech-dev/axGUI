# axGUI library sources. Include from an app .pro together with libs/raylib.pri:
#   include(path/to/src/axGUI.pri)
#   include(path/to/libs/raylib.pri)

CONFIG += c++17

INCLUDEPATH += \
    $$PWD \
    $$PWD/sdf2d \
    $$PWD/rich \
    $$PWD/../libs/nlohmann

SOURCES += \
    $$PWD/stb_impl.cpp \
    $$PWD/axGuiSystem.cpp \
    $$PWD/axGUITextBox.cpp \
    $$PWD/axGUITable.cpp \
    $$PWD/axGUIStyle.cpp \
    $$PWD/sdf2d/sdfRenderer2D.cpp \
    $$PWD/rich/axRichFonts.cpp \
    $$PWD/rich/axRichText.cpp \
    $$PWD/rich/axRichEdit.cpp \
    $$PWD/rich/axRichHtml.cpp \
    $$PWD/rich/axRichClip.cpp \
    $$PWD/rich/axGUIRichText.cpp

HEADERS += \
    $$PWD/parameter.h \
    $$PWD/axDefs.h \
    $$PWD/axDrawBot.h \
    $$PWD/axFont.h \
    $$PWD/axGuiSystem.h \
    $$PWD/axGUITextBox.h \
    $$PWD/axGUITable.h \
    $$PWD/axUTF8.h \
    $$PWD/sdf2d/SdfRenderer2D.h \
    $$PWD/sdf2d/TileBatchSystem.h \
    $$PWD/sdf2d/sdfShaders.h \
    $$PWD/rich/axRichFonts.h \
    $$PWD/rich/axRichText.h \
    $$PWD/rich/axRichEdit.h \
    $$PWD/rich/axRichHtml.h \
    $$PWD/rich/axRichClip.h \
    $$PWD/rich/axGUIRichText.h

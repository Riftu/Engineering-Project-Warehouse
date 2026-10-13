QT       += core gui multimedia network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

VERSION = 1.0.0.1
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

include($$PWD/module/module.pri)
INCLUDEPATH += $$PWD/module/

SOURCES += \
    main.cpp \
    widget.cpp

HEADERS += \
    widget.h \
    common.h \
    winuser.h

FORMS += \
    widget.ui

RC_ICONS = stico.ico

INCLUDEPATH += $$PWD/include

LIBS += $$PWD/lib/avformat.lib \
        $$PWD/lib/avcodec.lib \
        $$PWD/lib/avdevice.lib \
        $$PWD/lib/avfilter.lib \
        $$PWD/lib/avutil.lib \
        $$PWD/lib/postproc.lib \
        $$PWD/lib/swresample.lib \
        $$PWD/lib/swscale.lib
#LIBS += -lgdi32

RESOURCES += \
    res.qrc

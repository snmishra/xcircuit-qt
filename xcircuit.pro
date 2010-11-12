#-------------------------------------------------
#
# Project created by QtCreator 2010-09-13T19:03:00
#
#-------------------------------------------------

QT += core gui

TARGET = xcircuit
TEMPLATE = app

macx:ICON = xcircuit.icns
win:RC_FILE = resources.rc

VERSION=3.7
REVISION=8

isEmpty(PREFIX): PREFIX=/usr/lib/xcircuit-$$VERSION

LIBRARY_DIR=$$PREFIX
PROLOGUE_FILE=xcircps2.pro
STARTUP_FILE=startup.script
USER_RC_FILE=.xcircuitrc
GS_EXEC=gs
TEMP_DIR=/tmp

DEFINES += \
    RESOURCES_DIR=$$join(LIBRARY_DIR,'','\\\"','\\\"') \
    BUILTINS_DIR=$$join(LIBRARY_DIR,'','\\\"','\\\"') \
    PROLOGUE_DIR=$$join(LIBRARY_DIR,'','\\\"','\\\"') \
    PROLOGUE_FILE=$$join(PROLOGUE_FILE,'','\\\"','\\\"') \
    SCRIPTS_DIR=$$join(LIBRARY_DIR,'','\\\"','\\\"') \
    STARTUP_FILE=$$join(STARTUP_FILE,'','\\\"','\\\"') \
    USER_RC_FILE=$$join(USER_RC_FILE,'','\\\"','\\\"') \
    GS_EXEC=$$join(GS_EXEC,'','\\\"','\\\"') \
    TEMP_DIR=$$join(TEMP_DIR,'','\\\"','\\\"') \
    PROG_REVISION=$$join(REVISION) \
    PROG_VERSION=$$join(VERSION) \
    SIZEOF_VOID_P=8 \
    SIZEOF_UNSIGNED_INT=4 \
    SIZEOF_UNSIGNED_LONG=8 \
    HAVE_DIRENT_H=1 \
    HAVE_U_CHAR=1 \
    XC_QT=1 \
    HAVE_LIBZ=1
#INPUT_FOCUS=1

unix:LIBS += -lz -lm

SOURCES = \
    colors.cpp \
    files.cpp \
    flate.cpp \
    fontfile.cpp \
    formats.cpp \
    functions.cpp \
    graphic.cpp \
    help.cpp \
    keybindings.cpp \
    libraries.cpp \
    menus.cpp \
    menucalls.cpp \
    netlist.cpp \
    ngspice.cpp \
    parameter.cpp \
    python.cpp \
    rcfile.cpp \
    render.cpp \
    schema.cpp \
    selection.cpp \
    svg.cpp \
    text.cpp \
    undo.cpp \
    xcircuit.cpp \
    xtfuncs.cpp \
    xtgui.cpp \
    elements.cpp \
    events.cpp \
    filelist.cpp \
    xcqt.cpp \
    pixmaps.cpp \
    monitoredvar.cpp \
    area.cpp \
    label.cpp \
    polygon.cpp \
    path.cpp \
    objinst.cpp \
    arc.cpp \
    spline.cpp \
    plist.cpp \
    generic.cpp \
    object.cpp \
    context.cpp \
    positionable.cpp \
    matrix.cpp

HEADERS = \
    colors.h \
    xcqt.h \
    xcqt_p.h \
    xcircuit.h \
    prototypes.h \
    menus.h \
    pixmaps.h \
    xctypes.h \
    xcolors.h \
    monitoredvar.h \
    area.h \
    elements.h \
    context.h \
    matrix.h

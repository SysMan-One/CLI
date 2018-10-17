TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    cli_routines.c \
    ../SecurityCode/vCloud/utility_routines.c

DEFINES	+= __CLI_DEBUG__=1
DEFINES	+= __TRACE__=1
DEFINES	+= _DEBUG=1

INCLUDEPATH	+= ../SecurityCode/vCloud/
INCLUDEPATH	+= ./

HEADERS += \
    cli_routines.h

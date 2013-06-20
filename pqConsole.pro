#--------------------------------------------------
# pqConsole.pro: SWI-Prolog / QT interface
#--------------------------------------------------
#
# REPL in QTextEdit on a background logic processor
#--------------------------------------------------
# Ing. Capelli Carlo - Brescia 2013

#-------------------------------------------------
# Project created by QtCreator 2013-03-27T12:59:54
#-------------------------------------------------

QT += testlib

TARGET = pqConsole
TEMPLATE = lib

DEFINES += PQCONSOLE_LIBRARY

# please, not obsolete compiler
QMAKE_CXXFLAGS += -std=c++0x

SOURCES += \
    pqConsole.cpp \
    SwiPrologEngine.cpp \
    ConsoleEdit.cpp \
    pqTerm.cpp

HEADERS += \
    pqConsole.h \
    pqConsole_global.h \
    SwiPrologEngine.h \
    ConsoleEdit.h \
    PREDICATE.h \
    pqTerm.h

symbian {
    MMP_RULES += EXPORTUNFROZEN
    TARGET.UID3 = 0xE3F3BDE3
    TARGET.CAPABILITY = 
    TARGET.EPOCALLOWDLLDATA = 1
    addFiles.sources = pqConsole.dll
    addFiles.path = !:/sys/bin
    DEPLOYMENT += addFiles
}

unix:!symbian {

    # because SWI-Prolog is built from source
    CONFIG += link_pkgconfig
    PKGCONFIG += swipl

    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }

    INSTALLS += target
}

OTHER_FILES += \
    README.md

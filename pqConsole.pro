#--------------------------------------------------
# pqConsole.pro: SWI-Prolog / QT interface
#--------------------------------------------------
# REPL in QTextEdit on a background logic processor
#--------------------------------------------------
# Copyright (C) : 2013,2014 Carlo Capelli

QT += core gui widgets

TARGET = pqConsole
TEMPLATE = lib

DEFINES += PQCONSOLE_LIBRARY

# moved where the class is defined
# DEFINES += PQCONSOLE_BROWSER

# prevent symbol/macro clashes with Qt
DEFINES += PL_SAFE_ARG_MACROS

# please, not obsolete compiler
!macx: QMAKE_CXXFLAGS += -std=c++0x

SOURCES += \
    pqConsole.cpp \
    SwiPrologEngine.cpp \
    ConsoleEdit.cpp \
    pqTerm.cpp \
    Completion.cpp \
    Swipl_IO.cpp \
    pqMainWindow.cpp \
    Preferences.cpp \
    FlushOutputEvents.cpp \
    pqApplication.cpp \
    win_builtins.cpp \
    reflexive.cpp \
    pqMiniSyntax.cpp

HEADERS += \
    pqConsole.h \
    pqConsole_global.h \
    SwiPrologEngine.h \
    ConsoleEdit.h \
    PREDICATE.h \
    pqTerm.h \
    Completion.h \
    Swipl_IO.h \
    pqMainWindow.h \
    Preferences.h \
    FlushOutputEvents.h \
    pqApplication.h \
    pqMiniSyntax.h

symbian {
    MMP_RULES += EXPORTUNFROZEN
    TARGET.UID3 = 0xE3F3BDE3
    TARGET.CAPABILITY = 
    TARGET.EPOCALLOWDLLDATA = 1
    addFiles.sources = pqConsole.dll
    addFiles.path = !:/sys/bin
    DEPLOYMENT += addFiles
}

macx {
    QT_CONFIG -= no-pkg-config
    # The mac build of qmake has pkg-config support disabled by default, see
    # http://stackoverflow.com/a/16972067/1329652
    !system(pkg-config --exists swipl):error("pkg-config indicates that swipl is missing.")
    SWIPL_CXXFLAGS = $$system("pkg-config --cflags swipl")
    # remove the macports include path since it'll interfere with Qt
    SWIPL_CXXFLAGS = $$replace(SWIPL_CXXFLAGS, "-I/opt/local/include", "")
    SWIPL_LFLAGS = $$system("pkg-config --libs-only-L --libs-only-l swipl")
    QMAKE_CXXFLAGS += $$SWIPL_CXXFLAGS
    QMAKE_LFLAGS += $$SWIPL_LFLAGS

    greaterThan(QT_MAJOR_VERSION, 4): {
        CONFIG += c++11
        cache()
    } else {
        QMAKE_CXXFLAGS += -stdlib=libc++ -std=c++0x
        QMAKE_LFLAGS += -stdlib=libc++
    }
}

unix:!symbian:!macx {
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

win32 {
    contains(QMAKE_HOST.arch, x86_64) {
       SwiPl = "C:\Program Files\swipl"
    } else {
       SwiPl = "C:\Program Files (x86)\swipl"
    }
    INCLUDEPATH += $$SwiPl\include
    LIBS += -L$$SwiPl\lib
    win32-msvc*: {
        CONFIG += c++11
        DEFINES += ssize_t=intptr_t
        QMAKE_LFLAGS += libswipl.dll.a
    } else {
        QMAKE_CXXFLAGS += -std=c++0x
        LIBS += -lswipl
    }
}

OTHER_FILES += \
    README.md \
    pqConsole.doxy \
    swipl.png \
    trace_interception.pl

RESOURCES += \
    pqConsole.qrc

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../lqUty/release/ -llqUty
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../lqUty/debug/ -llqUty
else:unix:!symbian: LIBS += -L$$OUT_PWD/../lqUty/ -llqUty

INCLUDEPATH += $$PWD/../lqUty
DEPENDPATH += $$PWD/../lqUty

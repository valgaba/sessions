QT += core gui widgets testlib svg
VERSION = 1.0.0
RC_FILE = sessions.rc
win32:exists(C:/msys64/mingw64/bin/windres.exe) {
    QMAKE_RC = C:/msys64/mingw64/bin/windres.exe --preprocessor=C:/Qt/Tools/mingw1120_64/bin/g++.exe --preprocessor-arg=-E --preprocessor-arg=-xc++ --preprocessor-arg=-DRC_INVOKED
} else:win32:exists($$PWD/../out/qt67-binutils/windres.exe) {
    QMAKE_RC = $$shell_quote($$PWD/../out/qt67-binutils/windres.exe) --preprocessor=C:/Qt/Tools/mingw1120_64/bin/g++.exe --preprocessor-arg=-E --preprocessor-arg=-xc++ --preprocessor-arg=-DRC_INVOKED
}
CONFIG += c++17
CONFIG -= console
win32:QMAKE_LFLAGS_CONSOLE = -Wl,-subsystem,windows
TARGET = sessions
SOURCES += main.cpp mainwindow.cpp canvas.cpp smoke.cpp widgetcontainer.cpp propertiesdialog.cpp
HEADERS += mainwindow.h canvas.h widgetcontainer.h propertiesdialog.h
SOURCES += outputview.cpp
HEADERS += outputview.h
SOURCES += countdown.cpp
HEADERS += countdown.h
SOURCES += sourcepanel.cpp
HEADERS += sourcepanel.h
SOURCES += multimedia.cpp
HEADERS += multimedia.h
SOURCES += slider.cpp
HEADERS += slider.h
SOURCES += aboutdialog.cpp
HEADERS += aboutdialog.h
RESOURCES += sessions.qrc
DESTDIR = $$PWD/../out/bin
# Keep intermediates and linker response files local to each build directory.
# Qt Creator and build.ps1 must not share intermediate files.
CONFIG(debug, debug|release) {
    OBJECTS_DIR = $$OUT_PWD/debug/obj
    MOC_DIR = $$OUT_PWD/debug/moc
    RCC_DIR = $$OUT_PWD/debug/rcc
    UI_DIR = $$OUT_PWD/debug/ui
} else {
    OBJECTS_DIR = $$OUT_PWD/release/obj
    MOC_DIR = $$OUT_PWD/release/moc
    RCC_DIR = $$OUT_PWD/release/rcc
    UI_DIR = $$OUT_PWD/release/ui
}

# Use the same private binutils as build.ps1 when compiling in Qt Creator.
win32:exists($$PWD/../out/qt67-binutils/as.exe):exists($$PWD/../out/qt67-binutils/ld.exe) {
    SESSIONS_TOOLS = $$relative_path($$PWD/../out/qt67-binutils, $$OUT_PWD)/
    QMAKE_CXXFLAGS += -B$$shell_quote($$SESSIONS_TOOLS)
    QMAKE_LFLAGS += -B$$shell_quote($$SESSIONS_TOOLS)
}

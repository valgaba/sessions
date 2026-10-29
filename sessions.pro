QT += core gui widgets testlib svg
VERSION = 0.1.0
RC_FILE = sessions.rc
win32:exists(C:/msys64/mingw64/bin/windres.exe) {
    QMAKE_RC = C:/msys64/mingw64/bin/windres.exe --preprocessor=C:/Qt/Tools/mingw1120_64/bin/g++.exe --preprocessor-arg=-E --preprocessor-arg=-xc++ --preprocessor-arg=-DRC_INVOKED
} else:win32:exists($$PWD/build/tools/windres.exe) {
    QMAKE_RC = $$shell_quote($$PWD/build/tools/windres.exe) --preprocessor=C:/Qt/Tools/mingw1120_64/bin/g++.exe --preprocessor-arg=-E --preprocessor-arg=-xc++ --preprocessor-arg=-DRC_INVOKED
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
DESTDIR = $$PWD/out/bin
# Keep intermediates and linker response files local to each build directory.
# Sharing them between Qt Creator (out/) and build.ps1 (build/) breaks paths.
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
win32:exists($$PWD/build/tools/as.exe):exists($$PWD/build/tools/ld.exe) {
    SESSIONS_TOOLS = $$relative_path($$PWD/build/tools, $$OUT_PWD)/
    QMAKE_CXXFLAGS += -B$$shell_quote($$SESSIONS_TOOLS)
    QMAKE_LFLAGS += -B$$shell_quote($$SESSIONS_TOOLS)
}

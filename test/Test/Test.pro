QT += testlib
QT += gui widgets core
CONFIG += qt warn_on depend_includepath testcase

CONFIG += c++2a

TEMPLATE = app

SOURCES +=  tst_langscoreapptest.cpp

DESTDIR     = test_bin
OBJECTS_DIR = build_test
MOC_DIR     = build_test/moc
UI_DIR      = build_test/ui


DEFINES += LANGSCORE_GUIAPP_TEST

SOURCES += \
    ../../MainWindow.cpp \
    ../../src/ui/FormTaskBar.cpp \
    ../../src/ui/AnalyzeDialog.cpp \
    ../../src/ui/WriteModeComponent.cpp \
    ../../src/ui/PackingMode.cpp \
    ../../src/ui/LanguageSelectComponent.cpp \
    ../../src/ui/ScriptViewer.cpp \
    ../../src/invoker.cpp \
    ../../src/settings.cpp \
    ../../src/graphics.hpp \
    ../../src/ui/WriteDialog.cpp

HEADERS += \
    ../../MainWindow.h \
    ../../src/ui/ComponentBase.h \
    ../../src/ui/FormTaskBar.h \
    ../../src/ui/AnalyzeDialog.h \
    ../../src/ui/WriteModeComponent.h \
    ../../src/ui/PackingMode.h \
    ../../src/ui/LanguageSelectComponent.h \
    ../../src/ui/ScriptViewer.h \
    ../../src/csv.hpp \
    ../../src/invoker.h \
    ../../src/settings.h \
    ../../src/ui/WriteDialog.h

FORMS += \
    ../../MainWindow.ui \
    ../../src/ui/WriteDialog.ui \
    ../../src/ui/WriteModeComponent.ui \
    ../../src/ui/PackingMode.ui \
    ../../src/ui/FormTaskBar.ui \
    ../../src/ui/AnalyzeDialog.ui

INCLUDEPATH += ../../src/ui
INCLUDEPATH += ../../src
INCLUDEPATH += ../..

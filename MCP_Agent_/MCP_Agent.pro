QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

DEFINES += APP_SRC_DIR=\\\"$$PWD\\\"

# TDLib (libtdjson) підвантажується під час виконання — див. telegramaccountclient.cpp.

SOURCES += \
    agents.cpp \
    filemanager.cpp \
    jsonparser.cpp \
    main.cpp \
    mainwindow.cpp \
    phonenumber.cpp \
    requests.cpp \
    tgbot.cpp \
    telegramaccountclient.cpp

HEADERS += \
    agents.h \
    filemanager.h \
    jsonparser.h \
    mainwindow.h \
    phonenumber.h \
    requests.h \
    tgbot.h \
    telegramaccountclient.h

FORMS += \
    mainwindow.ui

DISTFILES += \
    prompt.md \
    SYSTEM_PROMPT_REQUIRED.md

unix:!android {
    target.path = /opt/$${TARGET}/bin
    INSTALLS += target
}
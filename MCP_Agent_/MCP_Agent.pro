QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
DEFINES += APP_SRC_DIR=\\\"$$PWD\\\"
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    agents.cpp \
    filemanager.cpp \
    jsonparser.cpp \
    main.cpp \
    mainwindow.cpp \
    phonenumber.cpp \
    requests.cpp \
    skillmanager.cpp \
    telegramaccountclient.cpp \
    tgbot.cpp

HEADERS += \
    agents.h \
    filemanager.h \
    jsonparser.h \
    mainwindow.h \
    phonenumber.h \
    requests.h \
    skillmanager.h \
    telegramaccountclient.h \
    tgbot.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    prompt.md

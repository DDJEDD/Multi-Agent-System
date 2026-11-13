#include "mainwindow.h"
#include "tgbot.h"
#include <QApplication>
#include <QCoreApplication>
#include <QLoggingCategory>
#include "QLabel"
#include <QSslSocket>
#include "skillmanager.h"

int main(int argc, char *argv[])
{
    qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    QApplication a(argc, argv);
    qInfo() << "SSL:" << QSslSocket::supportsSsl() << QSslSocket::sslLibraryVersionString();

    SkillManager manager;
    QStringList list = manager.listPlugins();
    qDebug() << list;
    QString file = manager.getSkilsFile("telegram");
    qDebug() << file;

    agents *sharedAgents = new agents();
    TgBot bot(sharedAgents);
    MainWindow w(sharedAgents);
    QObject::connect(&w, &MainWindow::appAiModelChanged,
                     sharedAgents, &agents::setAiModel);
    QObject::connect(&w, &MainWindow::appAiKeyChanged,
                     sharedAgents, &agents::setAiToken);

    QObject::connect(&w, &MainWindow::appTelegramTokenChanged,
                     &bot, &TgBot::setToken);
    w.show();
    QObject::connect(&w, &MainWindow::appTelegramTokenChanged,
                     &bot, &TgBot::setToken);
    w.show();
    return a.exec();
}

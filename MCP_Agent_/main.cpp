#include "mainwindow.h"
#include "tgbot.h"
#include <QApplication>
#include <QCoreApplication>
#include <QLoggingCategory>
#include "QLabel"
#include <QSslSocket>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    qInfo() << "SSL:" << QSslSocket::supportsSsl() << QSslSocket::sslLibraryVersionString();

    agents *sharedAgents = new agents();
    TgBot bot(sharedAgents);
    MainWindow w(sharedAgents);

    QObject::connect(&w, &MainWindow::appGeminiTokenChanged,
                     sharedAgents, &agents::setGeminiKey);
    QObject::connect(&w, &MainWindow::appTelegramTokenChanged,
                     &bot, &TgBot::setToken);
    w.show();
    return a.exec();
}

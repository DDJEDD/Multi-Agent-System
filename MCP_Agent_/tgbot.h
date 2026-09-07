#ifndef TGBOT_H
#define TGBOT_H
#include <QCoreApplication>
#include <QSslSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QObject>
#include "requests.h"
#include "phonenumber.h"
#include <QMap>
#include "agents.h"
#include "jsonparser.h"
class TgBot : public QObject
{
    Q_OBJECT
    Requests *requests;
    phonenumber *phone;
    agents *m_agents;
    QString token;

    QString host  = "api.telegram.org";
    qint64 offset = 0;

    void poll();
    void checkreqPhoto(const QJsonObject &response, qint64 chatId, const QString &text);
    void sendSticker(qint64 chatId, const QString &stickerId);
    void sendMessagesDelayed(qint64 chatId,const QList<DelayedMessage> &messages);
    void sendTyping(qint64 chatId);
    void sendMessage(qint64 chatId, const QString &text);
    void processPhotoMessage(const QString &fileId, const QString &text, qint64 chatId);
    void sendAnimation(qint64 chatId, const QString &animationUrl, const QString &caption);
    void downloadFile(const QString &fileId, std::function<void(const QByteArray &)> callback);
    void sendPhoto(qint64 chatId, const QString &photoUrl, const QString &caption );
public:
    explicit TgBot(QObject *parent = nullptr);
    void setToken(const QString &token);


signals:
};
#endif // TGBOT_H

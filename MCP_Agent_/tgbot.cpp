#include "tgbot.h"
#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QTimer>
#include <optional>
#include <QSettings>

TgBot::TgBot(QObject *parent) : QObject(parent) {

    phone = new phonenumber(this);
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);

    QString token = settings.value("telegram_token").toString();
    setToken(token.toUtf8());
    if (token.isEmpty()) {
        qWarning() << "ВНИМАНИЕ: telegram_token не найден в config.ini!";
    }
    m_agents = new agents(this);
    connect(m_agents, &agents::requestSendMessagesDelayed,
            this, &TgBot::sendMessagesDelayed);

    connect(m_agents, &agents::requestSendPhoto,
            this, &TgBot::sendPhoto);

    poll();

}

void TgBot::setToken(const QString &key){
    token=key;

}
void TgBot::checkreqPhoto(const QJsonObject &response, qint64 chatId, const QString &prompt) {
    if (response.contains("error")) {
        qWarning() << "Gemini Photo Error:" << response["error"].toObject()["message"].toString();
        sendMessage(chatId, "Не удалось сгенерировать изображение. Попробуйте изменить запрос.");
        return;
    }

    QJsonArray candidates = response["candidates"].toArray();
    if (candidates.isEmpty()) {
        sendMessage(chatId, "ИИ не смог сгенерировать картинку.");
        return;
    }

    QJsonArray parts = candidates[0].toObject()["content"].toObject()["parts"].toArray();
    for (const QJsonValue &val : parts) {
        QJsonObject partObj = val.toObject();

        if (partObj.contains("inline_data")) {
            QString base64Data = partObj["inline_data"].toObject()["data"].toString();


            QString photoUrl = QString("data:image/png;base64,%1").arg(base64Data);

            sendPhoto(chatId, photoUrl, QString("🎨 <i>%1</i>").arg(prompt));
            return;
        }
    }

    sendMessage(chatId, "В ответе ИИ не оказалось изображения.");
}

void TgBot::downloadFile(const QString &fileId, std::function<void(const QByteArray &)> callback) {
    QString path = QString("/bot%1/getFile").arg(token);
    QJsonObject body{{"file_id", fileId}};

    requests->apiCall(this, host, path,"POST" ,body, {}, [this, callback](const QJsonObject &resp) {
        if (!resp.value("ok").toBool()) {
            qWarning() << "Ошибка получения файла из Telegram:" << resp;
            callback(QByteArray());
            return;
        }

        QString filePath = resp["result"].toObject()["file_path"].toString();
        if (filePath.isEmpty()) {
            qWarning() << "Telegram вернул пустой file_path";
            callback(QByteArray());
            return;
        }

        QString downloadPath = QString("/file/bot%1/%2").arg(token, filePath);

        requests->downloadFileCall(this, host, downloadPath, [callback](const QByteArray &fileData) {
            callback(fileData);
        });
    });
}

void TgBot::processPhotoMessage(const QString &fileId, const QString &text, qint64 chatId) {
    qDebug() << "Получено изображение:" << fileId << "Чат:" << chatId << "Подпись:" << text;

    downloadFile(fileId, [this, text, chatId](const QByteArray &imageData) {
        if (imageData.isEmpty()) {
            qWarning() << "Не удалось загрузить изображение";
            sendMessage(chatId, "Ошибка при обработке изображения.");
            return;
        }


        m_agents->reqAgent(text.isEmpty() ? "Что на этой картинке?" : text,  chatId,"Главный агент",MessageSource::Telegram,0 ,imageData);
    });
}


void TgBot::poll() {
    const QString path = QString("/bot%1/getUpdates").arg(token);
    QJsonObject body{{"timeout", 30}, {"offset", offset}};

    requests->apiCall(this, host, path,"POST" ,body, {}, [this](const QJsonObject &resp) {
        if (!resp.value("ok").toBool()) {
            qWarning() << "getUpdates вернул ошибку:" << resp;
            poll();
            return;
        }

        for (const auto &v : resp.value("result").toArray()) {
            const auto update = v.toObject();
            offset = update["update_id"].toInteger() + 1;

            const auto message = update["message"].toObject();
            if (message.isEmpty())
                continue;

            const qint64 chatId = message["chat"].toObject()["id"].toInteger();
            const QString text = message["caption"].toString().isEmpty()
                                     ? message["text"].toString()
                                     : message["caption"].toString();

            if (message.contains("photo") && message["photo"].isArray()) {
                QJsonArray photoArray = message["photo"].toArray();
                if (!photoArray.isEmpty()) {
                    QJsonObject largestPhoto = photoArray.last().toObject();
                    QString fileId = largestPhoto["file_id"].toString();
                    processPhotoMessage(fileId, text, chatId);
                    continue;
                }
            }

            if (chatId != 0 && !text.isEmpty())
                if (text.startsWith("/start")) {
                    sendMessage(chatId, "На связи Олег Сигмов, senior AI-ассистент по разработке, DevOps и системной инженерии из Sigmov LTD. А ещё у нас на вооружении появилась новая фича — команда /generate. Напиши её, опиши задачу, и я сгенерирую тебе сочный арт, техническую схему или архитектурный концепт.");
                } else {

                    sendTyping(chatId);
                    m_agents->reqAgent(text, chatId, "Главный агент", MessageSource::Telegram);
                }
        }
        QTimer::singleShot(0, this, &TgBot::poll);
    });
}

void TgBot::sendMessage(qint64 chatId, const QString &text) {
    const QString path = QString("/bot%1/sendMessage").arg(token);
    QJsonObject body{{"chat_id", chatId}, {"text", text}, {"parse_mode", "HTML"}};
    requests->apiCall(this, host, path,"POST" ,body, {}, [](const QJsonObject &) {});
}

void TgBot::sendAnimation(qint64 chatId, const QString &animationUrl, const QString &caption) {
    const QString path = QString("/bot%1/sendAnimation").arg(token);
    QJsonObject body{
        {"chat_id", chatId},
        {"animation", animationUrl},
        {"caption", caption},
        {"parse_mode", "HTML"}
    };

    requests->apiCall(this, host, path,"POST" ,body, {}, [this, chatId, caption](const QJsonObject &resp) {
        if (!resp.value("ok").toBool()) {
            qWarning() << "sendAnimation failed:" << resp["description"].toString();
            qWarning() << "Falling back to sending text message only...";

            sendMessage(chatId, caption);
        } else {
            qDebug() << "Animation sent successfully!";
        }
    });
}
void TgBot::sendPhoto(qint64 chatId, const QString &photoUrl, const QString &caption)
{
    const QString path = QString("/bot%1/sendPhoto").arg(token);

    QJsonObject body{
        {"chat_id", chatId},
        {"photo", photoUrl},
        {"caption", caption},
        {"parse_mode", "HTML"}
    };

    requests->apiCall(this,host,path,"POST",body,{},[this, chatId, caption](const QJsonObject &resp) {

        if (!resp.value("ok").toBool()) {
            qWarning() << "sendPhoto failed:"
                       << resp["description"].toString();

            qWarning() << "Falling back to sending text message only...";

            sendMessage(chatId, caption);
        } else {
            qDebug() << "Photo sent successfully!";
        }
    }
                      );
}
void TgBot::sendSticker(qint64 chatId, const QString &stickerId)
{
    const QString path = QString("/bot%1/sendSticker").arg(token);

    QJsonObject body{
        {"chat_id", chatId},
        {"sticker", stickerId}
    };

    requests->apiCall( this, host, path,"POST",body,{}, [](const QJsonObject &resp) {
        if (!resp.value("ok").toBool()) {
            qWarning() << "sendSticker failed:"
                       << resp["description"].toString();
        }
    }
                      );
}
void TgBot::sendTyping(qint64 chatId) {
    const QString path = QString("/bot%1/sendChatAction").arg(token);

    QJsonObject body{
        {"chat_id", chatId},
        {"action", "typing"}
    };

    requests->apiCall(this, host, path, "POST",body, {}, [](const QJsonObject &) {});
}void TgBot::sendMessagesDelayed(qint64 chatId, const QList<DelayedMessage> &messages)
{
    if (messages.isEmpty()) return;

    auto index = std::make_shared<int>(0);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);

    auto sendNext = [this, chatId, messages, index, timer]() mutable {
        if (*index >= messages.size()) {
            timer->deleteLater();
            return;
        }

        const DelayedMessage &msg = messages[*index];

        if (!msg.stickerId.isEmpty()) {
            qDebug() << "[TgBot] Отправляем стикер:" << msg.stickerId;
            sendSticker(chatId, msg.stickerId);
        }
        else if (!msg.photoUrl.isEmpty()) {
            qDebug() << "[TgBot] Отправляем фото:" << msg.photoUrl;
            sendPhoto(chatId, msg.photoUrl,msg.text);
        }
        else if (!msg.text.isEmpty()) {
            qDebug() << "[TgBot] Отправляем текст:" << msg.text;
            sendMessage(chatId, msg.text);
        }

        (*index)++;

        if (*index >= messages.size()) {
            timer->deleteLater();
            return;
        }

        int nextDelayMs = messages[*index].delay > 0 ? messages[*index].delay * 1000 : 0;
        timer->start(nextDelayMs);
    };

    connect(timer, &QTimer::timeout, this, sendNext);
    sendNext();
}

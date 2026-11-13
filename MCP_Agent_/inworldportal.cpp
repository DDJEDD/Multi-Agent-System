#include "inworldportal.h"
#include "requests.h"
#include <QJsonDocument>
#include <QDebug>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include "agents.h"
InworldPortalClient::InworldPortalClient(QObject *parent)
    : QObject(parent)
{
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    apiKey = settings.value("inworld_api_key", "").toString();

    // Хост и путь из вашего эндпоинта Inworld
    host = settings.value("inworld_host", "api.inworld.ai").toString();
    path = settings.value("inworld_path", "/v1/voice/generate").toString();
}

bool InworldPortalClient::saveAudioFile(const QString &fileName, const QByteArray &audioData)
{
    if (audioData.isEmpty()) {
        qWarning() << "[InworldPortal] Попытка сохранить пустой аудиофайл!";
        return false;
    }

    QString dirPath = QCoreApplication::applicationDirPath() + "/Audio";
    QDir().mkpath(dirPath);

    QString targetName = fileName;
    if (!targetName.endsWith(".mp3", Qt::CaseInsensitive)) {
        targetName += ".mp3";
    }

    QString fullPath = dirPath + "/" + targetName;
    QFile file(fullPath);

    if (file.open(QIODevice::WriteOnly)) {
        file.write(audioData);
        file.close();
        qDebug() << "[InworldPortal] Аудиофайл успешно сохранен:" << fullPath;
        return true;
    } else {
        qWarning() << "[InworldPortal] Ошибка открытия файла для записи:" << fullPath;
        return false;
    }
}

void InworldPortalClient::generateVoice(const QString &text,
                                        const QString &voiceId,
                                        const QString &modelId,
                                        const QString &audioEncoding,
                                        const QString &fileName,
                                        std::function<void(const QByteArray &audioData)> callback)
{
    if (apiKey.isEmpty()) {
        qWarning() << "[InworldPortal] Ошибка: Inworld API Key не задан в config.ini!";
        callback(QByteArray());
        return;
    }

    QJsonObject body;
    body["text"] = text;
    body["voiceId"] = voiceId.isEmpty() ? "Ashley" : voiceId;
    body["modelId"] = modelId.isEmpty() ? "inworld-tts-2" : modelId;

    QJsonObject audioConfig;
    audioConfig["audioEncoding"] = audioEncoding.isEmpty() ? "MP3" : audioEncoding;
    body["audioConfig"] = audioConfig;

    QMap<QString, QString> headers;
    headers.insert("Authorization", "Bearer " + apiKey);

    qDebug() << "[InworldPortal] Отправка запроса на озвучку текста (файл:" << fileName << ")...";

    Requests::apiCall(this, host, path, "POST", body, headers, [this, fileName, callback](const QJsonObject &response) {
        if (response.contains("is_html") && response.value("is_html").toBool()) {
            qWarning() << "[InworldPortal] Ошибка: сервер вернул HTML вместо аудио/JSON";
            callback(QByteArray());
            return;
        }

        if (response.contains("audioContent")) {
            QByteArray audioBytes = QByteArray::fromBase64(response["audioContent"].toString().toLatin1());

            if (!fileName.isEmpty()) {
                saveAudioFile(fileName, audioBytes);
            }

            callback(audioBytes);
        } else {
            qWarning() << "[InworldPortal] Ответ сервера не содержит 'audioContent'";
            callback(QByteArray());
        }
    });
}


void InworldPortalClient::handleTTSRequest(const QVariantMap &args, qint64 chatId,
                                           const QString &callerAgentName, MessageSource source, agents *agentContext)
{
    QString text = args.value("text").toString().trimmed();
    QString voiceId = args.value("voiceId", args.value("voice_id", "Ashley")).toString().trimmed();
    QString modelId = args.value("modelId", args.value("model_id", "inworld-tts-2")).toString().trimmed();
    QString audioEncoding = args.value("audioEncoding", args.value("audio_encoding", "MP3")).toString().trimmed();
    QString fileName = args.value("fileName", args.value("file_name", "output_audio")).toString().trimmed();

    if (text.isEmpty()) {
        qWarning() << "[InworldPortal] Ошибка: Текст для озвучки не указан в ReqTTSAudio!";
        return;
    }

    generateVoice(text, voiceId, modelId, audioEncoding, fileName,
                  [agentContext, chatId, callerAgentName, source](const QByteArray &audioData) {
                      if (!audioData.isEmpty()) {
                          QString resultMsg = QString("Успешно сгенерирована и сохранена озвучка (%1 байт).").arg(audioData.size());
                          agentContext->reqAgent(resultMsg, chatId, callerAgentName, source, 0);
                      } else {
                          QString errorMsg = "Ошибка: не удалось сгенерировать озвучку через Inworld API.";
                          qWarning() << "[InworldPortal]" << errorMsg;
                          agentContext->reqAgent(errorMsg, chatId, callerAgentName, source, 0);
                      }
                  }
                  );
}
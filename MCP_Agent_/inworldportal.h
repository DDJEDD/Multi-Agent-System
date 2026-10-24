#ifndef INWORLDPORTAL_H
#define INWORLDPORTAL_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QVariantMap>
#include <functional>

class agents;
enum class MessageSource;

class InworldPortalClient : public QObject
{
    Q_OBJECT
public:
    explicit InworldPortalClient(QObject *parent = nullptr);

    void generateVoice(const QString &text,
                       const QString &voiceId,
                       const QString &modelId,
                       const QString &audioEncoding,
                       const QString &fileName,
                       std::function<void(const QByteArray &audioData)> callback);

    bool saveAudioFile(const QString &fileName, const QByteArray &audioData);

    void handleTTSRequest(const QVariantMap &args, qint64 chatId,
                          const QString &callerAgentName, MessageSource source, agents *agentContext);

private:
    QString apiKey;
    QString host;
    QString path;
};

#endif // INWORLDPORTAL_H
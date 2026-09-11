#ifndef AGENTS_H
#define AGENTS_H

#include <QString>
#include <QObject>
#include "jsonparser.h"
#include "phonenumber.h"
#include "requests.h"
static constexpr int kMaxRetries = 5;
static constexpr int kRetryBaseDelayMs = 1000;

namespace Constants {
inline constexpr QStringView RoleOrchestrator = u"orchestrator";
}

struct AggregatedMessages {
    QList<DelayedMessage> messages;
    QString fullAiResponse;
};
enum class MessageSource {
    Telegram,
    UI
};
class agents : public QObject
{
    Q_OBJECT
private:
    QString geminiKey;
    phonenumber *number;
    Requests *requests;
public:
    explicit agents(QObject *parent = nullptr);

    void createAgent(const QString &name, const QString &purpose);
    bool deleteAgent(const QString &name);
    QString getAgentPath(const QString &agentName) const;
    void changeAgentName(const QString &oldName,  const QString &newName);
    void editFile(const QString &agentName, const QString &content, const QString &fileToEdit);
    QString getFile(const QString &agentName,  const QString &fileToGet) const;
    QString getFullPrompt(const QString &agentName) const;
    QStringList listAgents() const;
    void executeCall(const QString &id, const QString &agentName, const QVariantMap &args, const QString &functionName, qint64 chatId, const QString &userText, const QString &role, MessageSource source);
    void reqAgent(const QString &userText, qint64 chatId,const QString &agentName, MessageSource source,int retryCount = 0 ,const QByteArray &imageData = QByteArray()  );
    bool RetryReq(const QJsonObject &response,int retryCount,const QString &userText, qint64 chatId,const QString &agentName,MessageSource source, const QByteArray &imageData = QByteArray());
    void checkreq(const QJsonObject &response, qint64 chatId, const QString &text, const QMap<QString, QString> &nums, MessageSource source );
    void setGeminiKey(const QString &Key);
signals:
    void requestSendMessagesDelayed(qint64 chatId, const QList<DelayedMessage> &messages);
    void requestThinkingContext(const QString &type, const QString &message );
    void requestSendPhoto(qint64 chatId, const QString &photoUrl, const QString &caption);
    void requestSendSticker(qint64 chatId, const QString &stickerId);
    void requestError(const QString &ErrorType, const QString &ErrorMsg);
    void requestGeminiLog(const QString &aiText);
    void requestMessageToUI(const QList<DelayedMessage> &messages );
    void requestSendMessagesToUIDelayed(qint64 chatId, const QList<DelayedMessage> delayedMsgs);
};

#endif // AGENTS_H

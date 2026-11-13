#ifndef AGENTS_H
#define AGENTS_H

#include <QString>
#include <QObject>
#include <QHash>
#include <QSet>
#include <QStringList>
#include "jsonparser.h"
#include "phonenumber.h"
#include "skillmanager.h"
#include "telegramaccountclient.h"

// ---------------------------------------------------------------------------
// HARDCODE: все константы ниже стоит вынести в config.ini
// ---------------------------------------------------------------------------
static constexpr int kMaxRetries = 5;              // HARDCODE
static constexpr int kRetryBaseDelayMs = 1000;     // HARDCODE
static constexpr int kMaxToolDepth = 12;           // HARDCODE: макс. длина цепочки "инструмент -> модель -> инструмент" на один чат
static constexpr int kMaxToolResultChars = 4000;   // HARDCODE: обрезка результатов инструментов
static constexpr int kMaxMessageDelayMs = 30000;   // HARDCODE: потолок delay в сообщениях модели
static constexpr int kCmdTimeoutMs = 15000;        // HARDCODE: таймаут reqCMD
static constexpr int kTgTimeoutMs = 20000;         // HARDCODE: таймаут ответа Telegram
static constexpr int kTgMaxHistory = 50;           // HARDCODE
static constexpr int kTgDefaultLimit = 20;         // HARDCODE

namespace Constants {
inline constexpr QStringView RoleOrchestrator = u"orchestrator";
inline constexpr QStringView RoleSubagent = u"subagent";
// HARDCODE: имя главного агента
inline const QString kMainAgentName = QStringLiteral("Главный агент");
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
    QString m_aiKey;        // OpenRouter API key
    QString m_aiModel;
    phonenumber *number;
    SkillManager *skillManager;

    QHash<QString, TelegramAccountClient*> m_tgClients;
    QHash<qint64, int> m_toolDepth;
    QHash<qint64, QString> m_lastUserText;
    QSet<qint64> m_tgAllowedUsers;
    QStringList m_cmdAllowlist;
    QString AiModel;
    int kAiMaxTokens = 4096;
    TelegramAccountClient *tgClientFor(const QString &agentName);
    bool handleCoreFunction(const QString &functionName, const QString &callerAgentName,
                            const QVariantMap &args, const QString &role,
                            qint64 chatId, const QString &userText, MessageSource source);

    void handleMessagesPayload(const QVariantMap &args, qint64 chatId,
                               const QString &callerAgentName, const QString &userText,
                               MessageSource source);
    void startTelegramListening();

    void sendRequest(const QString &userText, qint64 chatId, const QString &agentName,
                     MessageSource source, int retryCount, const QByteArray &imageData);
    void continueChain(const QString &text, qint64 chatId, const QString &agentName,
                       MessageSource source, const QByteArray &imageData = QByteArray());
    void toolResult(const QString &label, const QString &text, qint64 chatId,
                    const QString &agentName, MessageSource source);
    QString roleFor(const QString &agentName) const;

public:
    explicit agents(QObject *parent = nullptr);

    static bool isValidName(const QString &name);
    void setAiModel(const QString &model);
    void SetkAiMaxTokens(int tokenum);
    void createAgent(const QString &name, const QString &purpose);
    bool deleteAgent(const QString &name);
    QString getAgentPath(const QString &agentName) const;
    void changeAgentName(const QString &oldName, const QString &newName);
    void editFile(const QString &agentName, const QString &content, const QString &fileToEdit);
    QString getFile(const QString &agentName, const QString &fileToGet) const;
    QString getFullPrompt(const QString &agentName) const;
    QStringList listAgents() const;
    void runPluginFunction(const QString &fn, const QVariantMap &args, qint64 chatId, const QString &caller, MessageSource source);
    void executeCall(const QString &id, const QString &agentName, const QVariantMap &args, const QString &functionName, qint64 chatId, const QString &userText, const QString &role, MessageSource source);
    void reqAgent(const QString &userText, qint64 chatId, const QString &agentName, MessageSource source, int retryCount = 0, const QByteArray &imageData = QByteArray());
    bool RetryReq(const QJsonObject &response, int retryCount, const QString &userText, qint64 chatId, const QString &agentName, MessageSource source, const QByteArray &imageData = QByteArray());
    void checkreq(const QJsonObject &response, qint64 chatId, const QString &text,  const MaskTable &table, MessageSource source, const QString &agentName);
    void setAiToken(const QString &apiToken);

signals:
    void requestSendMessagesDelayed(qint64 chatId, const QList<DelayedMessage> &messages);
    void requestThinkingContext(const QString &type, const QString &message);
    void requestSendPhoto(qint64 chatId, const QString &photoUrl, const QString &caption);
    void requestSendSticker(qint64 chatId, const QString &stickerId);
    void requestError(const QString &ErrorType, const QString &ErrorMsg);
    void requestGeminiLog(const QString &aiText);   // TODO: переименовать (остался от Gemini), заодно поправить в подписчиках
    void requestMessageToUI(const QList<DelayedMessage> &messages);
    void requestSendMessagesToUIDelayed(qint64 chatId, const QList<DelayedMessage> delayedMsgs);
};

#endif // AGENTS_H
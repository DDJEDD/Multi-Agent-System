#ifndef AGENTS_H
#define AGENTS_H

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include "jsonparser.h"
#include "phonenumber.h"
#include "skillmanager.h"
#include "telegramaccountclient.h"
#include "aimanager.h"
//потом винесу в конфіг, зайбався уже
static constexpr int kMaxRetries = 5;
static constexpr int kRetryBaseDelayMs = 1000;
static constexpr int kMaxToolDepth = 50;
static constexpr int kMaxToolResultChars = 4000;
static constexpr int kMaxMessageDelayMs = 30000;
static constexpr int kCmdTimeoutMs = 15000;
static constexpr int kTgTimeoutMs = 20000;
static constexpr int kTgMaxHistory = 50;
static constexpr int kTgDefaultLimit = 20;

static constexpr int kPinMinChars = 1500;
static constexpr int kPinMaxChars = 12000;
static constexpr int kMaxActionLog = 12;
static constexpr int kMaxGuardTries = 2;
static constexpr int kFilesCatalogLimit = 30;

namespace Constants {
inline constexpr QStringView RoleOrchestrator = u"orchestrator";
inline constexpr QStringView RoleSubagent = u"subagent";

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
    AiManager *m_ai = nullptr;
    phonenumber *number;
    SkillManager *skillManager;

    QHash<QString, TelegramAccountClient*> m_tgClients;
    QHash<qint64, int> m_toolDepth;
    QHash<qint64, QString> m_lastUserText;
    QSet<qint64> m_tgAllowedUsers;
    QStringList m_cmdAllowlist;
    int kAiMaxTokens = 4096;

    QHash<QString, QJsonArray> m_chain;
    QHash<QString, QList<QPair<QString, QString>>> m_history;
    QHash<qint64, MaskTable> m_maskTables;
    QHash<QString, int> m_batchPending;
    QHash<QString, QStringList> m_batchResults;

    QSet<QString> m_delegated;
    QSet<QString> m_finished;
    QHash<qint64, QString> m_pendingCode;

    QHash<qint64, QString> m_pinned;
    QHash<qint64, bool> m_pinnedLoaded;
    QHash<qint64, QStringList> m_turnCalls;
    QHash<qint64, QStringList> m_actionLog;
    QHash<qint64, int> m_guardTries;

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
    void deliver(const QString &block, qint64 chatId, const QString &agentName,
                 MessageSource source);
    void toolResult(const QString &label, const QString &text, qint64 chatId,
                    const QString &agentName, MessageSource source);
    QString buildPluginsCatalog() const;
    QString roleFor(const QString &agentName) const;

    QString maskKeep(const QString &text, MaskTable &table);
    QString buildFilesCatalog() const;
    QString pinnedFor(qint64 chatId);
    void setPinned(qint64 chatId, const QString &text);
    void resetChatState(qint64 chatId);

public:
    explicit agents(QObject *parent = nullptr);
    void reloadAi() { if (m_ai) m_ai->reload(); }
    void setAiToken(const QString &) { reloadAi(); }
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
    void checkreq(const QJsonObject &response, qint64 chatId, const QString &text, const MaskTable &table, MessageSource source, const QString &agentName);

signals:
    void requestSendMessagesDelayed(qint64 chatId, const QList<DelayedMessage> &messages);
    void requestThinkingContext(const QString &type, const QString &message);
    void requestSendPhoto(qint64 chatId, const QString &photoUrl, const QString &caption);
    void requestSendSticker(qint64 chatId, const QString &stickerId);
    void requestError(const QString &ErrorType, const QString &ErrorMsg);
    void requestMessageToUI(const QList<DelayedMessage> &messages);
    void requestSendMessagesToUIDelayed(qint64 chatId, const QList<DelayedMessage> delayedMsgs);
};

#endif // AGENTS_H

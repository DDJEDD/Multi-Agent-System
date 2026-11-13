#ifndef TELEGRAMACCOUNTCLIENT_H
#define TELEGRAMACCOUNTCLIENT_H

#ifndef TELEGRAMACCOUNTCLIENT_H
#define TELEGRAMACCOUNTCLIENT_H

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
class QWidget;
class TelegramAccountClient : public QObject
{
    Q_OBJECT

public:
    explicit TelegramAccountClient(QWidget *dialogParent = nullptr,
                                   QObject *parent = nullptr);
    ~TelegramAccountClient() override;
    explicit TelegramAccountClient(QWidget *dialogParent = nullptr,
                                   QObject *parent = nullptr);
    ~TelegramAccountClient() override;

    bool isReady() const;
    QString sessionDirectory() const;

    void startAuthorization(qint32 apiId,
                            const QString &apiHash,
                            const QString &phoneNumber,
                            const QString &m_sessionDirectory);
    void submitCode(const QString &code);
    void submitPassword(const QString &password);
    void logout();
    bool startForAgent(const QString &agentName);
    void requestChats(int limit = 100);
    void requestMessages(qint64 chatId, qint64 fromMessageId = 0, int limit = 50);
    void sendMessage(qint64 chatId, const QString &text);
    void sendHistoryRequest(qint64 chatId, qint64 fromMessageId, int limit, int attempt);
    static QString messagePreview(const QJsonObject &message);
    bool isStarted() const { return m_started; }
    void requestProfile();
    void setName(const QString &firstName, const QString &lastName);
    void setUsername(const QString &username);

signals:
    void statusChanged(const QString &status);
    void authorizationError(const QString &message);
    void authorizationReady();
    void loggedOut();
    void userMessageReceived(qint64 chatId, qint64 userId, const QString &text,
                             const QJsonObject &message);
    void operationFailed(const QString &message);
    void chatsLoaded(const QJsonArray &chats);
    void messagesLoaded(qint64 chatId, const QJsonArray &messages);
    void messageReceived(const QJsonObject &message);
    void messageSent(const QJsonObject &message);
    void profileLoaded(const QJsonObject &profile);

private:
    enum class RequestKind {
        LoadChats, GetChats, Messages, SendMessage, GetMe, SetName, SetUsername,
        OpenChat
    };

    struct PendingRequest
    {
        RequestKind kind = RequestKind::LoadChats;
        qint64 chatId = 0;
        int limit = 0;
        qint64 fromMessageId = 0;
        int attempt = 0;
    };

    void setStatus(const QString &status);
    void showError(const QString &message);
    void send(const QJsonObject &request);
    void sendTracked(QJsonObject request, const PendingRequest &pending);
    bool ensureReady();

    void receiveTdlibResponse();
    void processResponse(const QString &json);
    void processAuthorizationState(const QJsonObject &state);
    void handleTrackedResponse(const PendingRequest &pending, const QJsonObject &object);
    bool processUpdate(const QString &type, const QJsonObject &object);

    void sendTdlibParameters();
    void sendDatabaseEncryptionKey();
    void sendPhoneNumber();
    void requestCodeFromUser();
    void requestPasswordFromUser();

    static QJsonObject normalizeProfile(const QJsonObject &user);

    QWidget *m_dialogParent = nullptr;
    void *m_client = nullptr;
    QTimer *m_receiveTimer = nullptr;

    qint32 m_apiId = 0;
    QString m_apiHash;
    QString m_phoneNumber;
    QString m_sessionDirectory;
    QHash<qint64, bool> m_isBot;
    bool m_started = false;
    bool m_ready = false;
    bool m_tdlibParametersSent = false;
    bool m_phoneNumberSent = false;
    bool m_codeDialogOpened = false;
    bool m_passwordDialogOpened = false;
    qint64 m_nextExtra = 1;
    QHash<QString, PendingRequest> m_pending;
    QHash<qint64, QJsonObject> m_chats;
};

#endif // TELEGRAMACCOUNTCLIENT_H
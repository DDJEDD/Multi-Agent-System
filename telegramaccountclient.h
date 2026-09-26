#ifndef TELEGRAMACCOUNTCLIENT_H
#define TELEGRAMACCOUNTCLIENT_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QPointer>
#include <QJsonObject>

class QWidget;

extern "C" {
#include <td/telegram/td_json_client.h>
}

class TelegramAccountClient : public QObject
{
    Q_OBJECT

public:
    explicit TelegramAccountClient(QWidget *dialogParent = nullptr,
                                   QObject *parent = nullptr);
    ~TelegramAccountClient() override;

    void startAuthorization(qint32 apiId,
                            const QString &apiHash,
                            const QString &phoneNumber,
                            const QString &sessionDirectory);

    void submitCode(const QString &code);
    void submitPassword(const QString &password);
    void logout();

    bool isReady() const;
    QString sessionDirectory() const;

signals:
    void statusChanged(const QString &status);
    void authorizationReady();
    void authorizationError(const QString &message);
    void loggedOut();

private slots:
    void receiveTdlibResponse();

private:
    void send(const QJsonObject &request);
    void processResponse(const QString &json);
    void processAuthorizationState(const QJsonObject &state);

    void sendTdlibParameters();
    void sendDatabaseEncryptionKey();
    void sendPhoneNumber();

    void requestCodeFromUser();
    void requestPasswordFromUser();

    void showError(const QString &message);
    void setStatus(const QString &status);

    void *m_client = nullptr;

    QTimer *m_receiveTimer = nullptr;
    QPointer<QWidget> m_dialogParent;

    qint32 m_apiId = 0;
    QString m_apiHash;
    QString m_phoneNumber;
    QString m_sessionDirectory;

    bool m_ready = false;
    bool m_started = false;
    bool m_tdlibParametersSent = false;
    bool m_phoneNumberSent = false;
    bool m_codeDialogOpened = false;
    bool m_passwordDialogOpened = false;
};

#endif // TELEGRAMACCOUNTCLIENT_H
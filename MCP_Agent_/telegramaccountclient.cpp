#include "telegramaccountclient.h"

#include <QDir>
#include <QDebug>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLibrary>
#include <QCoreApplication>
#include <QLineEdit>
#include <QMessageBox>
#include <QWidget>

namespace {

// Мінімальний JSON-інтерфейс TDLib (td_json_client.h), який потрібен цьому класу.
struct TdJsonApi
{
    using CreateFn = void *(*)();
    using SendFn = void (*)(void *, const char *);
    using ReceiveFn = const char *(*)(void *, double);
    using DestroyFn = void (*)(void *);

    CreateFn create = nullptr;
    SendFn send = nullptr;
    ReceiveFn receive = nullptr;
    DestroyFn destroy = nullptr;
    QString error;

    bool isLoaded() const { return create && send && receive && destroy; }
};

// Шукає libtdjson (.so / .dylib / .dll) у такому порядку:
// поруч із програмою, у теці проєкту, у системних шляхах, у Homebrew.
const TdJsonApi &tdJson()
{
    static const TdJsonApi api = [] {
        TdJsonApi result;

        const QStringList candidates = {
            QCoreApplication::applicationDirPath() + "/tdjson",
            QCoreApplication::applicationDirPath() + "/../Frameworks/tdjson",
            QString(APP_SRC_DIR) + "/tdjson",
            QStringLiteral("tdjson"),
            QStringLiteral("/opt/homebrew/lib/tdjson"),
            QStringLiteral("/usr/local/lib/tdjson"),
        };

        static QLibrary library;
        for (const QString &name : candidates) {
            library.setFileName(name);
            if (library.load())
                break;
        }

        if (!library.isLoaded()) {
            result.error = QStringLiteral(
                "Не знайдено бібліотеку TDLib (libtdjson). "
                "Покладіть libtdjson поруч із програмою або в теку проєкту.");
            return result;
        }

        result.create = reinterpret_cast<TdJsonApi::CreateFn>(library.resolve("td_json_client_create"));
        result.send = reinterpret_cast<TdJsonApi::SendFn>(library.resolve("td_json_client_send"));
        result.receive = reinterpret_cast<TdJsonApi::ReceiveFn>(library.resolve("td_json_client_receive"));
        result.destroy = reinterpret_cast<TdJsonApi::DestroyFn>(library.resolve("td_json_client_destroy"));

        if (!result.isLoaded())
            result.error = QStringLiteral("Бібліотека TDLib (%1) не містить потрібних функцій td_json_client_*.")
                               .arg(library.fileName());

        return result;
    }();

    return api;
}

QString clientNotCreatedMessage()
{
    return tdJson().error.isEmpty() ? QStringLiteral("TDLib client не створений.") : tdJson().error;
}

} // namespace

TelegramAccountClient::TelegramAccountClient(QWidget *dialogParent,
                                             QObject *parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{
    if (tdJson().isLoaded())
        m_client = tdJson().create();

    if (!m_client) {
        qWarning() << "[Telegram]" << clientNotCreatedMessage();
        return;
    }

    m_receiveTimer = new QTimer(this);
    m_receiveTimer->setInterval(50);

    connect(m_receiveTimer,
            &QTimer::timeout,
            this,
            &TelegramAccountClient::receiveTdlibResponse);
}

TelegramAccountClient::~TelegramAccountClient()
{
    if (m_receiveTimer)
        m_receiveTimer->stop();

    if (m_client) {
        tdJson().destroy(m_client);
        m_client = nullptr;
    }
}

bool TelegramAccountClient::isReady() const
{
    return m_ready;
}

QString TelegramAccountClient::sessionDirectory() const
{
    return m_sessionDirectory;
}

void TelegramAccountClient::setStatus(const QString &status)
{
    qInfo() << "[Telegram]" << status;
    emit statusChanged(status);
}

void TelegramAccountClient::showError(const QString &message)
{
    const QString finalMessage =
        message.trimmed().isEmpty()
            ? QStringLiteral("Невідома помилка Telegram.")
            : message.trimmed();

    qWarning() << "[Telegram]" << finalMessage;

    setStatus("Помилка Telegram");
    emit authorizationError(finalMessage);
}

void TelegramAccountClient::send(const QJsonObject &request)
{
    if (!m_client) {
        showError(clientNotCreatedMessage());
        return;
    }

    const QByteArray requestData =
        QJsonDocument(request).toJson(QJsonDocument::Compact);

    tdJson().send(m_client, requestData.constData());
}

void TelegramAccountClient::startAuthorization(
    qint32 apiId,
    const QString &apiHash,
    const QString &phoneNumber,
    const QString &sessionDirectory)
{
    if (!m_client) {
        showError(clientNotCreatedMessage());
        return;
    }

    if (m_started) {
        showError("Авторизація вже запущена.");
        return;
    }

    if (apiId <= 0) {
        showError("API ID має бути додатним числом.");
        return;
    }

    if (apiHash.trimmed().isEmpty()) {
        showError("API Hash не може бути порожнім.");
        return;
    }

    if (phoneNumber.trimmed().isEmpty()) {
        showError("Номер телефону не може бути порожнім.");
        return;
    }

    if (sessionDirectory.trimmed().isEmpty()) {
        showError("Папка сесії не вказана.");
        return;
    }

    m_apiId = apiId;
    m_apiHash = apiHash.trimmed();
    m_phoneNumber = phoneNumber.trimmed();
    m_sessionDirectory = QDir::cleanPath(sessionDirectory.trimmed());

    QDir sessionDir(m_sessionDirectory);

    if (!sessionDir.exists() && !sessionDir.mkpath(".")) {
        showError(
            "Не вдалося створити папку сесії:\n" +
            m_sessionDirectory);
        return;
    }

    const QString filesDirectory =
        QDir(m_sessionDirectory).filePath("files");

    QDir().mkpath(filesDirectory);

    m_started = true;
    m_ready = false;
    m_tdlibParametersSent = false;
    m_phoneNumberSent = false;
    m_codeDialogOpened = false;
    m_passwordDialogOpened = false;

    m_receiveTimer->start();

    setStatus("Запуск TDLib...");
}

void TelegramAccountClient::receiveTdlibResponse()
{
    if (!m_client)
        return;

    const char *response =
        tdJson().receive(m_client, 0.01);

    if (!response)
        return;

    processResponse(QString::fromUtf8(response));
}

void TelegramAccountClient::processResponse(const QString &json)
{
    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(json.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        qWarning()
        << "[Telegram] Некоректна відповідь TDLib:"
        << parseError.errorString();
        return;
    }

    const QJsonObject object = document.object();
    const QString type = object.value("@type").toString();

    if (type == "error") {
        const int errorCode = object.value("code").toInt();
        const QString errorMessage =
            object.value("message").toString();

        m_codeDialogOpened = false;
        m_passwordDialogOpened = false;

        showError(
            QString("Помилка Telegram %1: %2")
                .arg(errorCode)
                .arg(errorMessage));

        return;
    }

    if (type == "updateAuthorizationState") {
        const QJsonObject state =
            object.value("authorization_state").toObject();

        processAuthorizationState(state);
        return;
    }

    if (type.startsWith("authorizationState")) {
        processAuthorizationState(object);
    }
}

void TelegramAccountClient::processAuthorizationState(
    const QJsonObject &state)
{
    const QString type = state.value("@type").toString();

    if (type == "authorizationStateWaitTdlibParameters") {
        if (!m_tdlibParametersSent) {
            sendTdlibParameters();
            m_tdlibParametersSent = true;
        }

        setStatus("Передача параметрів TDLib...");
        return;
    }

    if (type == "authorizationStateWaitEncryptionKey") {
        sendDatabaseEncryptionKey();
        setStatus("Відкриття локальної сесії...");
        return;
    }

    if (type == "authorizationStateWaitPhoneNumber") {
        if (!m_phoneNumberSent) {
            sendPhoneNumber();
            m_phoneNumberSent = true;
        }

        setStatus("Надсилання запиту коду Telegram...");
        return;
    }

    if (type == "authorizationStateWaitCode") {
        setStatus("Очікується код підтвердження Telegram.");
        requestCodeFromUser();
        return;
    }

    if (type == "authorizationStateWaitPassword") {
        setStatus("Очікується пароль Telegram 2FA.");
        requestPasswordFromUser();
        return;
    }

    if (type == "authorizationStateReady") {
        m_ready = true;
        setStatus("Telegram-акаунт підключено.");
        emit authorizationReady();
        return;
    }

    if (type == "authorizationStateLoggingOut") {
        setStatus("Вихід із Telegram...");
        return;
    }

    if (type == "authorizationStateClosed") {
        m_ready = false;
        m_started = false;

        if (m_receiveTimer)
            m_receiveTimer->stop();

        setStatus("Telegram-сесію закрито.");
        emit loggedOut();
        return;
    }
}

void TelegramAccountClient::sendTdlibParameters()
{
    QJsonObject request;

    request["@type"] = "setTdlibParameters";
    request["database_directory"] = m_sessionDirectory;
    request["files_directory"] =
        QDir(m_sessionDirectory).filePath("files");
    request["database_encryption_key"] = "";
    request["use_message_database"] = true;
    request["use_secret_chats"] = false;
    request["api_id"] = m_apiId;
    request["api_hash"] = m_apiHash;
    request["system_language_code"] = "uk";
    request["device_model"] = "MCP Agent";
    request["system_version"] = "Linux";
    request["application_version"] = "1.0";
    request["enable_storage_optimizer"] = true;
    request["ignore_file_names"] = false;

    // TDLib до 1.8.6 (напр. Homebrew 1.8.0) чекає поля всередині "parameters",
    // новіші версії — на верхньому рівні. Надсилаємо обидва варіанти.
    QJsonObject legacyParameters = request;
    legacyParameters.remove("@type");
    legacyParameters["@type"] = "tdlibParameters";
    request["parameters"] = legacyParameters;

    send(request);
}

void TelegramAccountClient::sendDatabaseEncryptionKey()
{
    QJsonObject request;

    request["@type"] = "checkDatabaseEncryptionKey";
    request["encryption_key"] = "";

    send(request);
}

void TelegramAccountClient::sendPhoneNumber()
{
    QJsonObject settings;

    settings["@type"] =
        "phoneNumberAuthenticationSettings";
    settings["allow_flash_call"] = false;
    settings["allow_missed_call"] = false;
    settings["is_current_phone_number"] = false;
    settings["has_unknown_decline_code"] = false;

    QJsonObject request;

    request["@type"] =
        "setAuthenticationPhoneNumber";
    request["phone_number"] = m_phoneNumber;
    request["settings"] = settings;

    send(request);
}

void TelegramAccountClient::requestCodeFromUser()
{
    if (m_codeDialogOpened)
        return;

    m_codeDialogOpened = true;

    bool accepted = false;

    const QString code =
        QInputDialog::getText(
            m_dialogParent,
            "Код Telegram",
            "Введіть код підтвердження Telegram:",
            QLineEdit::Normal,
            QString(),
            &accepted);

    m_codeDialogOpened = false;

    if (!accepted) {
        showError("Введення коду скасовано.");
        return;
    }

    if (code.trimmed().isEmpty()) {
        showError("Код підтвердження порожній.");
        return;
    }

    submitCode(code.trimmed());
}

void TelegramAccountClient::requestPasswordFromUser()
{
    if (m_passwordDialogOpened)
        return;

    m_passwordDialogOpened = true;

    bool accepted = false;

    const QString password =
        QInputDialog::getText(
            m_dialogParent,
            "Пароль Telegram 2FA",
            "Введіть пароль двофакторної авторизації:",
            QLineEdit::Password,
            QString(),
            &accepted);

    m_passwordDialogOpened = false;

    if (!accepted) {
        showError("Введення пароля скасовано.");
        return;
    }

    if (password.isEmpty()) {
        showError("Пароль 2FA порожній.");
        return;
    }

    submitPassword(password);
}

void TelegramAccountClient::submitCode(const QString &code)
{
    if (!m_started) {
        showError("Авторизація ще не запущена.");
        return;
    }

    if (code.trimmed().isEmpty()) {
        showError("Код підтвердження порожній.");
        return;
    }

    QJsonObject request;

    request["@type"] = "checkAuthenticationCode";
    request["code"] = code.trimmed();

    send(request);
}

void TelegramAccountClient::submitPassword(
    const QString &password)
{
    if (!m_started) {
        showError("Авторизація ще не запущена.");
        return;
    }

    if (password.isEmpty()) {
        showError("Пароль 2FA порожній.");
        return;
    }

    QJsonObject request;

    request["@type"] =
        "checkAuthenticationPassword";
    request["password"] = password;

    send(request);
}

void TelegramAccountClient::logout()
{
    if (!m_client || !m_started)
        return;

    QJsonObject request;
    request["@type"] = "logOut";

    send(request);
}
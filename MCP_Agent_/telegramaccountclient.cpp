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
#include <QSettings>
namespace {

// Мінімальний JSON-інтерфейс TDLib (td_json_client.h), який потрібен цьому класу.
struct TdJsonApi
{
    using CreateFn = void *(*)();
    using SendFn = void (*)(void *, const char *);
    using ReceiveFn = const char *(*)(void *, double);
    using DestroyFn = void (*)(void *);
    using ExecuteFn = const char *(*)(void *, const char *);

    CreateFn create = nullptr;
    SendFn send = nullptr;
    ReceiveFn receive = nullptr;
    DestroyFn destroy = nullptr;
    ExecuteFn execute = nullptr;
    QString error;

    bool isLoaded() const { return create && send && receive && destroy; }
};

// Шукає libtdjson (.so / .dylib / .dll) у такому порядку:
// поруч із програмою, у теці проєкту, у системних шляхах, у Homebrew.
const TdJsonApi &tdJson()
{
    static const TdJsonApi api = [] {
        TdJsonApi result;

#if defined(Q_OS_MACOS)
        const QString fileName = QStringLiteral("libtdjson.dylib");
#elif defined(Q_OS_WIN)
        const QString fileName = QStringLiteral("tdjson.dll");
#else
        const QString fileName = QStringLiteral("libtdjson.so");
#endif
        // Точне ім'я файлу під ОС: інакше QLibrary може схопити чужий
        // libtdjson.so (Linux) поруч і не спробувати .dylib.
        const QStringList candidates = {
            QCoreApplication::applicationDirPath() + "/" + fileName,
            QCoreApplication::applicationDirPath() + "/../Frameworks/" + fileName,
            QString(APP_SRC_DIR) + "/" + fileName,
            fileName,
            QStringLiteral("/opt/homebrew/lib/") + fileName,
            QStringLiteral("/usr/local/lib/") + fileName,
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

        qInfo() << "[Telegram] TDLib:" << library.fileName();

        result.create = reinterpret_cast<TdJsonApi::CreateFn>(library.resolve("td_json_client_create"));
        result.send = reinterpret_cast<TdJsonApi::SendFn>(library.resolve("td_json_client_send"));
        result.receive = reinterpret_cast<TdJsonApi::ReceiveFn>(library.resolve("td_json_client_receive"));
        result.destroy = reinterpret_cast<TdJsonApi::DestroyFn>(library.resolve("td_json_client_destroy"));

        result.execute = reinterpret_cast<TdJsonApi::ExecuteFn>(library.resolve("td_json_client_execute"));

        // Лише помилки TDLib у лог, інакше він засипає консоль службовими рядками.
        if (result.execute)
            result.execute(nullptr, R"({"@type":"setLogVerbosityLevel","new_verbosity_level":1})");

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

// id чатів/користувачів/повідомлень — 64-бітні; читаємо через QVariant, а не через double.
qint64 toId(const QJsonValue &value)
{
    return value.toVariant().toLongLong();
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
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    m_apiHash = settings.value("api_hash").toString();
    m_apiId = settings.value("api_id").toInt();
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
    if (sessionDirectory.trimmed().isEmpty()) {
        showError("Папка сесії не вказана.");
        return;
    }

    const QDir savedDir(sessionDirectory.trimmed());
    const bool hasSavedSession = savedDir.exists("td.binlog") || savedDir.exists("db.sqlite");

    if (phoneNumber.trimmed().isEmpty() && !hasSavedSession) {
        showError("Номер телефону не може бути порожнім.");
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
    m_pending.clear();
    m_chats.clear();

    m_receiveTimer->start();

    setStatus("Запуск TDLib...");
}
bool TelegramAccountClient::startForAgent(const QString &agentName)
{
    const QString name = agentName.trimmed();
    if (name.isEmpty()) {
        showError("Ім'я агента не вказане.");
        return false;
    }

    QString safeName = name;
    safeName.replace(' ', '_');

    const QString dir = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + "/sessions/" + safeName);
    const QDir d(dir);
    qInfo() << "[Telegram] session dir =" << dir
            << "binlog =" << d.exists("td.binlog")
            << "sqlite =" << d.exists("db.sqlite");
    if (!d.exists("td.binlog") && !d.exists("db.sqlite")) {
        showError("Для агента «" + name + "» немає збереженої сесії.");
        return false;
    }

    QSettings cfg(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    const qint32  apiId   = cfg.value("api_id").toInt();
    const QString apiHash = cfg.value("api_hash").toString();

    // Номер не передаємо: сесія вже є
    startAuthorization(apiId, apiHash, QString(), dir);
    return m_started;
}
void TelegramAccountClient::receiveTdlibResponse()
{
    if (!m_client)
        return;

    // Забираємо всі відповіді, що накопичились (TDLib шле десятки оновлень поспіль),
    // але не більше 500 за тік, щоб не блокувати UI.
    for (int i = 0; i < 500 && m_client; ++i) {
        const char *response = tdJson().receive(m_client, 0.0);

        if (!response)
            return;

        processResponse(QString::fromUtf8(response));
    }
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

    // Відповідь на наш запит з @extra (чати, повідомлення, профіль) — обробляємо окремо,
    // включно з помилками, щоб вони не йшли в авторизаційну гілку.
    const QString extra = object.value("@extra").toString();
    if (!extra.isEmpty() && m_pending.contains(extra)) {
        handleTrackedResponse(m_pending.take(extra), object);
        return;
    }

    if (type == "error") {
        const int errorCode = object.value("code").toInt();
        const QString errorMessage =
            object.value("message").toString();

        m_codeDialogOpened = false;
        m_passwordDialogOpened = false;

        // Невірний код або пароль: стан авторизації не змінюється,
        // тому TDLib сам не попросить ще раз — питаємо повторно.
        if (errorMessage == "PHONE_CODE_INVALID" || errorMessage == "PHONE_CODE_EMPTY") {
            QMessageBox::warning(m_dialogParent, "Telegram", "Невірний код. Спробуйте ще раз.");
            QTimer::singleShot(0, this, &TelegramAccountClient::requestCodeFromUser);
            return;
        }

        if (errorMessage == "PASSWORD_HASH_INVALID") {
            QMessageBox::warning(m_dialogParent, "Telegram", "Невірний пароль 2FA. Спробуйте ще раз.");
            QTimer::singleShot(0, this, &TelegramAccountClient::requestPasswordFromUser);
            return;
        }

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
        return;
    }

    processUpdate(type, object);
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
        m_pending.clear();
        m_chats.clear();

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
            "Введіть код, який Telegram надіслав у застосунок\n(чат «Telegram») або в SMS:",
            QLineEdit::Normal,
            QString(),
            &accepted);

    m_codeDialogOpened = false;

    if (!accepted) {
        showError("Введення коду скасовано.");
        return;
    }

    if (code.trimmed().isEmpty()) {
        setStatus("Код порожній — введіть код ще раз.");
        QTimer::singleShot(0, this, &TelegramAccountClient::requestCodeFromUser);
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
        setStatus("Пароль порожній — введіть пароль ще раз.");
        QTimer::singleShot(0, this, &TelegramAccountClient::requestPasswordFromUser);
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

// =====================================================================
//  Нижче — чати, повідомлення, профіль
// =====================================================================

bool TelegramAccountClient::ensureReady()
{
    if (m_ready)
        return true;

    emit operationFailed(QStringLiteral("Telegram-акаунт ще не підключено."));
    return false;
}

void TelegramAccountClient::sendTracked(QJsonObject request, const PendingRequest &pending)
{
    const QString extra = QString::number(m_nextExtra++);
    request["@extra"] = extra;
    m_pending.insert(extra, pending);
    send(request);
}

// ---------- Чати ----------

void TelegramAccountClient::requestChats(int limit)
{
    if (!ensureReady())
        return;

    QJsonObject chatList;
    chatList["@type"] = "chatListMain";

    // Спочатку просимо TDLib підвантажити чати (він надішле updateNewChat),
    // потім читаємо впорядкований список id.
    QJsonObject request;
    request["@type"] = "loadChats";
    request["chat_list"] = chatList;
    request["limit"] = limit;

    PendingRequest pending;
    pending.kind = RequestKind::LoadChats;
    pending.limit = limit;
    sendTracked(request, pending);
}

// ---------- Повідомлення ----------
void TelegramAccountClient::requestMessages(qint64 chatId, qint64 fromMessageId, int limit)
{
    if (!ensureReady())
        return;

    QJsonObject request;
    request["@type"] = "openChat";
    request["chat_id"] = chatId;

    PendingRequest pending;
    pending.kind = RequestKind::OpenChat;
    pending.chatId = chatId;
    pending.fromMessageId = fromMessageId;
    pending.limit = limit;
    sendTracked(request, pending);
}

void TelegramAccountClient::sendHistoryRequest(qint64 chatId, qint64 fromMessageId,
                                               int limit, int attempt)
{
    QJsonObject request;
    request["@type"] = "getChatHistory";
    request["chat_id"] = chatId;
    request["from_message_id"] = fromMessageId;
    request["offset"] = 0;
    request["limit"] = limit;
    request["only_local"] = false;

    PendingRequest pending;
    pending.kind = RequestKind::Messages;
    pending.chatId = chatId;
    pending.limit = limit;
    pending.fromMessageId = fromMessageId;
    pending.attempt = attempt;
    sendTracked(request, pending);
}

void TelegramAccountClient::sendMessage(qint64 chatId, const QString &text)
{
    if (!ensureReady()){
        qDebug() << "phone number: " <<  m_phoneNumber;
        qDebug() << "api id: " << m_apiId;
        return;
    }
    if (text.trimmed().isEmpty()) {
        emit operationFailed(QStringLiteral("Повідомлення порожнє."));
        return;
    }

    QJsonObject formatted;
    formatted["@type"] = "formattedText";
    formatted["text"] = text;

    QJsonObject content;
    content["@type"] = "inputMessageText";
    content["text"] = formatted;

    QJsonObject request;
    request["@type"] = "sendMessage";
    request["chat_id"] = chatId;
    request["input_message_content"] = content;

    qInfo() << "[Telegram] sendMessage -> chat" << chatId;

    PendingRequest pending;
    pending.kind = RequestKind::SendMessage;
    pending.chatId = chatId;
    sendTracked(request, pending);
}

QString TelegramAccountClient::messagePreview(const QJsonObject &message)
{
    const QJsonObject content = message.value("content").toObject();
    const QString type = content.value("@type").toString();

    if (type == "messageText")
        return content.value("text").toObject().value("text").toString();

    const QString caption = content.value("caption").toObject().value("text").toString();

    QString label;
    if (type == "messagePhoto")          label = QStringLiteral("[Фото]");
    else if (type == "messageVideo")     label = QStringLiteral("[Відео]");
    else if (type == "messageDocument")  label = QStringLiteral("[Файл]");
    else if (type == "messageVoiceNote") label = QStringLiteral("[Голосове]");
    else if (type == "messageAudio")     label = QStringLiteral("[Аудіо]");
    else if (type == "messageSticker")   label = QStringLiteral("[Стікер]");
    else if (type == "messageAnimation") label = QStringLiteral("[GIF]");
    else                                 label = QStringLiteral("[Повідомлення]");

    return caption.isEmpty() ? label : label + " " + caption;
}

// ---------- Профіль ----------

void TelegramAccountClient::requestProfile()
{
    if (!ensureReady())
        return;

    QJsonObject request;
    request["@type"] = "getMe";

    PendingRequest pending;
    pending.kind = RequestKind::GetMe;
    sendTracked(request, pending);
}

void TelegramAccountClient::setName(const QString &firstName, const QString &lastName)
{
    if (!ensureReady())
        return;

    if (firstName.trimmed().isEmpty()) {
        emit operationFailed(QStringLiteral("Ім'я не може бути порожнім."));
        return;
    }

    QJsonObject request;
    request["@type"] = "setName";
    request["first_name"] = firstName.trimmed();
    request["last_name"] = lastName.trimmed();

    PendingRequest pending;
    pending.kind = RequestKind::SetName;
    sendTracked(request, pending);
}

void TelegramAccountClient::setUsername(const QString &username)
{
    if (!ensureReady())
        return;

    QString clean = username.trimmed();
    if (clean.startsWith('@'))
        clean.remove(0, 1);

    QJsonObject request;
    request["@type"] = "setUsername";
    request["username"] = clean;

    PendingRequest pending;
    pending.kind = RequestKind::SetUsername;
    sendTracked(request, pending);
}

QJsonObject TelegramAccountClient::normalizeProfile(const QJsonObject &user)
{
    // Новий TDLib: usernames.editable_username; старий: username.
    QString username = user.value("usernames").toObject().value("editable_username").toString();
    if (username.isEmpty())
        username = user.value("username").toString();

    QJsonObject profile;
    profile["id"] = user.value("id");
    profile["first_name"] = user.value("first_name").toString();
    profile["last_name"] = user.value("last_name").toString();
    profile["username"] = username;
    profile["phone_number"] = user.value("phone_number").toString();
    return profile;
}

// ---------- Відповіді на запити з @extra ----------

void TelegramAccountClient::handleTrackedResponse(const PendingRequest &pending,
                                                  const QJsonObject &object)
{
    const QString type = object.value("@type").toString();
    const bool isError = (type == "error");
    const QString errorText = QStringLiteral("Помилка Telegram %1: %2")
                                  .arg(object.value("code").toInt())
                                  .arg(object.value("message").toString());

    qInfo() << "[Telegram] <- tracked kind =" << int(pending.kind) << type
            << object.value("code").toInt() << object.value("message").toString();

    switch (pending.kind) {
    case RequestKind::LoadChats: {

        QJsonObject chatList;
        chatList["@type"] = "chatListMain";

        QJsonObject request;
        request["@type"] = "getChats";
        request["chat_list"] = chatList;
        request["limit"] = pending.limit;

        PendingRequest next;
        next.kind = RequestKind::GetChats;
        next.limit = pending.limit;
        sendTracked(request, next);
        return;
    }

    case RequestKind::GetChats: {
        if (isError) {
            emit operationFailed(errorText);
            return;
        }

        QJsonArray result;
        const QJsonArray ids = object.value("chat_ids").toArray();
        for (const QJsonValue &idValue : ids) {
            const qint64 id = toId(idValue);
            if (m_chats.contains(id))
                result.append(m_chats.value(id));
        }

        emit chatsLoaded(result);
        return;
    }

    case RequestKind::OpenChat: {
        if (isError) {
            emit operationFailed(errorText);
            return;
        }
        sendHistoryRequest(pending.chatId, pending.fromMessageId, pending.limit, 0);
        return;
    }

    case RequestKind::Messages: {
        if (isError) {
            emit operationFailed(errorText);
            return;
        }

        QJsonArray result;
        const QJsonArray messages = object.value("messages").toArray();
        for (const QJsonValue &value : messages) {
            if (value.isObject())
                result.append(value);
        }


        if (result.size() < pending.limit && pending.attempt < 2) {
            const qint64 chatId = pending.chatId;
            const qint64 from = pending.fromMessageId;
            const int limit = pending.limit;
            const int next = pending.attempt + 1;
            QTimer::singleShot(1000, this, [this, chatId, from, limit, next]() {
                sendHistoryRequest(chatId, from, limit, next);
            });
            return;
        }

        emit messagesLoaded(pending.chatId, result);

        QJsonObject close;
        close["@type"] = "closeChat";
        close["chat_id"] = pending.chatId;
        send(close);
        return;
    }

    case RequestKind::SendMessage:
        // Успіх приходить пізніше як updateMessageSendSucceeded;
        // тут — помилка або підтвердження, що TDLib узяв повідомлення в чергу.
        if (isError) {
            qWarning() << "[Telegram] sendMessage error:" << errorText;
            emit operationFailed(errorText);
            return;
        }
        qInfo() << "[Telegram] Повідомлення прийняте TDLib, id ="
                << toId(object.value("id"))
                << "state ="
                << object.value("sending_state").toObject().value("@type").toString();
        return;

    case RequestKind::GetMe:
        if (isError) {
            emit operationFailed(errorText);
            return;
        }
        emit profileLoaded(normalizeProfile(object));
        return;

    case RequestKind::SetName:
    case RequestKind::SetUsername:
        if (isError) {
            // наприклад USERNAME_OCCUPIED, USERNAME_INVALID
            emit operationFailed(errorText);
            return;
        }
        requestProfile(); // перечитуємо профіль і повідомляємо UI
        return;
    }
}

// ---------- Оновлення від TDLib ----------

bool TelegramAccountClient::processUpdate(const QString &type, const QJsonObject &object)
{
    if (type == "updateNewChat") {
        const QJsonObject chat = object.value("chat").toObject();
        m_chats.insert(toId(chat.value("id")), chat);
        return true;
    }

    if (type == "updateChatTitle") {
        const qint64 id = toId(object.value("chat_id"));
        if (m_chats.contains(id)) {
            QJsonObject chat = m_chats.value(id);
            chat["title"] = object.value("title");
            m_chats.insert(id, chat);
        }
        return true;
    }

    if (type == "updateChatLastMessage") {
        const qint64 id = toId(object.value("chat_id"));
        if (m_chats.contains(id)) {
            QJsonObject chat = m_chats.value(id);
            chat["last_message"] = object.value("last_message");
            m_chats.insert(id, chat);
        }
        return true;
    }

    if (type == "updateChatReadInbox") {
        const qint64 id = toId(object.value("chat_id"));
        if (m_chats.contains(id)) {
            QJsonObject chat = m_chats.value(id);
            chat["unread_count"] = object.value("unread_count");
            m_chats.insert(id, chat);
        }
        return true;
    }

    if (type == "updateMessageSendAcknowledged") {
        qInfo() << "[Telegram] Сервер Telegram підтвердив отримання, chat"
                << toId(object.value("chat_id"));
        return true;
    }

    if (type == "updateMessageSendSucceeded") {
        qInfo() << "[Telegram] Повідомлення доставлене, старий id ="
                << toId(object.value("old_message_id"));
        emit messageSent(object.value("message").toObject());
        return true;
    }

    if (type == "updateMessageSendFailed") {
        // Новий TDLib: error.{code,message}; старий: error_code / error_message.
        const QJsonObject err = object.value("error").toObject();
        int code = err.value("code").toInt();
        QString text = err.value("message").toString();
        if (text.isEmpty()) {
            code = object.value("error_code").toInt();
            text = object.value("error_message").toString();
        }

        qWarning() << "[Telegram] sendMessage failed:" << code << text;
        emit operationFailed(
            QStringLiteral("Не вдалося надіслати повідомлення (%1): %2").arg(code).arg(text));
        return true;
    }
    if (type == "updateUser") {
        const QJsonObject user = object.value("user").toObject();
        const bool bot = user.value("type").toObject().value("@type").toString() == "userTypeBot";
        m_isBot.insert(toId(user.value("id")), bot);
        return true;
    }

    if (type == "updateNewMessage") {
        const QJsonObject message = object.value("message").toObject();
        emit messageReceived(message);   // сырой сигнал как раньше

        // только входящие
        if (message.value("is_outgoing").toBool())
            return true;

        // только от пользователя (не от канала/анонимного админа)
        const QJsonObject sender = message.value("sender_id").toObject();
        if (sender.value("@type").toString() != "messageSenderUser")
            return true;

        // только личные чаты
        const qint64 chatId = toId(message.value("chat_id"));
        const QString chatType = m_chats.value(chatId)
                                     .value("type").toObject().value("@type").toString();
        if (chatType != "chatTypePrivate")
            return true;

        const qint64 userId = toId(sender.value("user_id"));

        // служебный аккаунт Telegram (коды входа и т.п.)
        if (userId == 777000)
            return true;

        // боты
        if (m_isBot.value(userId, false))
            return true;

        // только реальный контент, без служебных сообщений
        static const QStringList allowed = {
            "messageText", "messagePhoto", "messageVideo", "messageDocument",
            "messageVoiceNote", "messageAudio", "messageSticker", "messageAnimation"
        };
        const QString contentType = message.value("content").toObject().value("@type").toString();
        if (!allowed.contains(contentType))
            return true;

        // не отвечаем на старые сообщения, пришедшие после запуска
        const qint64 date = message.value("date").toVariant().toLongLong();
        if (QDateTime::currentSecsSinceEpoch() - date > 60)
            return true;

        emit userMessageReceived(chatId, userId, messagePreview(message), message);
        return true;
    }

    return false;
}

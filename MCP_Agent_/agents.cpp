#include "agents.h"
#include "filemanager.h"
#include "requests.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <functional>
#include <memory>
#include <QNetworkReply>
static QString configPath() { return QString(APP_SRC_DIR) + "/config.ini"; }

static QString agentsRoot() { return QCoreApplication::applicationDirPath() + "/Agents"; }
static QString pluginDataRoot() { return QCoreApplication::applicationDirPath() + "/PluginData"; }
static const QString kAiHost = QStringLiteral("openrouter.ai");
static const QString kAiPath = QStringLiteral("/api/v1/chat/completions");
static const QString kAppReferer = QStringLiteral("https://localhost/agents");
static const QString kAppTitle = QStringLiteral("Agents Orchestrator");
static const QStringList kAgentFiles = {"soul", "min_info", "system_prompt"};
static const QStringList kDefaultCmdAllowlist = {"ls", "echo", "pwd", "whoami", "date", "hostname"};
static const QString kDefaultAiModel = QStringLiteral("nvidia/nemotron-3.5-embed-8b:free");
static QString iniString(const QVariant &v)
{
    return v.typeId() == QMetaType::QStringList ? v.toStringList().join(",") : v.toString();
}

static QStringList iniList(const QVariant &v)
{
    QStringList out;
    const QStringList raw = v.typeId() == QMetaType::QStringList
                                ? v.toStringList()
                                : v.toString().split(',', Qt::SkipEmptyParts);
    for (const QString &s : raw) {
        const QString t = s.trimmed();
        if (!t.isEmpty()) out << t;
    }
    return out;
}
static QString describeOpenRouterError(QNetworkReply *reply, const QByteArray &body)
{
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject obj = QJsonDocument::fromJson(body).object();

    if (obj.contains("error")) {
        const QJsonObject e = obj["error"].toObject();
        QString msg = e["message"].toString();


        const QString raw = e["metadata"].toObject()["raw"].toString();
        if (!raw.isEmpty())
            msg += "\n" + raw;

        return QString("OpenRouter %1: %2").arg(e["code"].toInt(http)).arg(msg);
    }
    if (reply->error() != QNetworkReply::NoError)
        return "Сеть: " + reply->errorString();
    return QString();
}
bool agents::RetryReq(const QJsonObject &response, int retryCount, const QString &userText,
                      qint64 chatId, const QString &agentName, MessageSource source,
                      const QByteArray &imageData)
{
    if (!response.contains("error"))
        return false;

void agents::setAiToken(const QString &apiToken)
{
    m_aiKey = apiToken.trimmed();
}
void agents::setAiModel(const QString &model)
{
    const QString m = model.trimmed();
    m_aiModel = m.isEmpty() ? kDefaultAiModel : m;

    QSettings s(configPath(), QSettings::IniFormat);
    if (m.isEmpty()) s.remove("ai_model");
    else             s.setValue("ai_model", m);
}

void agents::SetkAiMaxTokens(int tokenum)
{
    if (tokenum <= 0) return;
    kAiMaxTokens = tokenum;
    QSettings(configPath(), QSettings::IniFormat).setValue("ai_max_tokens", tokenum);
}

static bool isForbiddenHost(const QString &hostIn)
{
    const QString host = hostIn.trimmed().toLower();
    if (host.isEmpty() || host.contains('/') || host.contains('@') || host.contains(' '))
        return true;
    if (host == "localhost" || host.endsWith(".localhost") || host.endsWith(".local")
        || host.endsWith(".internal"))
        return true;

    QHostAddress addr;
    if (addr.setAddress(host)) {
        if (addr.isNull() || addr.isLoopback() || addr.isLinkLocal() || addr.isMulticast()
            || addr.isUniqueLocalUnicast())
            return true;
        static const QList<QPair<QHostAddress, int>> nets = {
            {QHostAddress("0.0.0.0"), 8},   {QHostAddress("10.0.0.0"), 8},
            {QHostAddress("100.64.0.0"), 10}, {QHostAddress("127.0.0.0"), 8},
            {QHostAddress("169.254.0.0"), 16}, {QHostAddress("172.16.0.0"), 12},
            {QHostAddress("192.168.0.0"), 16}};
        for (const auto &n : nets)
            if (addr.isInSubnet(n.first, n.second)) return true;
    }
    return false;
}

static void runCmd(QObject *context, const QString &program, const QStringList &args, int timeoutMs,
                   std::function<void(QString result)> callback)
{
    auto *proc = new QProcess(context);
    auto *timer = new QTimer(proc);
    timer->setSingleShot(true);
    auto done = std::make_shared<bool>(false);
    auto finish = [proc, timer, done, callback](const QString &text) {
        if (*done) return;
        *done = true;
        timer->stop();
        callback(text);
        proc->deleteLater();
    };

    QObject::connect(proc, &QProcess::errorOccurred, proc,
                     [proc, finish](QProcess::ProcessError e) {
                         if (e == QProcess::FailedToStart)
                             finish("ERROR: не удалось запустить программу: " + proc->errorString());
                     });

    QObject::connect(proc, &QProcess::finished, proc,
                     [proc, finish](int code, QProcess::ExitStatus) {
                         const QString out = QString::fromLocal8Bit(proc->readAllStandardOutput()).trimmed();
                         const QString err = QString::fromLocal8Bit(proc->readAllStandardError()).trimmed();
                         QString res = out;
                         if (!err.isEmpty()) res += "\nSTDERR: " + err;
                         res += "\n[exit code: " + QString::number(code) + "]";
                         finish(res);
                     });

    QObject::connect(timer, &QTimer::timeout, proc, [proc, finish]() {
        proc->kill();
        finish("ERROR: команда не завершилась вовремя (timeout)");
    });

    timer->start(timeoutMs);
    proc->start(program, args);
}
                   std::function<void(QString result)> callback)
{
    auto *proc = new QProcess(context);
    auto *timer = new QTimer(proc);
    timer->setSingleShot(true);
    auto done = std::make_shared<bool>(false);

    auto finish = [proc, timer, done, callback](const QString &text) {
        if (*done) return;
        *done = true;
        timer->stop();
        callback(text.left(4000));          // не раздуваем контекст модели
        proc->deleteLater();
    };

    QObject::connect(proc, &QProcess::errorOccurred, proc,
                     [proc, finish](QProcess::ProcessError e) {
                         if (e == QProcess::FailedToStart)
                             finish("ERROR: не удалось запустить программу: " + proc->errorString());
                     });

    QObject::connect(proc, &QProcess::finished, proc,
                     [proc, finish](int code, QProcess::ExitStatus) {
                         const QString out = QString::fromLocal8Bit(proc->readAllStandardOutput()).trimmed();
                         const QString err = QString::fromLocal8Bit(proc->readAllStandardError()).trimmed();
                         QString res = out;
                         if (!err.isEmpty()) res += "\nSTDERR: " + err;
                         res += "\n[exit code: " + QString::number(code) + "]";
                         finish(res);
                     });

    QObject::connect(timer, &QTimer::timeout, proc, [proc, finish]() {
        proc->kill();
        finish("ERROR: команда не завершилась вовремя (timeout)");
    });

    timer->start(timeoutMs);
    proc->start(program, args);
    };

    QObject::connect(proc, &QProcess::errorOccurred, proc,
                     [proc, finish](QProcess::ProcessError e) {
                         if (e == QProcess::FailedToStart)
                             finish("ERROR: не удалось запустить программу: " + proc->errorString());
                     });

    QObject::connect(proc, &QProcess::finished, proc,
                     [proc, finish](int code, QProcess::ExitStatus) {
                         const QString out = QString::fromLocal8Bit(proc->readAllStandardOutput()).trimmed();
                         const QString err = QString::fromLocal8Bit(proc->readAllStandardError()).trimmed();
                         QString res = out;
                         if (!err.isEmpty()) res += "\nSTDERR: " + err;
                         res += "\n[exit code: " + QString::number(code) + "]";
                         finish(res);
                     });

    QObject::connect(timer, &QTimer::timeout, proc, [proc, finish]() {
        proc->kill();
        finish("ERROR: команда не завершилась вовремя (timeout)");
    });

    timer->start(timeoutMs);
    proc->start(program, args);
}

static AggregatedMessages MessageAggregator(const QVariantList &msgList)
{
    AggregatedMessages result;

    for (const QVariant &item : msgList) {
        const QVariantMap msgMap = item.toMap();

        const QString text = msgMap.value("text").toString().trimmed();
        const int delay = qBound(0, msgMap.value("delay", 0).toInt(), kMaxMessageDelayMs);

        QString stickerId;
        if (msgMap.contains("stickerId"))
            stickerId = msgMap.value("stickerId").toString().trimmed();
        else if (msgMap.contains("sticker"))
            stickerId = msgMap.value("sticker").toString().trimmed();

        const QString photoUrl = msgMap.value("photoUrl").toString().trimmed();

        if (text.isEmpty() && stickerId.isEmpty() && photoUrl.isEmpty())
            continue;

        QString code = msgMap.value("code").toString().trimmed();
        if (code.compare("NULL", Qt::CaseInsensitive) == 0 || code.compare("null", Qt::CaseInsensitive) == 0)
            code.clear();

        DelayedMessage dMsg;
        dMsg.text = text;
        dMsg.stickerId = stickerId;
        dMsg.delay = delay;
        dMsg.photoUrl = photoUrl;
        dMsg.code = code;
        result.messages.append(dMsg);

        if (!text.isEmpty()) {
            if (!result.fullAiResponse.isEmpty())
                result.fullAiResponse += "\n";
            result.fullAiResponse += text;
        }
    }

    return result;
}
void agents::handleMessagesPayload(const QVariantMap &args, qint64 chatId,
                                   const QString &callerAgentName, const QString &userText,
                                   MessageSource source)
{
    Q_UNUSED(callerAgentName);
    if (!args.contains("messages"))
        return;

    const AggregatedMessages agg = MessageAggregator(args.value("messages").toList());

    if (!agg.messages.isEmpty() && chatId != 0) {
        if (source == MessageSource::Telegram)
            emit requestSendMessagesDelayed(chatId, agg.messages);
        else if (source == MessageSource::UI)
            emit requestSendMessagesToUIDelayed(chatId, agg.messages);
    }

    if (!agg.fullAiResponse.isEmpty() && chatId != 0) {
        FileManager::SaveMessage(chatId, m_lastUserText.value(chatId, userText), agg.fullAiResponse);
    }
}

        if (targetAgent.isEmpty() || fileToEdit.isEmpty()) {
            qWarning() << "[Agents] Ошибка: Не указан целевой агент или имя файла в editFile!";
            return true;
        }

        emit requestThinkingContext("editFile", "[Agents] Изменение файла " + fileToEdit + " у агента: " + targetAgent);
        editFile(targetAgent, content, fileToEdit);
        return true;
    }

    else if (functionName == "getFile" && isOrch) {
        const QString targetAgent = args.value("agentName", args.value("agent_name", callerAgentName)).toString().trimmed();
        const QString fileToGet = args.value("fileToGet", args.value("file_to_get")).toString().trimmed();

        if (!isValidName(targetAgent) || !kAgentFiles.contains(fileToGet))
            return fail("getFile", "недопустимый агент или имя файла");

        const QString content = getFile(targetAgent, fileToGet);
        toolResult("getFile " + targetAgent + "/" + fileToGet,
                   content.isEmpty() ? "(файл пуст или не найден)" : content,
                   chatId, callerAgentName, source);
        return true;
    }

    else if (functionName == "GetChatTgHistory") {
        const qint64 targetChat = args.value("chatId", args.value("chat_id")).toLongLong();
        const int limit = qBound(1, args.value("limit", kTgDefaultLimit).toInt(), kTgMaxHistory);

        if (targetChat == 0)
            return fail("tg", "не указан chatId");

        auto *cl = tgClientFor(callerAgentName);
        if (!ensureTgStarted(cl, callerAgentName))
            return fail("tg", "у агента нет сессии Telegram");

        QPointer<TelegramAccountClient> clp(cl);
        auto request = [this, clp, targetChat, limit, chatId, callerAgentName, source]() {
            if (!clp) return;
            auto conn = std::make_shared<QMetaObject::Connection>();
            auto done = std::make_shared<bool>(false);

            *conn = connect(clp, &TelegramAccountClient::messagesLoaded, this,
                            [this, conn, done, targetChat, chatId, callerAgentName, source](qint64 id, const QJsonArray &msgs) {
                                if (*done || id != targetChat)
                                    return;
                                *done = true;
                                QObject::disconnect(*conn);

                                QStringList lines;
                                for (const QJsonValue &v : msgs) {
                                    const QJsonObject m = v.toObject();
                                    const QString who = m.value("is_outgoing").toBool() ? "я" : "собеседник";
                                    lines.prepend(who + ": " + TelegramAccountClient::messagePreview(m));
                                }
                                toolResult("tg history (чат " + QString::number(targetChat) + ")",
                                           lines.isEmpty() ? "(пусто)" : lines.join("\n"),
                                           chatId, callerAgentName, source);
                            });

            QTimer::singleShot(kTgTimeoutMs, this, [this, conn, done, chatId, callerAgentName, source]() {
                if (*done) return;
                *done = true;
                QObject::disconnect(*conn);
                toolResult("tg", "ERROR: Telegram не ответил вовремя", chatId, callerAgentName, source);
            });

            clp->requestMessages(targetChat, 0, limit);
        };

        if (cl->isReady())
            request();
        else
            connect(cl, &TelegramAccountClient::authorizationReady, this, request,
                    Qt::SingleShotConnection);
        return true;
    }
        return true;
    }

    else if (functionName == "getPluginSkill") {
        const QString targetPlugin = args.value("pluginName", args.value("plugin_name")).toString().trimmed();

        if (targetPlugin.isEmpty()) {
            qWarning() << "[Agents] Ошибка: Имя плагина не указано в getPluginSkill!";
            return true;
        }

        emit requestThinkingContext("getPluginSkill", "[Agents] Запрос инструкции для плагина: " + targetPlugin);

        const QString skillContent = skillManager->getSkilsFile(targetPlugin);
        if (skillContent.isEmpty())
            return fail("getPluginSkill", QString("Плагин '%1' не найден или у него нет файла SKILL.").arg(targetPlugin));
        continueChain(QString("Успешно получена инструкция для плагина '%1':\n%2").arg(targetPlugin, skillContent),
                      chatId, callerAgentName, source);
        return true;
    }

    else if (functionName == "reqCMD") {
        if (!isOrch)
            return fail("cmd", "reqCMD доступна только главному агенту");

        const QString program = args.value("program").toString().trimmed();
        QStringList cmdArgs;
        for (const QVariant &a : args.value("cmdArgs").toList())
            cmdArgs << a.toString();

        if (program.isEmpty())
            return fail("cmd", "не указана программа (program)");
        if (program.contains('/') || program.contains('\\'))
            return fail("cmd", "указывайте только имя программы, без пути");

        if (!m_cmdAllowlist.contains(program.toLower())) {
            qWarning() << "[Agents] reqCMD заблокирована (нет в белом списке):" << program;
            return fail("cmd", "программа не разрешена политикой безопасности. Разрешены: "
                                   + m_cmdAllowlist.join(", "));
        }

        emit requestThinkingContext("reqCMD", "[Agents] CMD: " + program + " " + cmdArgs.join(" "));

        runCmd(this, program, cmdArgs, kCmdTimeoutMs,
               [this, chatId, callerAgentName, source](QString result) {
                   toolResult("cmd output", result, chatId, callerAgentName, source);
               });
        return true;
    }

    else if (functionName == "SendMsgToTgUser") {
        const QString text = args.value("text").toString().trimmed();
        const qint64 targetChat = args.value("chatId", args.value("chat_id")).toLongLong();

        if (text.isEmpty() || targetChat == 0)
            return fail("tg", "не указан text или chatId");

        auto *cl = tgClientFor(callerAgentName);
        if (!ensureTgStarted(cl, callerAgentName))
            return fail("tg", "у агента нет сессии Telegram");
        if (cl->isReady()) {
            cl->sendMessage(targetChat, text);
        } else {
            QPointer<TelegramAccountClient> clp(cl);
            connect(cl, &TelegramAccountClient::authorizationReady, this,
                    [clp, targetChat, text]() { if (clp) clp->sendMessage(targetChat, text); },
                    Qt::SingleShotConnection);
        }
    else if (functionName == "GetChatTgList") {
        const int limit = qBound(1, args.value("limit", kTgDefaultLimit).toInt(), kTgMaxHistory);

        auto *cl = tgClientFor(callerAgentName);
        if (!ensureTgStarted(cl, callerAgentName))
            return fail("tg", "у агента нет сессии Telegram");

        QPointer<TelegramAccountClient> clp(cl);
        auto request = [this, clp, limit, chatId, callerAgentName, source]() {
            if (!clp) return;
            auto conn = std::make_shared<QMetaObject::Connection>();
            auto done = std::make_shared<bool>(false);

            *conn = connect(clp, &TelegramAccountClient::chatsLoaded, this,
                            [this, conn, done, chatId, callerAgentName, source](const QJsonArray &chats) {
                                if (*done) return;
                                *done = true;
                                QObject::disconnect(*conn);

                                QStringList lines;
                                for (const QJsonValue &v : chats) {
                                    const QJsonObject c = v.toObject();
                                    const QString id = QString::number(c.value("id").toVariant().toLongLong());
                                    lines << id + ": " + c.value("title").toString();
                                }
                                toolResult("tg chats", lines.isEmpty() ? "(пусто)" : lines.join("\n"),
                                           chatId, callerAgentName, source);
                            });

            QTimer::singleShot(kTgTimeoutMs, this, [this, conn, done, chatId, callerAgentName, source]() {
                if (*done) return;
                *done = true;
                QObject::disconnect(*conn);
                toolResult("tg", "ERROR: Telegram не ответил вовремя", chatId, callerAgentName, source);
            });

            clp->requestChats(limit);
        };

        if (cl->isReady())
            request();
        else
            connect(cl, &TelegramAccountClient::authorizationReady, this, request,
                    Qt::SingleShotConnection);
        return true;
    }

    else if (functionName == "reqAgent") {
        const QString targetAgent = args.value("agentName", args.value("agent_name", callerAgentName)).toString().trimmed();

        if (!isOrch && targetAgent != Constants::kMainAgentName) {
            qWarning() << "[Agents] Ошибка безопасности: Субагент" << callerAgentName
                       << "пытается вызвать другого субагента (" << targetAgent
                       << "). Разрешено вызывать только Главного агента!";
            return fail("reqAgent", "субагент может вызывать только главного агента");
        }
        if (targetAgent.isEmpty() || targetAgent == callerAgentName)
            return fail("reqAgent", "агент не указан или вызывает сам себя");
        if (!listAgents().contains(targetAgent))
            return fail("reqAgent", "агент не найден: " + targetAgent);

        const QString prompt = args.value("prompt").toString().trimmed();
        if (prompt.isEmpty())
            return fail("reqAgent", "пустой prompt");

        const QByteArray imageData = QByteArray::fromBase64(
            args.value("photoB64", args.value("photo_b64")).toString().toLatin1());

        qint64 chatID = chatId;
        if (isOrch) {
            const qint64 requested = args.value("chatId", args.value("chat_id")).toLongLong();
            if (requested != 0) chatID = requested;
        }

        continueChain(prompt, chatID, targetAgent, source, imageData);
        return true;
    }
            qWarning() << "[Agents] Ошибка безопасности: Субагент" << callerAgentName
                       << "пытается вызвать другого субагента (" << targetAgent
                       << "). Разрешено вызывать только Главного агента!";
    else if (functionName == "reqCMD") {
        const QString shellCmd = args.value("command").toString().trimmed();
        QString program = args.value("program").toString().trimmed();
        QStringList cmdArgs;
        for (const QVariant &a : args.value("cmdArgs").toList())
            cmdArgs << a.toString();

        if (!shellCmd.isEmpty()) {
#ifdef Q_OS_WIN
            program = "cmd";
            cmdArgs = QStringList{"/c", shellCmd};
#else
            program = "/bin/sh";
            cmdArgs = QStringList{"-c", shellCmd};
#endif
        }

        if (program.isEmpty()) {
            qWarning() << "[Agents] reqCMD: не указана программа или command";
            reqAgent("cmd output: ERROR: не указан program или command", chatId, callerAgentName, source, 0);
            return true;
        }

        static const QStringList banned = {
            "rm -rf", "rm -r /", "mkfs", "dd if=", "format ", "diskpart",
            "del /s", "del /q", "rd /s", "rmdir /s", "shutdown", "reboot",
            "reg delete", ":(){"
        };
        const QString lowered = (program + " " + cmdArgs.join(" ")).toLower();
        for (const QString &b : banned) {
            if (lowered.contains(b)) {
                qWarning() << "[Agents] reqCMD заблокирована:" << lowered;
                reqAgent("cmd output: ERROR: команда заблокирована политикой безопасности",
                         chatId, callerAgentName, source, 0);
                return true;
            }
        }

        emit requestThinkingContext("reqCMD", "[Agents] CMD: " + program + " " + cmdArgs.join(" "));

        runCmd(this, program, cmdArgs, 15000,
               [this, chatId, callerAgentName, source](QString result) {
                   reqAgent("cmd output: " + result, chatId, callerAgentName, source, 0);
               });
        return true;
    }

    else if (functionName == "SendMsgToTgUser") {
        QJsonObject textObject;
        textObject["@type"] = "formattedText";
        textObject["text"] = args.value("text", args.value("text").toString()).toString().trimmed();
        QJsonObject inputMessageContent;
        inputMessageContent["@type"] = "inputMessageText";
        inputMessageContent["text"] = textObject;

        QJsonObject request;
        request["@type"] = "sendMessage";
        QString chatIdStr = args.value("fileToGet", args.value("chat_id")).toString().trimmed();
        request["chat_id"] = chatIdStr.toLongLong();
        request["input_message_content"] = inputMessageContent;
        if (tgAccCl) {
            tgAccCl->send(request);
        }
        return true;
    }
        }

        continueChain(prompt, chatID, targetAgent, source, imageData);
        return true;
    }

    else if (functionName == "reqSite") {
        const QString host = args.value("host").toString().trimmed();
        QString path = args.value("path").toString().trimmed();
        QString method = args.value("method").toString().trimmed().toUpper();
        if (method.isEmpty()) method = "POST";
        if (path.isEmpty() || !path.startsWith('/')) path.prepend('/');

        if (isForbiddenHost(host))
            return fail("site", "доступ к этому хосту запрещён политикой безопасности");
        if (method != "GET" && method != "POST")   // HARDCODE: разрешённые методы
            return fail("site", "разрешены только GET и POST");

        // value<QJsonObject>() на QVariantMap возвращал пустой объект и терял тело
        const QJsonObject body = QJsonObject::fromVariantMap(args.value("body").toMap());
        const QJsonObject headersObj = QJsonObject::fromVariantMap(args.value("headers").toMap());
        QMap<QString, QString> headers;
        for (auto it = headersObj.constBegin(); it != headersObj.constEnd(); ++it)
            headers.insert(it.key(), it.value().toString());

    else if (functionName == "reqSite") {
        QString host = args.value("host", args.value("host", callerAgentName).toString()).toString().trimmed();
        QString path = args.value("path", args.value("path", callerAgentName).toString()).toString().trimmed();
        QString method = args.value("method", args.value("method", callerAgentName).toString()).toString().trimmed();
        qint64 chatID = args.value("chatId", args.value("chadId", chatId)).toLongLong();
        if (method.isEmpty())
            method = "POST";
        QJsonObject body;
        if (args.contains("body"))
            body = QJsonDocument::fromJson(args["body"].toString().toUtf8()).object();

        QMap<QString, QString> headers;
        if (args.contains("headers") && args["headers"].isMap()) {
            const QJsonObject headersObj = QJsonObject::fromVariantMap(args["headers"].toMap());
            for (auto it = headersObj.constBegin(); it != headersObj.constEnd(); ++it)
                headers.insert(it.key(), it.value().toString());
        }

        Requests::apiCall(this, host, path, method, body, headers,
                          [this, chatId, callerAgentName, source](const QJsonObject &response) {
                              QString content;
                              if (response.contains("is_html")) {
                                  content = response.value("raw_content").toString();
                                  qDebug() << "Успешно скрапнули HTML страницы!";
                              } else {
                                  content = QJsonDocument(response).toJson(QJsonDocument::Compact);
                              }
                              toolResult("site info", content, chatId, callerAgentName, source);
                          });
        return true;
    }
    else if (functionName == "respond") {
        return true;
    }

    return false;
}
}
void agents::runPluginFunction(const QString &fn, const QVariantMap &args, qint64 chatId,
                               const QString &caller, MessageSource source)
{
    const PluginInfo info = skillManager->pluginForFunction(fn);

    auto report = [this, fn, chatId, caller, source](const QString &text) {
        reqAgent("plugin " + fn + " result: " + text.left(2000), chatId, caller, source, 0);
    };

    if (info.exec.isEmpty()) {
        report("Ошибка: в manifest плагина '" + info.name + "' не указан exec");
        return;
    }
    QString program;
    if (info.exec.contains('/') || info.exec.contains('\\')) {
        const QString p = QFileInfo(info.dir + "/" + info.exec).canonicalFilePath();
        if (p.isEmpty() || !p.startsWith(QFileInfo(info.dir).canonicalFilePath())) {
            report("Ошибка: исполняемый файл плагина не найден или лежит вне папки плагина");
            return;
        }
        program = p;
    } else if (QFileInfo::exists(info.dir + "/" + info.exec)) {
        program = QFileInfo(info.dir + "/" + info.exec).absoluteFilePath();
    } else {
        program = info.exec;
    }

    QSettings cfg(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    QJsonObject settingsObj;
    cfg.beginGroup("plugin_" + info.name);
    for (const QString &k : cfg.childKeys())
        settingsObj[k] = cfg.value(k).toString();
    cfg.endGroup();

    const QString dataDir = QCoreApplication::applicationDirPath() + "/PluginData/" + info.name;
    QDir().mkpath(dataDir);

    QJsonObject context;
    context["chatId"] = QString::number(chatId);
    context["dataDir"] = dataDir;
    context["settings"] = settingsObj;

    QJsonObject payload;
    payload["function"] = fn;
    payload["args"] = QJsonObject::fromVariantMap(args);
    payload["context"] = context;
    const QByteArray input = QJsonDocument(payload).toJson(QJsonDocument::Compact) + "\n";

    auto *proc = new QProcess(this);
    auto *timer = new QTimer(proc);
    timer->setSingleShot(true);
    auto done = std::make_shared<bool>(false);

    auto finish = [proc, timer, done, report](const QString &text) {
        if (*done) return;
        *done = true;
        timer->stop();
        report(text);
        proc->deleteLater();
    };

    connect(proc, &QProcess::started, proc, [proc, input]() {
        proc->write(input);
        proc->closeWriteChannel();
    });
    connect(proc, &QProcess::errorOccurred, proc, [proc, finish](QProcess::ProcessError) {
        finish("Ошибка запуска плагина: " + proc->errorString());
    });
    connect(proc, &QProcess::finished, proc, [proc, finish](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        const QJsonObject res = QJsonDocument::fromJson(out.toUtf8()).object();
        qDebug() << "[Plugin Debug] Процесс завершился с кодом:" << code;
        qDebug() << "[Plugin Debug] Standard Output (out):" << out;
        qDebug() << "[Plugin Debug] Standard Error (err):" << err;
        if (res.isEmpty())
            finish("Ошибка: плагин вернул не JSON (код " + QString::number(code) + "). " + err);
        else
            finish(res["ok"].toBool() ? res["message"].toString()
                                      : "Ошибка: " + res["message"].toString());
    });
    connect(timer, &QTimer::timeout, proc, [proc, finish, info]() {
        proc->kill();
        finish("Ошибка: плагин не ответил за " + QString::number(info.timeoutMs) + " мс");
    });

    proc->setWorkingDirectory(info.dir);
    timer->start(info.timeoutMs);
    proc->start(program, info.args);
}
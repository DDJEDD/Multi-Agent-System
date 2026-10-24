#include "agents.h"
#include "filemanager.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QSettings>
#include <QTimer>
#include "requests.h"
#include <QProcess>
#include <QProcess>
#include <memory>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
agents::agents(QObject *parent) : QObject(parent) {
    number = new phonenumber(this);
    skillManager = new SkillManager(this);
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    QString groqKey = settings.value("groq_api_key").toString();
    setGroqKey(groqKey);

    if (groqKey.isEmpty()) {
        qWarning() << "ВНИМАНИЕ: GROQ_API_KEY не найден ни в config.ini, ни в переменных окружения!";
    }
    tgAccCl = new TelegramAccountClient(nullptr, this);
}
void agents::setGroqKey(const QString &key){
    groqKey=key;
}
bool agents::RetryReq(const QJsonObject &response, int retryCount, const QString &userText,
                      qint64 chatId, const QString &agentName, MessageSource source,
                      const QByteArray &imageData)
{
    if (!response.contains("error"))
        return false;

    const QJsonValue errVal = response["error"];
    QString type;
    QString code;
    int httpCode = 0;

    if (errVal.isObject()) {
        const QJsonObject err = errVal.toObject();
        type = err["type"].toString();


        if (err["code"].isString())
            code = err["code"].toString();
        else
            httpCode = err["code"].toInt();
    } else if (errVal.isString()) {
        code = errVal.toString();
    }

    const bool isRetryable = (code == "rate_limit_exceeded"
                              || type == "server_error"
                              || type == "overloaded_error"
                              || httpCode == 429
                              || httpCode == 500
                              || httpCode == 503);

    if (!isRetryable)
        return false;

    if (retryCount < kMaxRetries) {
        const int delayMs = kRetryBaseDelayMs * (1 << retryCount);
        qWarning() << "[Agents] Ошибка" << (code.isEmpty() ? type : code)
                   << "от Groq, повтор запроса через" << delayMs << "мс. Попытка №"
                   << (retryCount + 1) << "из" << kMaxRetries;

        QTimer::singleShot(delayMs, this, [this, userText, chatId, agentName, source, imageData, retryCount]() {
            reqAgent(userText, chatId, agentName, source, retryCount + 1, imageData);
        });
        return true;
    }

    qWarning() << "[Agents] Исчерпаны попытки повтора после ошибки" << (code.isEmpty() ? type : code);
    emit requestError("GROQ_UNAVAILABLE", "Сервис Groq временно недоступен, попробуйте позже.");
    return true;
}static void runCmd(QObject *context, const QString &program, const QStringList &args, int timeoutMs,
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
}
void agents::reqAgent(const QString &userText, qint64 chatId, const QString &agentName, MessageSource source, int retryCount, const QByteArray &imageData) {


    if (groqKey.isEmpty()) {
        qWarning() << "GROQ_API_KEY не установлен!";
        emit requestError("GROQ_KEY_NOT_FOUND", "Groq key не установлен!");
        return;
    }


    QString aiHost = "api.groq.com";
    QString aiPath = "/openai/v1/chat/completions";

    QStringList agentNames = agents::listAgents();
    QString agentsListStr = "Доступные субагенты в системе: " + agentNames.join(", ");

    QStringList plugins = skillManager->listPlugins();
    QString pluginsListStr = "Доступные плагины: " + plugins.join(", ");
    qDebug() << "доступные плагины:" << plugins.join(", ");

    QString systemPrompt = agents::getFullPrompt(agentName) + "\nHISTORY:" + "\n\n[СИСТЕМНАЯ СПРАВКА]\n"
                           + agentsListStr + "\n" + pluginsListStr + FileManager::GetOldMessages(chatId);
    qDebug() << systemPrompt;
    textwithoutnum finaluserText = number->HideNumbers(userText);
    QString userContextText = finaluserText.usertext;

    QJsonObject body;

    body["model"] = "qwen/qwen3.8-27b";

    QJsonArray messagesArray;

    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = systemPrompt;
    messagesArray.append(systemMessage);

    QJsonObject userMessage;
    userMessage["role"] = "user";

    if (imageData.isEmpty()) {

        userMessage["content"] = userContextText;
    } else {
        QJsonArray contentArray;

        QJsonObject textObj;
        textObj["type"] = "text";
        textObj["text"] = userContextText;
        contentArray.append(textObj);


        QJsonObject imageObj;
        imageObj["type"] = "image_url";
        QJsonObject imageUrlDetails;
        imageUrlDetails["url"] = QString("data:image/jpeg;base64,%1").arg(QString(imageData.toBase64()));
        imageObj["image_url"] = imageUrlDetails;
        contentArray.append(imageObj);

        userMessage["content"] = contentArray;
    }
    messagesArray.append(userMessage);
    body["messages"] = messagesArray;

    QJsonObject responseFormat;
    responseFormat["type"] = "json_object";
    body["response_format"] = responseFormat;
    body["max_tokens"] = 4096;

    QMap<QString, QString> headers;
    headers["Authorization"] = "Bearer " + groqKey;
    headers["Content-Type"] = "application/json";

    QMap<QString, QString> nums = finaluserText.numbers;


    Requests::apiCall(this, aiHost, aiPath, "POST", body, headers,
                      [this, chatId, userText, agentName, source, imageData, nums, retryCount](const QJsonObject &response) {
                          bool res = RetryReq(response, retryCount, userText, chatId, agentName, source, imageData);
                          if(!res){
                              checkreq(response, chatId, userText, nums, source);
                          }
                      }
                      );
}void agents::checkreq(const QJsonObject &response, qint64 chatId, const QString &text,
                      const QMap<QString, QString> &nums, MessageSource source) {

    const QString aiText = response["choices"].toArray()
    .at(0).toObject()
        ["message"].toObject()
                ["content"].toString();

    if (aiText.isEmpty()) {
        qWarning() << "Пустой ответ от Groq:" << QJsonDocument(response).toJson(QJsonDocument::Compact);
        emit requestError("GROQ_EMPTY_RESPONSE", "Groq вернул пустой ответ.");
        return;
    }

    qDebug().noquote() << "Ответ от Groq (текст):" << aiText;
    emit requestGeminiLog(aiText);

    auto optCalls = JSONParser::parse(response, nums, *number);   // <-- response, без адаптера
    if (!optCalls.has_value() || optCalls->isEmpty()) {
        qWarning() << "Не удалось распарсить ответ от Groq или стек вызовов пуст.";
        emit requestError("GROQ_JSON_ERROR", "Не удалось распарсить ответ от Groq или стек вызовов пуст.");
        return;
    }

    const QList<AgentCall> &calls = *optCalls;
    for (const AgentCall &call : calls) {
        executeCall(call.id, call.agentName, call.args, call.functionName,
                    chatId, text, call.role, source);
    }
}


QString agents::getAgentPath(const QString &agentName) const {
    QString baseDir = QCoreApplication::applicationDirPath();
    return baseDir + "/Agents/" + agentName;
}

void agents::createAgent(const QString &agentName, const QString &purpose) {
    QString dirpath = getAgentPath(agentName);

    QDir agentDir = FileManager::createDirectory(dirpath);

    QStringList fileNames = {
        "system_prompt",
        "soul",
        "min_info"
    };

    for (const QString &fileName : fileNames) {
        QString content;
        content += "# Файл: " + fileName + ".md\n";
        if(fileName == "min_info"){
            content += "Назначение агента: " + purpose + "\n";
        }
        FileManager::createAndEditFile(content, agentDir, fileName);
    }

}
bool agents::deleteAgent(const QString &agentName) {
    QString dirpath = getAgentPath(agentName);

    QDir agentDir(dirpath);

    bool success = FileManager::deleteRepo(agentName,agentDir);
    return success;
}

QStringList agents::listAgents() const
{
    QDir agentsRoot(QCoreApplication::applicationDirPath() + "/Agents");
    if (!agentsRoot.exists())
        return {};
    return agentsRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
}
void agents::changeAgentName(const QString &oldName, const QString &newName){
    QString oldpath = getAgentPath(oldName);
    QString dirpath = getAgentPath(newName);
    QDir dir;
    if(!dir.exists(oldpath)){
        qDebug() << "папка не существует";
        return;

    }
    if(dir.exists(dirpath)){
        qDebug() << "папка с указаным именем существует";
        return;
    }
    FileManager::renameDirectory(oldpath, dirpath);


}
QString agents::getFullPrompt(const QString &agentName) const
{
    static const QStringList files = {"soul", "min_info", "system_prompt", "stickers"};
    QStringList parts;
    for (const QString &f : files) {
        const QString content = getFile(agentName, f).trimmed();
        if (!content.isEmpty())
            parts << content;
    }
    return parts.join("\n\n");
}
void agents::editFile(const QString &agentName, const QString &content, const QString &fileToEdit){
    QString dirpath = getAgentPath(agentName);
    QDir agentDir(dirpath);

    FileManager::createAndEditFile(content, agentDir, fileToEdit);
}
QString agents::getFile(const QString &agentName,  const QString &fileToGet) const{
    QString dirpath = getAgentPath(agentName);
    QDir agentDir(dirpath);

    return FileManager::getFile(fileToGet, agentDir);
}

static AggregatedMessages MessageAggregator(const QVariantList &msgList){
    AggregatedMessages result;

    for (const QVariant &item : msgList) {
        QVariantMap msgMap = item.toMap();

        QString text = msgMap.value("text").toString().trimmed();
        int delay = msgMap.value("delay", 0).toInt();

        QString stickerId;
        if (msgMap.contains("stickerId")) {
            stickerId = msgMap.value("stickerId").toString().trimmed();
        } else if (msgMap.contains("sticker")) {
            stickerId = msgMap.value("sticker").toString().trimmed();
        }

        QString photoUrl;
        if (msgMap.contains("photoUrl")) {
            photoUrl = msgMap.value("photoUrl").toString().trimmed();
        }

        if (text.isEmpty() && stickerId.isEmpty()) {
            continue;
        }
        QString code;
        if(msgMap.contains("code") && msgMap.value("code").toString().trimmed() != "NULL"){
            code = msgMap.value("code").toString().trimmed();
        }


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
    if (args.contains("messages")) {
        const AggregatedMessages agg = MessageAggregator(args.value("messages").toList());

        if (!agg.messages.isEmpty() && chatId != 0) {
            if (source == MessageSource::Telegram) {
                emit requestSendMessagesDelayed(chatId, agg.messages);
            } else if (source == MessageSource::UI) {
                emit requestSendMessagesToUIDelayed(chatId, agg.messages);
            }
        }

        if (!agg.fullAiResponse.isEmpty() && chatId != 0) {
            QString agentLabel = QString("[Ответ от агента: %1]").arg(callerAgentName);
            FileManager::SaveMessage(chatId, userText, agg.fullAiResponse);
        }
    }
}
void agents::executeCall(const QString &id, const QString &callerAgentName,
                         const QVariantMap &args, const QString &functionName,
                         qint64 chatId, const QString &userText,
                         const QString &role, MessageSource source)
{
    emit requestThinkingContext("executeCall", "[Agents] Вызов функции: " + functionName + " от агента: " + callerAgentName);

    if (handleCoreFunction(functionName, callerAgentName, args, role, chatId, userText, source)) {
        handleMessagesPayload(args, chatId, callerAgentName, userText, source);
        return;
    }
    if (skillManager->hasFunction(functionName)) {
        emit requestThinkingContext("plugin", "[Agents] Плагин: " + skillManager->getPluginForFunction(functionName)
                                                  + " | функция: " + functionName);
        runPluginFunction(functionName, args, chatId, callerAgentName, source);
        handleMessagesPayload(args, chatId, callerAgentName, userText, source);
        return;
    }
    qWarning() << "[Agents] Предупреждение: Неизвестная функция" << functionName;
    handleMessagesPayload(args, chatId, callerAgentName, userText, source);
}
bool agents::handleCoreFunction(const QString &functionName, const QString &callerAgentName,
                                const QVariantMap &args, const QString &role,
                                qint64 chatId, const QString &userText, MessageSource source)
{
    if (functionName == "createAgent" && role == Constants::RoleOrchestrator) {
        QString targetAgent = args.value("agentName", args.value("agent_name").toString()).toString().trimmed();
        QString purpose = args.value("purpose").toString().trimmed();

        if (targetAgent.isEmpty()) {
            qWarning() << "[Agents] Ошибка: Имя нового агента не указано в args!";
            return true;
        }

        emit requestThinkingContext("agentCreate", "[Agents] Создание агента: " + targetAgent + " | Цель: " + purpose);
        createAgent(targetAgent, purpose);
        return true;
    }

    else if (functionName == "deleteAgent" && role == Constants::RoleOrchestrator) {
        QString targetAgent = args.value("agentName", args.value("agent_name").toString()).toString().trimmed();
        if (targetAgent.isEmpty()) targetAgent = callerAgentName;

        emit requestThinkingContext("deleteAgent", "[Agents] Удаление агента: " + targetAgent);
        deleteAgent(targetAgent);
        return true;
    }

    else if (functionName == "changeAgentName" && role == Constants::RoleOrchestrator) {
        QString oldName = args.value("oldName", args.value("old_name", callerAgentName).toString()).toString().trimmed();
        QString newName = args.value("newName", args.value("new_name").toString()).toString().trimmed();

        if (newName.isEmpty()) {
            qWarning() << "[Agents] Ошибка: Новое имя агента не указано!";
            return true;
        }

        emit requestThinkingContext("changeAgentName", "[Agents] Переименование агента с " + oldName + " на " + newName);
        changeAgentName(oldName, newName);
        return true;
    }

    else if (functionName == "editFile" && role == Constants::RoleOrchestrator) {
        QString targetAgent = args.value("agentName", args.value("agent_name", callerAgentName).toString()).toString().trimmed();
        QString fileToEdit = args.value("fileToEdit", args.value("file_to_edit").toString()).toString().trimmed();
        QString content = args.value("content").toString();

        if (targetAgent.isEmpty() || fileToEdit.isEmpty()) {
            qWarning() << "[Agents] Ошибка: Не указан целевой агент или имя файла в editFile!";
            return true;
        }

        emit requestThinkingContext("editFile", "[Agents] Изменение файла " + fileToEdit + " у агента: " + targetAgent);
        editFile(targetAgent, content, fileToEdit);
        return true;
    }

    else if (functionName == "getFile" && role == Constants::RoleOrchestrator) {
        QString targetAgent = args.value("agentName", args.value("agent_name", callerAgentName).toString()).toString().trimmed();
        QString fileToGet = args.value("fileToGet", args.value("file_to_get").toString()).toString().trimmed();

        getFile(targetAgent, fileToGet);
        return true;
    }

    else if (functionName == "getPluginSkill") {
        QString targetPlugin = args.value("pluginName", args.value("plugin_name").toString()).toString().trimmed();

        if (targetPlugin.isEmpty()) {
            qWarning() << "[Agents] Ошибка: Имя плагина не указано в getPluginSkill!";
            return true;
        }

        emit requestThinkingContext("getPluginSkill", "[Agents] Запрос инструкции для плагина: " + targetPlugin);

        QString skillContent = skillManager->getSkilsFile(targetPlugin);

        QString resultMsg;
        if (skillContent.isEmpty()) {
            resultMsg = QString("Ошибка: Плагин '%1' не найден или у него нет файла SKILL.").arg(targetPlugin);
        } else {
            resultMsg = QString("Успешно получена инструкция для плагина '%1':\n%2").arg(targetPlugin, skillContent);
        }
        reqAgent(resultMsg, chatId, callerAgentName, source, 0);
        return true;
    }
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

    else if (functionName == "reqAgent") {
        QString targetAgent = args.value("agentName", args.value("agent_name", callerAgentName).toString()).toString().trimmed();
        if (role != Constants::RoleOrchestrator && targetAgent != "Главный агент") {
            qWarning() << "[Agents] Ошибка безопасности: Субагент" << callerAgentName
                       << "пытается вызвать другого сабагента (" << targetAgent
                       << "). Разрешено вызывать только Главного агента!";
            return true;
        }
        if (targetAgent.isEmpty() || targetAgent == callerAgentName) {
            qWarning() << "[Agents] Ошибка: Агент" << callerAgentName << "пытается вызвать сам себя или не указал цель!";
            return true;
        }
        QString prompt = args.value("prompt", args.value("prompt", callerAgentName).toString()).toString().trimmed();
        QByteArray imageData = QByteArray::fromBase64(args.value("photoB64", args.value("photo_b64", "")).toString().toLatin1());
        qint64 chatID = args.value("chatId", args.value("chadId", chatId)).toLongLong();

        reqAgent(prompt, chatID, targetAgent, source, 0, imageData);
        return true;
    }

    else if (functionName == "reqSite") {
        QString host = args.value("host", args.value("host", callerAgentName).toString()).toString().trimmed();
        QString path = args.value("path", args.value("path", callerAgentName).toString()).toString().trimmed();
        QString method = args.value("method", args.value("method", callerAgentName).toString()).toString().trimmed();
        qint64 chatID = args.value("chatId", args.value("chadId", chatId)).toLongLong();
        if (method.isEmpty()) {
            method = "POST";
        }
        QJsonObject body = args.value("body").value<QJsonObject>();
        QMap<QString, QString> headers;
        QJsonObject headersObj = args.value("headers").toJsonObject();

        for (auto it = headersObj.constBegin(); it != headersObj.constEnd(); ++it) {
            headers.insert(it.key(), it.value().toString());
        }

        Requests::apiCall(this, host, path, method, body, headers, [this, chatID, callerAgentName, source](const QJsonObject &response) {
            QString content;
            if (response.contains("is_html")) {
                content = response.value("raw_content").toString();
                qDebug() << "Успешно скрапнули HTML страницы!";
            } else {
                content = QJsonDocument(response).toJson(QJsonDocument::Compact);
            }
            reqAgent("site info: " + content, chatID, callerAgentName, source, 0);
        });
        return true;
    }
    else if (functionName == "respond") {
        return true;
    }

    return false;
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
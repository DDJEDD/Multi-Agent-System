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
agents::agents(QObject *parent) : QObject(parent) {
    number = new phonenumber(this);
    QSettings settings(QString(APP_SRC_DIR) + "/config.ini", QSettings::IniFormat);
    QString geminiKey = settings.value("gemini_api_key").toString();
    setGeminiKey(geminiKey);

    if (geminiKey.isEmpty()) {
        qWarning() << "ВНИМАНИЕ: GEMINI_API_KEY не найден ни в config.ini, ни в переменных окружения!";
    }
}
void agents::setGeminiKey(const QString &key){
    geminiKey=key;
}
void agents::reqAgent(const QString &userText, qint64 chatId,const QString &agentName,MessageSource source,int retryCount,const QByteArray &imageData) {


    if (geminiKey.isEmpty()) {
        qWarning() << "GEMINI_API_KEY не установлен!";
        emit requestError("GEMINI_KEY_NOT_FOUND", "Gemini key не установлен!");
        return;
    }
    QString aiHost = "generativelanguage.googleapis.com";
    QString aiPath = QString("/v1beta/models/gemini-3.6-flash:generateContent?key=%1").arg(geminiKey);

    QStringList agentNames = agents::listAgents();
    QString agentsListStr = "Доступные субагенты в системе: " + agentNames.join(", ");

    QString final = agents::getFullPrompt(agentName) + "\nHISTORY:"  + "\n\n[СИСТЕМНАЯ СПРАВКА]\n" + agentsListStr + FileManager::GetOldMessages(chatId);
    textwithoutnum finaluserText = number->HideNumbers(userText);
    QString fullContextText = final + "\n" + finaluserText.usertext;

    qDebug() << fullContextText;
    QJsonObject body;
    QJsonArray partsArray;
    QJsonObject textPart;
    textPart["text"] = fullContextText;
    partsArray.append(textPart);
    if (!imageData.isEmpty()) {
        QJsonObject imagePart;
        QJsonObject inlineData;

        inlineData["mime_type"] = "image/jpeg";
        inlineData["data"] = QString(imageData.toBase64());
        imagePart["inline_data"] = inlineData;
        partsArray.append(imagePart);
    }

    QJsonObject contentsObj;
    contentsObj["parts"] = partsArray;

    QJsonArray contentsArray;
    contentsArray.append(contentsObj);
    body["contents"] = contentsArray;

    QJsonObject generationConfig;
    generationConfig["responseMimeType"] = "application/json";
    generationConfig["maxOutputTokens"] = 4096;
    body["generationConfig"] = generationConfig;


    QMap<QString, QString> nums = finaluserText.numbers;
    requests->apiCall(this, aiHost, aiPath, body, {},
                      [this, chatId, userText, agentName, source, imageData, nums, retryCount](const QJsonObject &response) {

                          if (response.contains("error")) {
                              int errorCode = response["error"].toObject()["code"].toInt();
                              bool isRetryable = (errorCode == 503 || errorCode == 429 || errorCode == 500);

                              if (isRetryable && retryCount < kMaxRetries) {
                                  int delayMs = kRetryBaseDelayMs * (1 << retryCount);
                                  qWarning() << "[Agents] Ошибка" << errorCode
                                             << "от Gemini, повтор запроса через" << delayMs << "мс. Попытка №"
                                             << (retryCount + 1) << "из" << kMaxRetries;

                                  QTimer::singleShot(delayMs, this, [this, userText, chatId, agentName, source, imageData, retryCount]() {
                                      reqAgent(userText, chatId, agentName, source,retryCount + 1,imageData );
                                  });
                                  return;
                              }

                              if (isRetryable) {
                                  qWarning() << "[Agents] Исчерпаны попытки повтора после ошибки" << errorCode;
                                  emit requestError("GEMINI_UNAVAILABLE", "Сервис Gemini временно недоступен, попробуйте позже.");
                                  return;
                              }
                          }

                          checkreq(response, chatId, userText, nums, source);
                      });
}

void agents::checkreq(const QJsonObject &response, qint64 chatId, const QString &text, const QMap<QString, QString> &nums, MessageSource source) {

    QJsonArray candidates = response["candidates"].toArray();
    if (!candidates.isEmpty()) {
        QJsonArray parts = candidates[0].toObject()["content"].toObject()["parts"].toArray();
        if (!parts.isEmpty()) {
            QString aiText = parts[0].toObject()["text"].toString();
            qDebug().noquote() << "Ответ от Gemini (текст):" << aiText;
            emit requestGeminiLog(aiText);
        }
    }

    auto optCalls = JSONParser::parse(response, nums, *number);
    if (!optCalls.has_value() || optCalls->isEmpty()) {
        qWarning() << "Не удалось распарсить ответ от Gemini или стек вызовов пуст.";
        emit requestError("GEMINI_JSON_ERROR", "Не удалось распарсить ответ от Gemini или стек вызовов пуст.");
        return;
    }

    const QList<AgentCall> &calls = *optCalls;
    qDebug() << "[TgBot] Получено вызовов сабагентов:" << calls.size();

    for (const AgentCall &call : calls) {
        qDebug() << "  -> Запуск функции:" << call.functionName
                 << "для агента:" << call.agentName
                 << "ID:" << call.id;


        executeCall(call.id, call.agentName, call.args, call.functionName, chatId, text, call.role, source);
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


void agents::executeCall(const QString &id, const QString &callerAgentName, const QVariantMap &args, const QString &functionName, qint64 chatId, const QString &userText,const QString &role, MessageSource source)
{
    emit requestThinkingContext("executeCall", "[Agents] Вызов функции:" + functionName + "От агента:" + callerAgentName + "ChatID:"  + QString::number(chatId));
    if (functionName == "createAgent" && role == Constants::RoleOrchestrator) {
        QString targetAgent = args.value("agentName", args.value("agent_name").toString()).toString().trimmed();
        QString purpose = args.value("purpose").toString().trimmed();

        if (targetAgent.isEmpty()) {
            qWarning() << "[Agents] Ошибка: Имя нового агента не указано в args!";
            return;
        }

        emit requestThinkingContext("agentCreate", "[Agents] Создание агента:" + targetAgent + "Цель:" + purpose);
        createAgent(targetAgent, purpose);
    }

    else if (functionName == "deleteAgent" && role ==  Constants::RoleOrchestrator) {
        QString targetAgent = args.value("agentName", args.value("agent_name").toString()).toString().trimmed();
        if (targetAgent.isEmpty()) targetAgent = callerAgentName;

        emit requestThinkingContext("deleteAgent", "[Agents] Удаление агента:" + targetAgent);
        deleteAgent(targetAgent);
    }

    else if (functionName == "changeAgentName" && role == Constants::RoleOrchestrator) {
        QString oldName = args.value("oldName", args.value("old_name", callerAgentName).toString()).toString().trimmed();
        QString newName = args.value("newName", args.value("new_name").toString()).toString().trimmed();

        if (newName.isEmpty()) {
            qWarning() << "[Agents] Ошибка: Новое имя агента не указано!";
            return;
        }

        emit requestThinkingContext("changeAgentName","[Agents] Переименование агента с" + oldName + "на" + newName);
        changeAgentName(oldName, newName);
    }

    else if (functionName == "editFile" && role == Constants::RoleOrchestrator) {
        QString targetAgent = args.value("agentName", args.value("agent_name", callerAgentName).toString()).toString().trimmed();
        QString fileToEdit = args.value("fileToEdit", args.value("file_to_edit").toString()).toString().trimmed();
        QString content = args.value("content").toString();

        if (targetAgent.isEmpty() || fileToEdit.isEmpty()) {
            qWarning() << "[Agents] Ошибка: Не указан целевой агент или имя файла в editFile!";
            return;
        }

        emit requestThinkingContext("editFile","[Agents] Изменение файла" + fileToEdit + "у агента:" + targetAgent);
        editFile(targetAgent, content, fileToEdit);
    }

    else if (functionName == "getFile" && role == Constants::RoleOrchestrator) {
        QString targetAgent = args.value("agentName", args.value("agent_name", callerAgentName).toString()).toString().trimmed();
        QString fileToGet = args.value("fileToGet", args.value("file_to_get").toString()).toString().trimmed();

        getFile(targetAgent, fileToGet);
    }

    else if(functionName == "reqAgent"){
        QString targetAgent = args.value("agentName", args.value("agent_name", callerAgentName).toString()).toString().trimmed();
        if (role != "orchestrator" && targetAgent != "Главный агент") {
            qWarning() << "[Agents] Ошибка безопасности: Субагент" << callerAgentName
                       << "пытается вызвать другого сабагента (" << targetAgent
                       << "). Разрешено вызывать только Главного агента!";
            return;
        }
        if (targetAgent.isEmpty() || targetAgent == callerAgentName) {
            qWarning() << "[Agents] Ошибка: Агент" << callerAgentName << "пытается вызвать сам себя или не указал цель!";
            return;
        }
        QString prompt = args.value("prompt", args.value("prompt", callerAgentName).toString()).toString().trimmed();
        QByteArray imageData = QByteArray::fromBase64(args.value("photoB64", args.value("photo_b64", "")).toString().toLatin1());
        qint64 chatID = args.value("chatId", args.value("chadId", chatId)).toLongLong();

        reqAgent(prompt, chatID, targetAgent,source,0,imageData );
    }

    if (args.contains("messages")) {
        const AggregatedMessages agg = MessageAggregator(args.value("messages").toList());

        if (!agg.messages.isEmpty() && chatId != 0) {
            if(source == MessageSource::Telegram){
                emit requestSendMessagesDelayed(chatId, agg.messages);

            }else if(source == MessageSource::UI){
                emit requestSendMessagesToUIDelayed(chatId, agg.messages);
            }
        }

        if (!agg.fullAiResponse.isEmpty() && chatId != 0) {
            QString agentLabel = QString("[Ответ от агента: %1]").arg(callerAgentName);
            FileManager::SaveMessage(chatId, userText, agg.fullAiResponse);
        }
    }
}

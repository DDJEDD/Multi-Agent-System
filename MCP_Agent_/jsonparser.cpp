#include "jsonparser.h"
#include <QCoreApplication>
#include <QJsonArray>
static bool looksLikeCallStack(const QJsonDocument &doc)
{
    if (!doc.isObject())
        return false;
    const QJsonObject o = doc.object();
    return o.contains("agent_controls_context") || o.contains("function_call_stack");
}
QString JSONParser::sanitizeJsonText(const QString &text)
{
    QString raw = text.trimmed();

    if (raw.startsWith("```")) {
        raw.remove(QRegularExpression("^```(?:json)?\\s*"));
        raw.remove(QRegularExpression("\\s*```$"));
        raw = raw.trimmed();
    }

    int lastBrace = raw.lastIndexOf('}');
    if (lastBrace != -1) {
        raw = raw.left(lastBrace + 1);
    }

    raw.remove(QRegularExpression(R"(,\s*"thoughtSignature"\s*:\s*"[^"]*")"));
    raw.remove(QRegularExpression(R"("thoughtSignature"\s*:\s*"[^"]*",?)"));

    if (raw.isEmpty()) {
        qWarning() << "После очистки текст оказался пустым!";
        return "";
    }
    return raw;
}

std::optional<QList<AgentCall>> JSONParser::parse(const QJsonObject &response,
                                                  const MaskTable &table,
                                                  const phonenumber &phone,
                                                  QString *errorOut)
{
    auto fail = [&](const QString &msg) -> std::optional<QList<AgentCall>> {
        qWarning() << msg;
        if (errorOut) *errorOut = msg;
        return std::nullopt;
    };
        return std::nullopt;
    };

    if (response.contains("error")) {
        const QJsonObject e = response["error"].toObject();
        QString msg = e["message"].toString();
        const QString raw = e["metadata"].toObject()["raw"].toString();
        if (!raw.isEmpty())
            msg += "\n" + raw;
        return fail(QString("OpenRouter %1: %2").arg(e["code"].toInt()).arg(msg));
    }

    const QJsonArray choices = response["choices"].toArray();
    if (choices.isEmpty())
        return fail("OpenRouter вернул пустой ответ (нет choices)");

    const QJsonObject firstChoice = choices[0].toObject();

    if (firstChoice.contains("error"))
        return fail("OpenRouter: " + firstChoice["error"].toObject()["message"].toString());

    const bool truncated = firstChoice["finish_reason"].toString() == "length";
    if (truncated)
        qWarning() << "Ответ модели обрезан по max_tokens: JSON, скорее всего, неполный";

    if (!firstChoice.contains("message"))
        return fail("В ответе модели нет message");

    const QJsonObject messageObj = firstChoice["message"].toObject();
    const QString rawAiText = sanitizeJsonText(messageObj["content"].toString());
    if (rawAiText.isEmpty())
        return fail(truncated
                        ? "Модель упёрлась в лимит токенов и не вернула ответ. Увеличь max_tokens или выбери модель без длинных рассуждений."
                        : "Модель вернула пустой ответ");

    // --- Дальнейшая логика обработки вашего внутреннего JSON остается прежней ---
    QJsonParseError parseError;
    QJsonDocument aiJsonDoc = QJsonDocument::fromJson(rawAiText.toUtf8(), &parseError);

    const bool needExtract = parseError.error != QJsonParseError::NoError
                             || (aiJsonDoc.isObject() && !looksLikeCallStack(aiJsonDoc));
    if (needExtract) {
        QJsonParseError extractError;
        const QJsonDocument extracted = extractFirstJsonObject(rawAiText, &extractError);
        if (extractError.error == QJsonParseError::NoError) {
            aiJsonDoc = extracted;
            parseError.error = QJsonParseError::NoError;
        }
    }

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "начало ответа:" << rawAiText.left(300);
        return fail("Модель вернула невалидный JSON: " + parseError.errorString()
                    + (truncated ? " (ответ обрезан по max_tokens)" : ""));
    }

    QJsonArray stack;

    if (aiJsonDoc.isArray()) {
        stack = aiJsonDoc.array();
    } else if (aiJsonDoc.isObject()) {
        const QJsonObject aiJsonObj = aiJsonDoc.object();
        if (aiJsonObj.contains("agent_controls_context")) {
            const QJsonObject contextObj = aiJsonObj["agent_controls_context"].toObject();
            stack = contextObj["function_call_stack"].toArray();
        } else if (aiJsonObj.contains("function_call_stack")) {
            stack = aiJsonObj["function_call_stack"].toArray();
        }
    }

    if (stack.isEmpty())
        return fail("В ответе модели нет function_call_stack или у него неверная структура");

    QList<AgentCall> calls;

    for (const QJsonValue &val : stack) {
        const QJsonObject callObj = val.toObject();
        const QJsonObject argsObj = callObj["args"].toObject();

        AgentCall call;
        call.id = callObj["id"].toString();
        if (call.id.isEmpty())
            call.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        call.role = callObj["role"].toString();

        if (callObj.contains("functionName"))
            call.functionName = callObj["functionName"].toString();
        else if (callObj.contains("name"))
            call.functionName = callObj["name"].toString();

        if (callObj.contains("agentName"))
            call.agentName = callObj["agentName"].toString();
        else if (callObj.contains("agent_name"))
            call.agentName = callObj["agent_name"].toString();
            call.agentName = callObj["agentName"].toString();
        else if (callObj.contains("agent_name"))
            call.agentName = callObj["agent_name"].toString();
        else if (argsObj.contains("agent_name"))
            call.agentName = argsObj["agent_name"].toString();
        else if (argsObj.contains("agentName"))
            call.agentName = argsObj["agentName"].toString();

        QVariantMap argsMap = phone.restoreDeep(argsObj.toVariantMap(), table).toMap();

        if (argsMap.contains("messages")) {
            const QVariantList msgArr = argsMap.value("messages").toList();
            QVariantList processedMsgs;

            for (const QVariant &msgVal : msgArr) {
                const QVariantMap msgObj = msgVal.toMap();

                QVariantMap msgMap;
                msgMap["text"]      = msgObj.value("text").toString().trimmed();
                msgMap["delay"]     = qBound(0, msgObj.value("delay").toInt(), 5000);
                msgMap["stickerId"] = msgObj.value("stickerId").toString();
                msgMap["photoUrl"]  = msgObj.value("photoUrl").toString();
                msgMap["code"]      = msgObj.value("code").toString();
                processedMsgs.append(msgMap);
            }
            argsMap["messages"] = processedMsgs;
        }

        call.args = argsMap;
        if (!call.id.isEmpty() && !call.functionName.isEmpty())
            calls.append(call);
    }

    if (calls.isEmpty())
        return fail("В ответе модели нет корректных вызовов функций");
    return calls;
}
QJsonDocument JSONParser::extractFirstJsonObject(const QString &rawText, QJsonParseError *error)
{
    QJsonDocument best;
    const int n = rawText.length();
    int i = 0;

    while (i < n) {
        if (rawText[i] != '{') { ++i; continue; }

        int depth = 0, endPos = -1;
        bool inString = false, escaped = false;
        for (int j = i; j < n; ++j) {
            const QChar c = rawText[j];
            if (escaped) { escaped = false; continue; }
            if (inString) {
                if (c == '\\') escaped = true;
                else if (c == '"') inString = false;
                continue;
            }
            if (c == '"') inString = true;
            else if (c == '{') ++depth;
            else if (c == '}') {
                if (--depth == 0) { endPos = j; break; }
            }
        }

        if (endPos == -1) { ++i; continue; }

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(
            rawText.mid(i, endPos - i + 1).toUtf8(), &err);

        if (err.error == QJsonParseError::NoError && looksLikeCallStack(doc)) {
            best = doc;
            i = endPos + 1;
        } else {
            ++i;
        }
    }

    if (error) {
        error->error = best.isNull() ? QJsonParseError::MissingObject : QJsonParseError::NoError;
        error->offset = 0;
    }
    return best;
}

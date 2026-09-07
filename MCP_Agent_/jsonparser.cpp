#include "jsonparser.h"
#include <QCoreApplication>
#include <QJsonArray>

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
        qWarning() << "После очистки текст от Gemini оказался пустым!";
        return "";
    }
    return raw;
}
std::optional<QList<AgentCall>> JSONParser::parse(const QJsonObject &response,
                                                  const QMap<QString, QString> &nums,
                                                  const phonenumber &phone)
{
    if (response.contains("error")) {
        qWarning() << "Gemini API Error:" << response["error"].toObject()["message"].toString();
        return std::nullopt;
    }

    const QJsonArray candidates = response["candidates"].toArray();
    if (candidates.isEmpty()) return std::nullopt;

    const QJsonArray parts = candidates[0].toObject()["content"].toObject()["parts"].toArray();
    if (parts.isEmpty()) return std::nullopt;

    const QString rawAiText = sanitizeJsonText(parts[0].toObject()["text"].toString());
    if (rawAiText.isEmpty()) return std::nullopt;

    QJsonParseError parseError;
    QJsonDocument aiJsonDoc = QJsonDocument::fromJson(rawAiText.toUtf8(), &parseError);

    if (parseError.error == QJsonParseError::GarbageAtEnd || parseError.error != QJsonParseError::NoError) {
        aiJsonDoc = extractFirstJsonObject(rawAiText, &parseError);
    }

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON Parse Error:" << parseError.errorString();
        return std::nullopt;
    }

    QJsonArray stack;


    if (aiJsonDoc.isArray()) {
        stack = aiJsonDoc.array();
    }

    else if (aiJsonDoc.isObject()) {
        const QJsonObject aiJsonObj = aiJsonDoc.object();
        if (aiJsonObj.contains("agent_controls_context")) {
            QJsonObject contextObj = aiJsonObj["agent_controls_context"].toObject();
            stack = contextObj["function_call_stack"].toArray();
        } else if (aiJsonObj.contains("function_call_stack")) {
            stack = aiJsonObj["function_call_stack"].toArray();
        }
    }

    if (stack.isEmpty()) {
        qWarning() << "Стек вызовов функций пуст или имеет неверную структуру.";
        return std::nullopt;
    }

    QList<AgentCall> calls;

    for (const QJsonValue &val : stack) {
        QJsonObject callObj = val.toObject();

        QJsonObject argsObj = callObj["args"].toObject();

        AgentCall call;
        call.id = callObj["id"].toString();
        if (call.id.isEmpty()) {
            call.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        call.role = callObj["role"].toString();

        if (callObj.contains("functionName")) {
            call.functionName = callObj["functionName"].toString();
        } else if (callObj.contains("name")) {
            call.functionName = callObj["name"].toString();
        }


        if (callObj.contains("agentName")) {
            call.agentName = callObj["agentName"].toString();
        } else if (callObj.contains("agent_name")) {
            call.agentName = callObj["agent_name"].toString();
        } else if (argsObj.contains("agent_name")) {
            call.agentName = argsObj["agent_name"].toString();
        } else if (argsObj.contains("agentName")) {
            call.agentName = argsObj["agentName"].toString();
        }

        QVariantMap argsMap = argsObj.toVariantMap();

        if (argsObj.contains("messages")) {
            QJsonArray msgArr = argsObj["messages"].toArray();
            QVariantList processedMsgs;

            for (const QJsonValue &msgVal : msgArr) {
                QJsonObject msgObj = msgVal.toObject();
                QString msgText = msgObj["text"].toString().trimmed();

                msgText = phone.restoreNumbers(msgText, nums);

                QVariantMap msgMap;
                msgMap["text"] = msgText;
                msgMap["delay"] = qBound(0, msgObj["delay"].toInt(), 5000);
                msgMap["stickerId"] = msgObj["stickerId"].toString();
                msgMap["photoUrl"]  = msgObj["photoUrl"].toString();
                msgMap["code"]      = msgObj["code"].toString();
                processedMsgs.append(msgMap);
            }
            argsMap["messages"] = processedMsgs;
        }

        call.args = argsMap;

        if (!call.id.isEmpty() && !call.functionName.isEmpty()) {
            calls.append(call);
        }
    }

    if (calls.isEmpty()) return std::nullopt;
    return calls;
}
QJsonDocument JSONParser::extractFirstJsonObject(const QString &rawText, QJsonParseError *error) {
    int depth = 0;
    bool inString = false, escaped = false;
    int endPos = -1;

    for (int i = 0; i < rawText.length(); ++i) {
        QChar c = rawText[i];
        if (escaped) { escaped = false; continue; }
        if (c == '\\' && inString) { escaped = true; continue; }
        if (c == '"') { inString = !inString; continue; }
        if (inString) continue;

        if (c == '{') depth++;
        else if (c == '}') {
            if (--depth == 0) { endPos = i; break; }
        }
    }

    if (endPos != -1) {
        return QJsonDocument::fromJson(rawText.left(endPos + 1).trimmed().toUtf8(), error);
    }
    return QJsonDocument();
}

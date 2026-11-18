#include "aiprovider.h"
#include <QJsonArray>



static bool has(const QString &s, const char *needle)
{
    return s.contains(QLatin1String(needle), Qt::CaseInsensitive);
}

static AiError makeError(int http, const QString &type, const QString &msg)
{
    AiError e;
    e.http = http;
    e.message = msg;
    const QString t = type.toLower();

    if (http == 402 || has(t, "billing") || has(msg, "credit balance")
        || has(msg, "more credits") || has(msg, "insufficient"))
        e.kind = AiError::NoCredits;
    else if (http == 401 || http == 403 || has(t, "auth") || has(t, "permission"))
        e.kind = AiError::Auth;
    else if (http == 429 || has(t, "rate") || has(t, "resource_exhausted"))
        e.kind = AiError::RateLimit;
    else if (http == 408 || http >= 500 || has(t, "overloaded") || has(t, "server")
             || has(t, "unavailable") || t == "api_error")
        e.kind = AiError::Server;
    else
        e.kind = AiError::BadRequest;
    return e;
}

static AiResult emptyResponse()
{
    AiResult r;
    r.error.kind = AiError::Network;
    r.error.message = "пустой ответ от провайдера (сеть, таймаут или обрыв соединения)";
    return r;
}

static AiResult emptyText()
{
    AiResult r;
    r.error.kind = AiError::Parse;
    r.error.message = "модель вернула пустой ответ";
    return r;
}

static QList<AiMessage> normalizeTurns(const QList<AiMessage> &in)
{
    QList<AiMessage> out;
    for (const AiMessage &m : in) {
        if (m.text.trimmed().isEmpty() && m.imageJpeg.isEmpty())
            continue;
        if (out.isEmpty() && m.role != "user")
            continue;
        if (!out.isEmpty() && out.last().role == m.role) {
            out.last().text += "\n\n" + m.text;
            if (out.last().imageJpeg.isEmpty())
                out.last().imageJpeg = m.imageJpeg;
        } else {
            out.append(m);
        }
    }
    return out;
}

QJsonObject AiResult::toEnvelope() const
{
    if (ok) {
        QJsonObject msg;
        msg["role"] = "assistant";
        msg["content"] = text;
        QJsonObject choice;
        choice["message"] = msg;
        QJsonObject root;
        root["choices"] = QJsonArray{choice};
        return root;
    }

    // шоб аі агент розумів
    int code = error.http;
    switch (error.kind) {
    case AiError::Auth:       code = 401; break;
    case AiError::NoCredits:  code = 402; break;
    case AiError::RateLimit:  code = 429; break;
    case AiError::Server:     code = 503; break;
    case AiError::Network:    code = 503; break;
    case AiError::BadRequest: code = 400; break;
    case AiError::Parse:      code = 422; break;
    default: break;
    }
    QJsonObject e;
    e["code"] = code;
    e["message"] = error.message;
    QJsonObject root;
    root["error"] = e;
    return root;
}

// ---------------------------------------------------------------- OpenAI-совместимые

class OpenAiProvider : public AiProvider {
public:
    using AiProvider::AiProvider;

    HttpSpec buildRequest(const AiRequest &req, const QString &model) const override
    {
        QJsonArray msgs;
        if (!req.system.isEmpty()) {
            QJsonObject s;
            s["role"] = "system";
            s["content"] = req.system;
            msgs.append(s);
        }
        for (const AiMessage &m : req.messages) {
            QJsonObject o;
            o["role"] = m.role;
            if (m.imageJpeg.isEmpty()) {
                o["content"] = m.text;
            } else {
                QJsonObject t;
                t["type"] = "text";
                t["text"] = m.text;
                QJsonObject url;
                url["url"] = "data:image/jpeg;base64," + QString::fromLatin1(m.imageJpeg.toBase64());
                QJsonObject img;
                img["type"] = "image_url";
                img["image_url"] = url;
                o["content"] = QJsonArray{t, img};
            }
            msgs.append(o);
        }

        HttpSpec h;
        h.host = m_cfg.host;
        h.path = m_cfg.path;
        h.body["model"] = model;
        h.body["messages"] = msgs;
        h.body["max_tokens"] = req.maxTokens;
        h.body["temperature"] = req.temperature;
        if (req.jsonMode) {
            QJsonObject rf;
            rf["type"] = "json_object";
            h.body["response_format"] = rf;
        }
        if (m_cfg.quirks.contains("openrouter", Qt::CaseInsensitive)) {
            QJsonObject reasoning;
            reasoning["enabled"] = false;
            h.body["reasoning"] = reasoning;
            QJsonObject provider;
            provider["require_parameters"] = true;
            h.body["provider"] = provider;
        }

        h.headers["Authorization"] = "Bearer " + m_cfg.apiKey;
        h.headers["Content-Type"] = "application/json";
        for (auto it = m_cfg.extraHeaders.constBegin(); it != m_cfg.extraHeaders.constEnd(); ++it)
            h.headers[it.key()] = it.value();
        return h;
    }

    AiResult parseResponse(const QJsonObject &r) const override
    {
        if (r.isEmpty())
            return emptyResponse();

        AiResult res;
        if (r.contains("error")) {
            const QJsonValue ev = r["error"];
            if (ev.isObject()) {
                const QJsonObject e = ev.toObject();
                QString msg = e["message"].toString();
                const QString raw = e["metadata"].toObject()["raw"].toString();
                if (!raw.isEmpty())
                    msg += "\n" + raw;
                const bool codeIsStr = e["code"].isString();
                res.error = makeError(codeIsStr ? 0 : e["code"].toInt(),
                                      codeIsStr ? e["code"].toString() : e["type"].toString(), msg);
            } else {
                res.error = makeError(0, ev.toString(), ev.toString());
            }
            return res;
        }

        res.text = r["choices"].toArray().at(0).toObject()["message"].toObject()["content"].toString();
        if (res.text.trimmed().isEmpty())
            return emptyText();
        res.ok = true;
        return res;
    }
};

// ---------------------------------------------------------------- Anthropic

class AnthropicProvider : public AiProvider {
public:
    using AiProvider::AiProvider;

    HttpSpec buildRequest(const AiRequest &req, const QString &model) const override
    {
        QJsonArray msgs;
        for (const AiMessage &m : normalizeTurns(req.messages)) {
            QJsonObject o;
            o["role"] = m.role;
            QJsonObject t;
            t["type"] = "text";
            t["text"] = m.text;
            if (m.imageJpeg.isEmpty()) {
                o["content"] = m.text;
            } else {
                QJsonObject src;
                src["type"] = "base64";
                src["media_type"] = "image/jpeg";
                src["data"] = QString::fromLatin1(m.imageJpeg.toBase64());
                QJsonObject img;
                img["type"] = "image";
                img["source"] = src;
                o["content"] = QJsonArray{img, t};
            }
            msgs.append(o);
        }

        QString system = req.system;
        if (req.jsonMode)
            system += "\n\nОтвечай только одним валидным JSON-объектом, без текста и Markdown вне JSON.";

        HttpSpec h;
        h.host = m_cfg.host;
        h.path = m_cfg.path;
        h.body["model"] = model;
        h.body["max_tokens"] = req.maxTokens;
        h.body["temperature"] = req.temperature;
        if (!system.isEmpty())
            h.body["system"] = system;
        h.body["messages"] = msgs;

        h.headers["x-api-key"] = m_cfg.apiKey;
        h.headers["anthropic-version"] = "2023-06-01";
        h.headers["Content-Type"] = "application/json";
        for (auto it = m_cfg.extraHeaders.constBegin(); it != m_cfg.extraHeaders.constEnd(); ++it)
            h.headers[it.key()] = it.value();
        return h;
    }

    AiResult parseResponse(const QJsonObject &r) const override
    {
        if (r.isEmpty())
            return emptyResponse();

        AiResult res;
        if (r["type"].toString() == "error" || r.contains("error")) {
            const QJsonObject e = r["error"].toObject();
            res.error = makeError(0, e["type"].toString(), e["message"].toString());
            return res;
        }

        for (const QJsonValue &b : r["content"].toArray()) {
            const QJsonObject o = b.toObject();
            if (o["type"].toString() == "text")
                res.text += o["text"].toString();
        }
        if (res.text.trimmed().isEmpty())
            return emptyText();
        res.ok = true;
        return res;
    }
};


class GeminiProvider : public AiProvider {
public:
    using AiProvider::AiProvider;

    HttpSpec buildRequest(const AiRequest &req, const QString &model) const override
    {
        QJsonArray contents;
        for (const AiMessage &m : normalizeTurns(req.messages)) {
            QJsonArray parts;
            QJsonObject t;
            t["text"] = m.text;
            parts.append(t);
            if (!m.imageJpeg.isEmpty()) {
                QJsonObject inl;
                inl["mimeType"] = "image/jpeg";
                inl["data"] = QString::fromLatin1(m.imageJpeg.toBase64());
                QJsonObject img;
                img["inlineData"] = inl;
                parts.append(img);
            }
            QJsonObject c;
            c["role"] = m.role == "assistant" ? "model" : "user";
            c["parts"] = parts;
            contents.append(c);
        }

        QJsonObject gen;
        gen["temperature"] = req.temperature;
        gen["maxOutputTokens"] = req.maxTokens;
        if (req.jsonMode)
            gen["responseMimeType"] = "application/json";

        HttpSpec h;
        h.host = m_cfg.host;
        h.path = "/v1beta/models/" + model + ":generateContent";
        h.body["contents"] = contents;
        h.body["generationConfig"] = gen;
        if (!req.system.isEmpty()) {
            QJsonObject sp;
            sp["text"] = req.system;
            QJsonObject si;
            si["parts"] = QJsonArray{sp};
            h.body["systemInstruction"] = si;
        }

        h.headers["x-goog-api-key"] = m_cfg.apiKey;
        h.headers["Content-Type"] = "application/json";
        for (auto it = m_cfg.extraHeaders.constBegin(); it != m_cfg.extraHeaders.constEnd(); ++it)
            h.headers[it.key()] = it.value();
        return h;
    }

    AiResult parseResponse(const QJsonObject &r) const override
    {
        if (r.isEmpty())
            return emptyResponse();

        AiResult res;
        if (r.contains("error")) {
            const QJsonObject e = r["error"].toObject();
            res.error = makeError(e["code"].toInt(), e["status"].toString(), e["message"].toString());
            return res;
        }

        const QJsonObject cand = r["candidates"].toArray().at(0).toObject();
        for (const QJsonValue &p : cand["content"].toObject()["parts"].toArray())
            res.text += p.toObject()["text"].toString();

        if (res.text.trimmed().isEmpty()) {
            const QString block = r["promptFeedback"].toObject()["blockReason"].toString();
            if (!block.isEmpty()) {
                res.error.kind = AiError::BadRequest;
                res.error.message = "Gemini заблокировал запрос: " + block;
                return res;
            }
            return emptyText();
        }
        res.ok = true;
        return res;
    }
};



std::unique_ptr<AiProvider> AiProvider::create(const ProviderConfig &cfg)
{
    const QString t = cfg.type.toLower();
    if (t == "openai")    return std::make_unique<OpenAiProvider>(cfg);
    if (t == "anthropic") return std::make_unique<AnthropicProvider>(cfg);
    if (t == "gemini")    return std::make_unique<GeminiProvider>(cfg);
    return nullptr;
}

#include "aimanager.h"
#include "requests.h"
#include <QDebug>
#include <QSettings>

namespace {
struct Preset { const char *name, *type, *host, *path; };
const Preset kPresets[] = {
    {"openrouter", "openai",    "openrouter.ai",                     "/api/v1/chat/completions"},
    {"openai",     "openai",    "api.openai.com",                    "/v1/chat/completions"},
    {"groq",       "openai",    "api.groq.com",                      "/openai/v1/chat/completions"},
    {"deepseek",   "openai",    "api.deepseek.com",                  "/chat/completions"},
    {"anthropic",  "anthropic", "api.anthropic.com",                 "/v1/messages"},
    {"gemini",     "gemini",    "generativelanguage.googleapis.com", ""},
    {"xai",        "openai",    "api.x.ai",                          "/v1/chat/completions"},
};


QString joined(const QVariant &v)
{
    return v.typeId() == QMetaType::QStringList ? v.toStringList().join(",") : v.toString();
}

QStringList splitList(const QVariant &v)
{
    QStringList out;
    for (const QString &s : joined(v).split(',', Qt::SkipEmptyParts))
        if (!s.trimmed().isEmpty())
            out << s.trimmed();
    return out;
}

bool shouldFailover(AiError::Kind k)
{
    return k == AiError::Auth || k == AiError::NoCredits || k == AiError::RateLimit
           || k == AiError::Server || k == AiError::Network || k == AiError::Parse;

}
}

AiManager::AiManager(const QString &iniPath, QObject *parent)
    : QObject(parent), m_ini(iniPath)
{
    reload();
}

void AiManager::reload()
{
    m_providers.clear();
    m_fallbacks.clear();
    m_agentTargets.clear();

    QSettings s(m_ini, QSettings::IniFormat);
    m_active = joined(s.value("ai/provider")).trimmed();
    m_fallbacks = splitList(s.value("ai/fallback"));

    for (const QString &g : s.childGroups()) {
        if (!g.startsWith("provider_"))
            continue;

        ProviderConfig c;
        c.name = g.mid(9);
        s.beginGroup(g);
        c.type   = joined(s.value("type")).trimmed();
        c.host   = joined(s.value("host")).trimmed();
        c.path   = joined(s.value("path")).trimmed();
        c.apiKey = joined(s.value("api_key")).trimmed();
        c.model  = joined(s.value("model")).trimmed();
        c.quirks = joined(s.value("quirks")).trimmed();
        for (const QString &k : s.childKeys())
            if (k.startsWith("header_"))
                c.extraHeaders[k.mid(7)] = joined(s.value(k));
        s.endGroup();

        for (const Preset &p : kPresets) {
            if (c.name == QLatin1String(p.name)) {
                if (c.type.isEmpty()) c.type = p.type;
                if (c.host.isEmpty()) c.host = p.host;
                if (c.path.isEmpty()) c.path = p.path;
            }
        }
        if (c.apiKey.isEmpty())
            c.apiKey = qEnvironmentVariable((c.name.toUpper() + "_API_KEY").toUtf8().constData());

        std::shared_ptr<AiProvider> prov = AiProvider::create(c);
        if (!prov) {
            qWarning() << "[AI] провайдер" << c.name << ": неизвестный type" << c.type;
            continue;
        }
        m_providers.insert(c.name, prov);
    }

    // совместимость со старым config.ini
    if (m_providers.isEmpty()) {
        ProviderConfig c;
        c.name = "openrouter";
        c.type = "openai";
        c.host = "openrouter.ai";
        c.path = "/api/v1/chat/completions";
        c.quirks = "openrouter";
        c.apiKey = joined(s.value("openrouter_api_key")).trimmed();
        if (c.apiKey.isEmpty())
            c.apiKey = qEnvironmentVariable("OPENROUTER_API_KEY");
        c.model = joined(s.value("ai_model")).trimmed();
        c.extraHeaders["HTTP-Referer"] = "https://localhost/agents";
        c.extraHeaders["X-Title"] = "Agents Orchestrator";
        std::shared_ptr<AiProvider> prov = AiProvider::create(c);
        m_providers.insert(c.name, prov);
        if (m_active.isEmpty())
            m_active = "openrouter";
    }

    s.beginGroup("agent_targets");
    for (const QString &k : s.childKeys())
        m_agentTargets.insert(k, joined(s.value(k)).trimmed());
    s.endGroup();

    qInfo() << "[AI] провайдеры:" << providers() << "активный:" << m_active << "fallback:" << m_fallbacks;
}

bool AiManager::isConfigured() const
{
    for (const auto &p : m_providers)
        if (!p->config().apiKey.isEmpty())
            return true;
    return false;
}

QStringList AiManager::providers() const
{
    return m_providers.keys();
}

void AiManager::setActive(const QString &target)
{
    m_active = target.trimmed();
    QSettings(m_ini, QSettings::IniFormat).setValue("ai/provider", m_active);
}

void AiManager::setAgentTarget(const QString &agent, const QString &target)
{
    QSettings s(m_ini, QSettings::IniFormat);
    if (target.trimmed().isEmpty()) {
        m_agentTargets.remove(agent);
        s.remove("agent_targets/" + agent);
    } else {
        m_agentTargets[agent] = target.trimmed();
        s.setValue("agent_targets/" + agent, target.trimmed());
    }
}

AiManager::Target AiManager::parseTarget(const QString &spec)
{
    Target t;
    const int i = spec.indexOf(':');
    t.provider = (i < 0 ? spec : spec.left(i)).trimmed();
    t.model = i < 0 ? QString() : spec.mid(i + 1).trimmed();
    return t;
}

QList<AiManager::Target> AiManager::chainFor(const QString &agent) const
{
    QList<Target> out;
    auto add = [&](const QString &spec) {
        const Target t = parseTarget(spec);
        if (t.provider.isEmpty() || !m_providers.contains(t.provider))
            return;
        for (const Target &x : out)
            if (x.provider == t.provider && x.model == t.model)
                return;
        out.append(t);
    };

    if (m_agentTargets.contains(agent))
        add(m_agentTargets.value(agent));
    add(m_active);
    for (const QString &f : m_fallbacks)
        add(f);
    return out;
}

void AiManager::request(const AiRequest &req, const QString &agent, Callback cb)
{
    auto chain = std::make_shared<QList<Target>>(chainFor(agent));
    tryTarget(chain, 0, req, std::move(cb), AiResult());
}

void AiManager::tryTarget(std::shared_ptr<QList<Target>> chain, int idx, AiRequest req,
                          Callback cb, AiResult last)
{
    while (idx < chain->size()) {
        const Target t = chain->at(idx);
        std::shared_ptr<AiProvider> prov = m_providers.value(t.provider);
        const QString model = t.model.isEmpty() ? (prov ? prov->config().model : QString()) : t.model;

        if (!prov || prov->config().apiKey.isEmpty() || model.isEmpty()) {
            qWarning() << "[AI] пропускаю" << t.provider << ": нет api_key или модели";
            ++idx;
            continue;
        }

        const HttpSpec http = prov->buildRequest(req, model);
        qInfo() << "[AI] запрос:" << t.provider << model << "сообщений:" << req.messages.size();

        Requests::apiCall(this, http.host, http.path, "POST", http.body, http.headers,
                          [this, chain, idx, req, cb, prov, t, model](const QJsonObject &resp) {
                              const AiResult res = prov->parseResponse(resp);
                              if (res.ok) {
                                  cb(res.toEnvelope());
                                  return;
                              }
                              qWarning() << "[AI] сырой ответ:"
                                         << QJsonDocument(resp).toJson(QJsonDocument::Compact).left(800);
                              qWarning() << "[AI]" << t.provider << model << "ошибка" << int(res.error.kind)
                                         << res.error.message.left(300);

                              if (shouldFailover(res.error.kind) && idx + 1 < chain->size()) {
                                  qInfo() << "[AI] переключаюсь на следующего провайдера";
                                  tryTarget(chain, idx + 1, req, cb, res);
                                  return;
                              }
                              cb(res.toEnvelope());
                          });
        return;
    }

    if (last.error.kind != AiError::None) {
        cb(last.toEnvelope());
        return;
    }
    AiResult none;
    none.error.kind = AiError::Auth;
    none.error.message = "не настроен ни один AI-провайдер (api_key / model в config.ini)";
    cb(none.toEnvelope());
}

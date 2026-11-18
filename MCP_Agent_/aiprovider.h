#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <memory>

struct AiMessage {
    QString role;
    QString text;
    QByteArray imageJpeg;
};

struct AiRequest {
    QString system;
    QList<AiMessage> messages;
    int maxTokens = 2048;
    double temperature = 0.3;
    bool jsonMode = true;
};

struct AiError {
    enum Kind { None, Auth, NoCredits, RateLimit, Server, BadRequest, Parse, Network };
    Kind kind = None;
    int http = 0;
    QString message;
};

struct AiResult {
    bool ok = false;
    QString text;
    AiError error;
    QJsonObject toEnvelope() const;
};

struct HttpSpec {
    QString host, path;
    QJsonObject body;
    QMap<QString, QString> headers;
};

struct ProviderConfig {
    QString name;
    QString type;
    QString host, path;
    QString apiKey;
    QString model;
    QString quirks;
    QMap<QString, QString> extraHeaders;
};

class AiProvider {
public:
    explicit AiProvider(ProviderConfig cfg) : m_cfg(std::move(cfg)) {}
    virtual ~AiProvider() = default;

    virtual HttpSpec buildRequest(const AiRequest &req, const QString &model) const = 0;
    virtual AiResult parseResponse(const QJsonObject &resp) const = 0;

    const ProviderConfig &config() const { return m_cfg; }
    static std::unique_ptr<AiProvider> create(const ProviderConfig &cfg);

protected:
    ProviderConfig m_cfg;
};

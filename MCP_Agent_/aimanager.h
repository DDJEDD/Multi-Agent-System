#pragma once
#include <QHash>
#include <QObject>
#include <QStringList>
#include <functional>
#include <memory>
#include "aiprovider.h"

class AiManager : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const QJsonObject &envelope)>;

    explicit AiManager(const QString &iniPath, QObject *parent = nullptr);

    void reload();
    bool isConfigured() const;
    QStringList providers() const;
    QString activeTarget() const { return m_active; }

    void setActive(const QString &target);
    void setAgentTarget(const QString &agent, const QString &target);

    void request(const AiRequest &req, const QString &agent, Callback cb);

private:
    struct Target { QString provider, model; };

    static Target parseTarget(const QString &spec);
    QList<Target> chainFor(const QString &agent) const;
    void tryTarget(std::shared_ptr<QList<Target>> chain, int idx, AiRequest req,
                   Callback cb, AiResult last);

    QString m_ini;
    QString m_active;
    QStringList m_fallbacks;
    QHash<QString, QString> m_agentTargets;
    QHash<QString, std::shared_ptr<AiProvider>> m_providers;
};

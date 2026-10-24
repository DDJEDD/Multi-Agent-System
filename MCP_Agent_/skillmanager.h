#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMap>
#include <functional>
struct PluginInfo {
    QString name;
    QString dir;
    QString exec;
    QStringList args;
    int timeoutMs = 30000;
};
class SkillManager : public QObject {
    Q_OBJECT
private:
    QString plugins_path;
    QMap<QString, QString> functionToPluginMap;
    QHash<QString, PluginInfo> pluginsByName;
public:
    explicit SkillManager(QObject *parent = nullptr, QString plugins = QString());

    QStringList listPlugins() const;
    QString getSkilsFile(QString plugin_name) const;
    PluginInfo pluginForFunction(const QString &functionName) const;

    void loadAllPluginManifests();
    bool hasFunction(const QString &functionName) const;
    QString getPluginForFunction(const QString &functionName) const;
};
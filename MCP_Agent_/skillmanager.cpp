#include "skillmanager.h"
#include "filemanager.h"
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

SkillManager::SkillManager(QObject *parent, QString plugins) : QObject(parent) {
    plugins_path = plugins.isEmpty() ? "/PLUGINS" : plugins;
    loadAllPluginManifests();
}
void SkillManager::loadAllPluginManifests() {
    functionToPluginMap.clear();
    pluginsByName.clear();

    QDir pluginRoot(QCoreApplication::applicationDirPath() + plugins_path);
    if (!pluginRoot.exists()) return;

    const QStringList pluginDirs = pluginRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &pluginName : pluginDirs) {
        QDir pDir(pluginRoot.absoluteFilePath(pluginName));
        const QString manifestContent = FileManager::getFile("manifest", pDir);
        if (manifestContent.isEmpty()) continue;

        const QJsonDocument doc = QJsonDocument::fromJson(manifestContent.toUtf8());
        if (!doc.isObject()) {
            qWarning() << "[SkillManager] Некорректный manifest у плагина" << pluginName;
            continue;
        }
        const QJsonObject obj = doc.object();

        PluginInfo info;
        info.name = pluginName;
        info.dir = pDir.absolutePath();
        info.exec = obj["exec"].toString().trimmed();
        info.timeoutMs = obj["timeout_ms"].toInt(30000);
        for (const QJsonValue &a : obj["args"].toArray())
            info.args << a.toString();
        pluginsByName[pluginName] = info;

        for (const QJsonValue &fVal : obj["functions"].toArray()) {
            const QString funcName = fVal.toObject()["name"].toString().trimmed();
            if (funcName.isEmpty()) continue;
            functionToPluginMap[funcName] = pluginName;
            qDebug() << "[SkillManager] Зарегистрирована функция" << funcName << "от плагина" << pluginName;
        }
    }
}

PluginInfo SkillManager::pluginForFunction(const QString &functionName) const {
    return pluginsByName.value(functionToPluginMap.value(functionName));
}

QStringList SkillManager::listPlugins() const {
    QDir pluginRoot(QCoreApplication::applicationDirPath() + plugins_path);
    if (!pluginRoot.exists()) return {};
    return pluginRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
}

QString SkillManager::getSkilsFile(QString plugin_name) const {
    QDir pluginRoot(QCoreApplication::applicationDirPath() + plugins_path + "/" + plugin_name);
    return FileManager::getFile("SKILL", pluginRoot);
}

bool SkillManager::hasFunction(const QString &functionName) const {
    return functionToPluginMap.contains(functionName);
}

QString SkillManager::getPluginForFunction(const QString &functionName) const {
    return functionToPluginMap.value(functionName, QString());
}
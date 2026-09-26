#include "skillmanager.h"
#include "filemanager.h"
#include <QDir>
#include <QCoreApplication>
SkillManager::SkillManager(QObject *parent, QString plugins) : QObject(parent) { if(plugins.isEmpty()) {plugins_path="/Plugins";}}


QStringList SkillManager::listPlugins() const {
    QDir pluginRoot(QCoreApplication::applicationDirPath() + plugins_path);
    if (!pluginRoot.exists())
        return {};
    return pluginRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
}


QString SkillManager::getSkilsFile(QString plugin_name) const {
    QDir pluginRoot(QCoreApplication::applicationDirPath() + plugins_path + "/"+ plugin_name);
    QString file = FileManager::getFile("SKILL", pluginRoot);
    if(file.isEmpty()){
        qDebug() << "файл скилза для плагина " << plugin_name << "пустой или не существует.";
        return "";
    }
    return file;
}

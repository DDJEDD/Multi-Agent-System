#ifndef SKILLMANAGER_H
#define SKILLMANAGER_H
#include <QObject>
#include <QStringList>

class SkillManager: public QObject
{
    Q_OBJECT
    QString plugins_path;
public:
    explicit SkillManager(QObject *parent = nullptr, QString plugins = "");
    QStringList listPlugins() const;
    QString getSkilsFile(QString plugin_name) const;
};

#endif // SKILLMANAGER_H

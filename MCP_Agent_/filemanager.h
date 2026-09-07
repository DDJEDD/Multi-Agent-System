#ifndef MCP_H
#define MCP_H
#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDir>
class FileManager
{
public:
    static void SaveMessage(qint64 userid,  const QString &userprompt,  const QString &airesp);
    static QString GetOldMessages(qint64 userid);
    static QDir createDirectory(const QString &dirpath);
    static void createAndEditFile(const QString &content, const  QDir &dirEngine, const QString &fileName);
    static bool deleteRepo(const QString &agentName, QDir dirEngine);
    static void renameDirectory(const QString &oldpath, const QString &dirpath);
    static QString getFile(const QString &fileToGet, QDir dirEngine);
};

#endif // MCP_H

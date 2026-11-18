#include "filemanager.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTextStream>

static QString historyPath(qint64 userid)
{
    QDir d(QCoreApplication::applicationDirPath() + "/History");
    d.mkpath(".");
    return d.absoluteFilePath(QString::number(userid) + ".jsonl");
}

void FileManager::createAndEditFile(const QString &content, const QDir &dirEngine, const QString &fileName)
{
    QString fullFilePath = dirEngine.absoluteFilePath(fileName + ".md");
    QFile file(fullFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << content;
        file.close();
        qDebug() << "Создан файл:" << fileName + ".md";
    } else {
        qWarning() << "Не удалось создать файл:" << file.errorString();
    }
}

bool FileManager::deleteRepo(const QString &agentName, QDir dirEngine)
{
    bool success = dirEngine.removeRecursively();
    if (success) {
        qDebug() << "Субагент" << agentName << "успешно удален.";
    } else {
        qCritical() << "Не удалось удалить папку субагента:" << dirEngine.absolutePath();
    }
    return success;
}

QDir FileManager::createDirectory(const QString &dirpath)
{
    QDir agentDir(dirpath);
    if (agentDir.mkpath(".")) {
        qDebug() << "Папка успешно создана или уже существует по пути:" << dirpath;
    } else {
        qWarning() << "Не удалось создать папку по пути:" << dirpath;
    }
    return agentDir;
}

void FileManager::renameDirectory(const QString &oldpath, const QString &dirpath)
{
    QDir dir;
    if (dir.rename(oldpath, dirpath))
        qDebug() << "Субагент успешно переименован";
    else
        qDebug() << "Субагент не переименовался по причине ошибки.";
}

void FileManager::SaveMessage(qint64 userid, const QString &userprompt, const QString &airesp)
{
    QFile userfile(historyPath(userid));
    if (!userfile.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Could not open history file for writing:" << userfile.errorString();
        return;
    }
    QJsonObject obj;
    obj["user_prompt"] = userprompt;
    obj["ai_resp"] = airesp;
    obj["datetime"] = QDateTime::currentDateTime().toString("yyyy::MM::dd::hh::mm::ss");
    userfile.write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
    userfile.close();
}

//я заебался пока писал єто
QList<QPair<QString, QString>> FileManager::GetOldPairs(qint64 userid, int maxPairs)
{
    QFile f(historyPath(userid));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QList<QPair<QString, QString>> all;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine().trimmed();
        if (line.isEmpty()) continue;
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        if (o.isEmpty()) continue;
        all.append({o["user_prompt"].toString(), o["ai_resp"].toString()});
    }
    f.close();

    const int MAX_CHARS = 3000;
    const int MAX_BYTES = 32768;
    auto clip = [&](const QString &s) { return s.size() > MAX_CHARS ? s.left(MAX_CHARS) + "…" : s; };

    QList<QPair<QString, QString>> out;
    int total = 0;
    for (int i = all.size() - 1; i >= 0 && out.size() < maxPairs; --i) {
        const QString u = clip(all[i].first);
        const QString a = clip(all[i].second);
        const int sz = u.toUtf8().size() + a.toUtf8().size();
        if (total + sz > MAX_BYTES && !out.isEmpty()) break;
        total += sz;
        out.prepend({u, a});
    }
    return out;
}

QString FileManager::getFile(const QString &fileToGet, QDir dirEngine)
{
    QString fullFilePath = dirEngine.absoluteFilePath(fileToGet + ".md");
    QFile file(fullFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Не удалось открыть файл для чтения:" << fullFilePath
                   << "Ошибка:" << file.errorString();
        return QString();
    }
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    return content;
}

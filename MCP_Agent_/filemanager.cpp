#include "filemanager.h"
#include <QJsonDocument>
#include <QDebug>
#include <QJsonObject>
#include <QFile>
#include <QSettings>
void FileManager::createAndEditFile(const QString &content, const  QDir &dirEngine, const QString &fileName){
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
bool FileManager::deleteRepo(const QString &agentName,  QDir dirEngine){
    bool success = dirEngine.removeRecursively();
    if (success) {
        qDebug() << "Субагент" << agentName << "успешно удален.";
    } else {
        qCritical() << "Не удалось удалить папку субагента:" << dirEngine.absolutePath();
    }

    return success;
}
QDir FileManager::createDirectory(const QString &dirpath) {
    QDir agentDir(dirpath);

    if (agentDir.mkpath(".")) {
        qDebug() << "Папка успешно создана или уже существует по пути:" << dirpath;
    } else {
        qWarning() << "Не удалось создать папку по пути:" << dirpath;
    }
    return agentDir;
}

void FileManager::renameDirectory(const QString &oldpath, const QString &dirpath){
    QDir dir;
    if(dir.rename(oldpath, dirpath)) {
        qDebug() << "Субагент успешно переименован";

    }
    else{
        qDebug() << "Субагент не переименовался по причине ошибки.";
    }
}


void FileManager::SaveMessage(qint64 userid, const QString &userprompt,  const QString &airesp){
    QFile userfile(QString::number(userid) +".jsonl");
    if (!userfile.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Could not open file for writing:" << userfile.errorString();
        return;
    }
    QJsonObject obj;
    obj["user_prompt"] = userprompt;
    obj["ai_resp"] = airesp;
    QDateTime dateTime = QDateTime::currentDateTime();
    QString result = dateTime.toString("yyyy::MM::dd::hh::mm::ss");
    obj["datetime"] = result;
    QJsonDocument json(obj);
    userfile.write(json.toJson(QJsonDocument::Compact) + "\n");

    userfile.close();

}
QString FileManager::GetOldMessages(qint64 userid) {
    QFile userfile(QString::number(userid) + ".jsonl");
    if (!userfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open file for reading:" << userfile.errorString();
        return "";
    }


    QList<QPair<QString, QString>> entries;
    QTextStream in(&userfile);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) continue;
        const QJsonObject o = doc.object();
        entries.append({o["user_prompt"].toString(), o["ai_resp"].toString()});
    }
    userfile.close();

    const int MAX_BYTES = 4096;
    const int MAX_FIELD_CHARS = 400;
    auto clip = [&](const QString &s) {
        return s.size() > MAX_FIELD_CHARS ? s.left(MAX_FIELD_CHARS) + "…" : s;
    };

    QStringList blocks;
    int total = 0;
    for (int i = entries.size() - 1; i >= 0; --i) {
        const QString block = "User: " + clip(entries[i].first) + "\n"
                              + "AI: " + clip(entries[i].second) + "\n\n";
        const int sz = block.toUtf8().size();
        if (total + sz > MAX_BYTES) break;
        total += sz;
        blocks.prepend(block);
    }
    return blocks.join("");
}
QString FileManager::getFile(const QString &fileToGet, QDir dirEngine){
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



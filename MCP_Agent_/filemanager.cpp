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

QString FileManager::GetOldMessages(qint64 userid){
    QFile userfile(QString::number(userid) +".jsonl");
    if (!userfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open file for reading:" << userfile.errorString();
        return "";
    }
    QTextStream in(&userfile);
    QString prompt;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject()) {
            QJsonObject logObj = doc.object();
            prompt.append("User: " + logObj["user_prompt"].toString() + "\n");
            prompt.append("AI: " + logObj["ai_resp"].toString() + "\n\n");

        }
    }
    userfile.close();
    return prompt;
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



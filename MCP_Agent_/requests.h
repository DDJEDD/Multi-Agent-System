#ifndef REQUESTS_H
#define REQUESTS_H
#include <QJsonObject>
#include <QJsonDocument>
#include <QSslSocket>
#include <QJsonObject>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QApplication>
class Requests
{
public:
    Requests();
    static void apiCall(QObject *parent, const QString &host, const QString &path,  const QString &method,
                 const QJsonObject &body,
                 const QMap<QString, QString> &headers,
                 std::function<void(const QJsonObject &)> callback);
    static void downloadFileCall(QObject *parent,
                          const QString &host,
                          const QString &path,
                          std::function<void(const QByteArray &)> callback);
};
#endif // REQUESTS_H

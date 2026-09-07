#ifndef JSONPARSER_H
#define JSONPARSER_H
#include <QString>
#include <QList>
#include <QJsonObject>
#include <optional>
#include "phonenumber.h"
struct DelayedMessage {
    QString text;
    QString stickerId;
    QString photoUrl;
    QString code;
    int delay;
};
struct AgentCall {
    QString id;
    QString agentName;
    QString functionName;
    QString role;
    QVariantMap args;
};

class JSONParser  {
public:

    static std::optional<QList<AgentCall>> parse(const QJsonObject &response,
                                                 const QMap<QString, QString> &nums,
                                                 const phonenumber &phone);
private:
    static QString sanitizeJsonText(const QString &text);
    static QJsonDocument extractFirstJsonObject(const QString &rawText, QJsonParseError *error);

};

#endif

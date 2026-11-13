#pragma once
#include <QObject>
#include <QMap>
#include <QHash>
#include <QString>
#include <QVariant>

struct MaskTable {
    QMap<QString, QString>  byTag;
    QHash<QString, QString> byValue;
};

class phonenumber : public QObject
{
    Q_OBJECT
public:
    explicit phonenumber(QObject *parent = nullptr);

    QString  mask(const QString &text, MaskTable &table) const;
    QString  restoreText(const QString &text, const MaskTable &table) const;
    QVariant restoreDeep(const QVariant &v, const MaskTable &table) const;
};

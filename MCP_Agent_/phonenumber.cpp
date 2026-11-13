#include "phonenumber.h"
#include <QRegularExpression>

phonenumber::phonenumber(QObject *parent) : QObject(parent) {}

QString phonenumber::mask(const QString &text, MaskTable &table) const
{
    static const QRegularExpression re(
        QStringLiteral(R"((?<!\w)(?:\+?\d[\d\s()-]{6,}\d|-?\d{6,}))"));
    static const QRegularExpression dateRe(QStringLiteral(R"(^\d{4}-\d{2}-\d{2})"));

    QString out;
    int last = 0;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString val = m.captured(0);
        if (dateRe.match(val).hasMatch())
            continue;

        QString tag = table.byValue.value(val);
        if (tag.isEmpty()) {
            tag = QStringLiteral("[ID_%1]").arg(table.byTag.size() + 1);
            table.byValue.insert(val, tag);
            table.byTag.insert(tag, val);
        }
        out += text.mid(last, m.capturedStart() - last) + tag;
        last = m.capturedEnd();
    }
    return out + text.mid(last);
}

QString phonenumber::restoreText(const QString &text, const MaskTable &table) const
{
    QString s = text;
    for (auto it = table.byTag.cbegin(); it != table.byTag.cend(); ++it)
        s.replace(it.key(), it.value());
    return s;
}

QVariant phonenumber::restoreDeep(const QVariant &v, const MaskTable &table) const
{
    switch (v.typeId()) {
    case QMetaType::QString:
        return restoreText(v.toString(), table);
    case QMetaType::QVariantList: {
        QVariantList out;
        for (const QVariant &x : v.toList())
            out << restoreDeep(x, table);
        return out;
    }
    case QMetaType::QVariantMap: {
        const QVariantMap in = v.toMap();
        QVariantMap out;
        for (auto it = in.cbegin(); it != in.cend(); ++it)
            out[it.key()] = restoreDeep(it.value(), table);
        return out;
    }
    default:
        return v;
    }
}

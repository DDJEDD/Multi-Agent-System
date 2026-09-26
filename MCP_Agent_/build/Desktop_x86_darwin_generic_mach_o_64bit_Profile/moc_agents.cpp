/****************************************************************************
** Meta object code from reading C++ file 'agents.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../agents.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'agents.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN6agentsE_t {};
} // unnamed namespace

template <> constexpr inline auto agents::qt_create_metaobjectdata<qt_meta_tag_ZN6agentsE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "agents",
        "requestSendMessagesDelayed",
        "",
        "chatId",
        "QList<DelayedMessage>",
        "messages",
        "requestThinkingContext",
        "type",
        "message",
        "requestSendPhoto",
        "photoUrl",
        "caption",
        "requestSendSticker",
        "stickerId",
        "requestError",
        "ErrorType",
        "ErrorMsg",
        "requestGeminiLog",
        "aiText",
        "requestMessageToUI",
        "requestSendMessagesToUIDelayed",
        "delayedMsgs"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'requestSendMessagesDelayed'
        QtMocHelpers::SignalData<void(qint64, const QList<DelayedMessage> &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 3 }, { 0x80000000 | 4, 5 },
        }}),
        // Signal 'requestThinkingContext'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 8 },
        }}),
        // Signal 'requestSendPhoto'
        QtMocHelpers::SignalData<void(qint64, const QString &, const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 3 }, { QMetaType::QString, 10 }, { QMetaType::QString, 11 },
        }}),
        // Signal 'requestSendSticker'
        QtMocHelpers::SignalData<void(qint64, const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 3 }, { QMetaType::QString, 13 },
        }}),
        // Signal 'requestError'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 }, { QMetaType::QString, 16 },
        }}),
        // Signal 'requestGeminiLog'
        QtMocHelpers::SignalData<void(const QString &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 18 },
        }}),
        // Signal 'requestMessageToUI'
        QtMocHelpers::SignalData<void(const QList<DelayedMessage> &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
        // Signal 'requestSendMessagesToUIDelayed'
        QtMocHelpers::SignalData<void(qint64, const QList<DelayedMessage>)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 3 }, { 0x80000000 | 4, 21 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<agents, qt_meta_tag_ZN6agentsE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject agents::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6agentsE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6agentsE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6agentsE_t>.metaTypes,
    nullptr
} };

void agents::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<agents *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->requestSendMessagesDelayed((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<DelayedMessage>>>(_a[2]))); break;
        case 1: _t->requestThinkingContext((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->requestSendPhoto((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 3: _t->requestSendSticker((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->requestError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->requestGeminiLog((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->requestMessageToUI((*reinterpret_cast<std::add_pointer_t<QList<DelayedMessage>>>(_a[1]))); break;
        case 7: _t->requestSendMessagesToUIDelayed((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<DelayedMessage>>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (agents::*)(qint64 , const QList<DelayedMessage> & )>(_a, &agents::requestSendMessagesDelayed, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (agents::*)(const QString & , const QString & )>(_a, &agents::requestThinkingContext, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (agents::*)(qint64 , const QString & , const QString & )>(_a, &agents::requestSendPhoto, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (agents::*)(qint64 , const QString & )>(_a, &agents::requestSendSticker, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (agents::*)(const QString & , const QString & )>(_a, &agents::requestError, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (agents::*)(const QString & )>(_a, &agents::requestGeminiLog, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (agents::*)(const QList<DelayedMessage> & )>(_a, &agents::requestMessageToUI, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (agents::*)(qint64 , const QList<DelayedMessage> )>(_a, &agents::requestSendMessagesToUIDelayed, 7))
            return;
    }
}

const QMetaObject *agents::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *agents::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6agentsE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int agents::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void agents::requestSendMessagesDelayed(qint64 _t1, const QList<DelayedMessage> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void agents::requestThinkingContext(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void agents::requestSendPhoto(qint64 _t1, const QString & _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}

// SIGNAL 3
void agents::requestSendSticker(qint64 _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void agents::requestError(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void agents::requestGeminiLog(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void agents::requestMessageToUI(const QList<DelayedMessage> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void agents::requestSendMessagesToUIDelayed(qint64 _t1, const QList<DelayedMessage> _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}
QT_WARNING_POP

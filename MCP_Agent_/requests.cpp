#include "requests.h"

void Requests::downloadFileCall(QObject *parent,
                                const QString &host,
                                const QString &path,
                                std::function<void(const QByteArray &)> callback)
{
    QSslSocket *socket = new QSslSocket(parent);
    QByteArray *responseData = new QByteArray();
    socket->connectToHostEncrypted(host, 443);
    QObject::connect(socket, &QSslSocket::encrypted, [socket, host, path]() {
        QString httpRequest = QString(
                                  "GET %1 HTTP/1.1\r\n"
                                  "Host: %2\r\n"
                                  "Connection: close\r\n\r\n"
                                  ).arg(path, host);
        socket->write(httpRequest.toUtf8());
    });
    QObject::connect(socket, &QSslSocket::readyRead, [socket, responseData]() {
        responseData->append(socket->readAll());
    });
    QObject::connect(socket, &QSslSocket::disconnected, [socket, responseData, callback]() {
        int headerEnd = responseData->indexOf("\r\n\r\n");
        QByteArray fileBytes;
        if (headerEnd != -1) {
            fileBytes = responseData->mid(headerEnd + 4);
        } else {
            qWarning() << "Requests: Не удалось найти заголовки HTTP в ответе!";
        }
        callback(fileBytes);
        delete responseData;
        socket->deleteLater();
    });
    QObject::connect(socket, &QAbstractSocket::errorOccurred, [socket, responseData, callback](QAbstractSocket::SocketError) {
        qWarning() << "Requests: Ошибка QSslSocket при скачивании файла:" << socket->errorString();
        callback(QByteArray());
        delete responseData;
        socket->deleteLater();
    });
}


static bool decodeChunkedBody(const QByteArray &chunked, QByteArray &outDecoded)
{
    int pos = 0;
    const int len = chunked.size();
    outDecoded.clear();

    while (pos < len) {
        int lineEnd = chunked.indexOf("\r\n", pos);
        if (lineEnd == -1) return false;

        QByteArray sizeLine = chunked.mid(pos, lineEnd - pos);
        int semicolon = sizeLine.indexOf(';');
        if (semicolon != -1) sizeLine = sizeLine.left(semicolon);

        bool ok = false;
        int chunkSize = sizeLine.trimmed().toInt(&ok, 16);
        if (!ok) return false;

        pos = lineEnd + 2;

        if (chunkSize == 0) {

            return true;
        }

        if (pos + chunkSize > len) return false;

        outDecoded.append(chunked.mid(pos, chunkSize));
        pos += chunkSize;


        if (chunked.mid(pos, 2) != "\r\n") return false;
        pos += 2;
    }
    return false;
}
void Requests::apiCall(QObject *parent, const QString &host, const QString &path,  const QString &method,
                       const QJsonObject &body,
                       const QMap<QString, QString> &headers,
                       std::function<void(const QJsonObject &)> callback)
{
    auto *socket = new QSslSocket(parent);

    // Обязательно инициализируем чистый буфер для каждого нового запроса
    socket->setProperty("_buf", QByteArray());

    QObject::connect(socket, &QSslSocket::encrypted, socket, [socket, host, path, body, headers, method]() {
        const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QByteArray request;
        request += method.toUtf8() + " " + path.toUtf8() + " HTTP/1.1\r\n";
        request += "Host: " + host.toUtf8() + "\r\n";
        request += "Content-Type: application/json\r\n";
        request += "Content-Length: " + QByteArray::number(payload.size()) + "\r\n";
        request += "Accept-Encoding: identity\r\n";

        for (auto it = headers.begin(); it != headers.end(); ++it) {
            request += it.key().toUtf8() + ": " + it.value().toUtf8() + "\r\n";
        }

        request += "Connection: close\r\n\r\n";
        request += payload;

        socket->write(request);
    });

    QObject::connect(socket, &QSslSocket::readyRead, socket, [socket]() {
        QByteArray buf = socket->property("_buf").toByteArray();
        buf += socket->readAll();
        socket->setProperty("_buf", buf);
    });

    QObject::connect(socket, &QSslSocket::disconnected, socket, [socket, callback]() {
        const QByteArray data = socket->property("_buf").toByteArray();
        const int sep = data.indexOf("\r\n\r\n");

        if (sep == -1) {
            qWarning() << "Requests: не найден конец заголовков HTTP-ответа";
            callback(QJsonObject());
            socket->deleteLater();
            return;
        }

        const QByteArray headerBlock = data.left(sep);
        QByteArray rawBody = data.mid(sep + 4);

        const bool isChunked = headerBlock.contains("Transfer-Encoding: chunked")
                               || headerBlock.contains("transfer-encoding: chunked");

        QByteArray finalBody;
        if (isChunked) {
            if (!decodeChunkedBody(rawBody, finalBody)) {
                qWarning() << "Requests: не удалось декодировать chunked-тело ответа";
                callback(QJsonObject());
                socket->deleteLater();
                return;
            }
        } else {
            finalBody = rawBody;
        }

        QJsonObject responseJson;
        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(finalBody, &parseError);

        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            responseJson = doc.object();
        } else {
            responseJson["is_html"] = true;
            responseJson["raw_content"] = QString::fromUtf8(finalBody);
        }

        callback(responseJson);
        socket->deleteLater();
    });

    QObject::connect(socket, &QAbstractSocket::errorOccurred, socket, [socket, callback](QAbstractSocket::SocketError err) {
        if (err != QAbstractSocket::RemoteHostClosedError) {
            qWarning() << "Requests: Socket error:" << socket->errorString();
        }


        if (socket->property("_buf").toByteArray().isEmpty()) {
            callback(QJsonObject());
            socket->deleteLater();
        }
    });

    socket->connectToHostEncrypted(host, 443);
}

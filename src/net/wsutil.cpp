#include "wsutil.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QtEndian>

namespace ws {

static const char* GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

QString acceptKey(const QString& clientKey) {
    QByteArray data = (clientKey + GUID).toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
    return QString::fromLatin1(hash.toBase64());
}

QByteArray serverHandshake(const QString& clientKey) {
    QString accept = acceptKey(clientKey);
    QByteArray resp;
    resp += "HTTP/1.1 101 Switching Protocols\r\n";
    resp += "Upgrade: websocket\r\n";
    resp += "Connection: Upgrade\r\n";
    resp += "Sec-WebSocket-Accept: " + accept.toUtf8() + "\r\n";
    resp += "\r\n";
    return resp;
}

QByteArray clientHandshake(const QString& host, quint16 port, const QString& path) {
    QString key = QString::fromLatin1(QCryptographicHash::hash(
        QByteArray::number(QRandomGenerator::global()->generate64()), QCryptographicHash::Sha1).toBase64());
    QByteArray req;
    req += "GET " + path.toUtf8() + " HTTP/1.1\r\n";
    req += "Host: " + host.toUtf8() + ":" + QByteArray::number(port) + "\r\n";
    req += "Upgrade: websocket\r\n";
    req += "Connection: Upgrade\r\n";
    req += "Sec-WebSocket-Key: " + key.toUtf8() + "\r\n";
    req += "Sec-WebSocket-Version: 13\r\n";
    req += "\r\n";
    return req;
}

QString parseAcceptFromResponse(const QByteArray& response) {
    int idx = response.indexOf("Sec-WebSocket-Accept:");
    if (idx < 0) return QString();
    int start = response.indexOf(':', idx) + 1;
    int end = response.indexOf("\r\n", start);
    if (end < 0) end = response.size();
    return QString::fromLatin1(response.mid(start, end - start).trimmed());
}

QByteArray encodeFrame(Opcode op, const QByteArray& payload, bool mask) {
    QByteArray frame;
    frame.append(char(0x80 | (op & 0x0F))); // FIN + opcode

    quint64 len = (quint64)payload.size();
    if (len <= 125) {
        quint8 b = (quint8)len;
        if (mask) b |= 0x80;
        frame.append(char(b));
    } else if (len <= 0xFFFF) {
        quint8 b = 126;
        if (mask) b |= 0x80;
        frame.append(char(b));
        quint16 n = qToBigEndian((quint16)len);
        frame.append((const char*)&n, 2);
    } else {
        quint8 b = 127;
        if (mask) b |= 0x80;
        frame.append(char(b));
        quint64 n = qToBigEndian((quint64)len);
        frame.append((const char*)&n, 8);
    }

    if (mask) {
        quint32 mk = QRandomGenerator::global()->generate();
        QByteArray maskKey(reinterpret_cast<const char*>(&mk), 4);
        frame.append(maskKey);
        QByteArray masked(payload.size(), Qt::Uninitialized);
        for (int i = 0; i < payload.size(); ++i)
            masked[i] = payload[i] ^ maskKey[i & 3];
        frame.append(masked);
    } else {
        frame.append(payload);
    }
    return frame;
}

FrameParser::FrameParser(QObject* parent) : QObject(parent) {}

void FrameParser::reset() {
    state_ = S_Opcode;
    fin_ = true;
    opcode_ = OpText;
    payloadLen_ = 0;
    lenBytes_ = 0;
    needMask_ = false;
    maskKey_.clear();
    maskIdx_ = 0;
    payload_.clear();
    // pending_ 保留未消费字节
}

void FrameParser::feed(const QByteArray& data) {
    pending_.append(data);
    while (true) {
        if (state_ == S_Opcode) {
            if (pending_.size() < 2) return;
            quint8 b0 = (quint8)pending_.at(0);
            quint8 b1 = (quint8)pending_.at(1);
            fin_ = (b0 & 0x80) != 0;
            opcode_ = (Opcode)(b0 & 0x0F);
            needMask_ = (b1 & 0x80) != 0;
            quint8 len = b1 & 0x7F;
            if (len <= 125) {
                payloadLen_ = len;
                pending_.remove(0, 2);
                state_ = needMask_ ? S_Mask : S_Payload;
            } else if (len == 126) {
                lenBytes_ = 2;
                pending_.remove(0, 2);
                state_ = S_Length;
            } else { // 127
                lenBytes_ = 8;
                pending_.remove(0, 2);
                state_ = S_Length;
            }
        }

        if (state_ == S_Length) {
            if (pending_.size() < lenBytes_) return;
            if (lenBytes_ == 2) {
                quint16 n;
                memcpy(&n, pending_.constData(), 2);
                payloadLen_ = qFromBigEndian(n);
            } else {
                quint64 n = 0;
                memcpy(&n, pending_.constData(), 8);
                payloadLen_ = qFromBigEndian(n);
            }
            pending_.remove(0, lenBytes_);
            lenBytes_ = 0;
            state_ = needMask_ ? S_Mask : S_Payload;
        }

        if (state_ == S_Mask) {
            if (pending_.size() < 4) return;
            maskKey_ = pending_.left(4);
            pending_.remove(0, 4);
            maskIdx_ = 0;
            state_ = S_Payload;
        }

        if (state_ == S_Payload) {
            if ((quint64)pending_.size() < payloadLen_) return;
            payload_ = pending_.left((int)payloadLen_);
            pending_.remove(0, (int)payloadLen_);
            if (needMask_) {
                for (int i = 0; i < payload_.size(); ++i) {
                    payload_[i] = (char)((quint8)payload_.at(i) ^ (quint8)maskKey_.at(maskIdx_ & 3));
                    maskIdx_++;
                }
            }
            emitMessage();
            if (opcode_ == OpClose) {
                emit closed();
                reset();
                return;
            }
            reset();
        }
    }
}

void FrameParser::emitMessage() {
    switch (opcode_) {
    case OpText:
        emit textMessage(QString::fromUtf8(payload_));
        break;
    case OpBinary:
        emit binaryMessage(payload_);
        break;
    case OpPing:
        emit ping(payload_);
        break;
    default:
        break;
    }
}

} // namespace ws

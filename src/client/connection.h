#pragma once
#include <QObject>
#include <QTcpSocket>
#include "net/wsserver.h"

// 客户端 WebSocket 连接：完成客户端握手，收发 JSON 文本与二进制帧。
class Connection : public QObject {
    Q_OBJECT
public:
    explicit Connection(QObject* parent = nullptr);
    void connectTo(const QString& host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;
    void sendJson(const QJsonObject& m);
    void sendBinary(const QByteArray& d);

signals:
    void connected();
    void disconnected();
    void message(const QJsonObject& m);
    void binary(const QByteArray& d);
    void errorOccurred(const QString& msg);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError);

private:
    QTcpSocket* sock_ = nullptr;
    WsPeer* peer_ = nullptr;
    QByteArray hsBuf_;
    bool handshaked_ = false;
};

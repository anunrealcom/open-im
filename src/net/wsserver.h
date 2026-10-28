#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include "wsutil.h"

// 单条 WebSocket 连接封装。maskOutgoing=false 表示服务端→客户端（不加掩码）。
class WsPeer : public QObject {
    Q_OBJECT
public:
    explicit WsPeer(QTcpSocket* sock, QObject* parent = nullptr);
    void setMaskOutgoing(bool m) { maskOutgoing_ = m; }
    void sendText(const QString& s);
    void sendBinary(const QByteArray& data);
    void feed(const QByteArray& data) { parser_.feed(data); }
    QTcpSocket* socket() const { return sock_; }
    QString peerAddress() const;

signals:
    void textMessage(const QString& text);
    void binaryMessage(const QByteArray& data);
    void closed();

private slots:
    void onReadyRead();

private:
    QTcpSocket* sock_;
    ws::FrameParser parser_;
    bool maskOutgoing_ = false;
};

// 最小 WebSocket 服务端：接受连接、完成握手、产出 WsPeer。
class WsServer : public QObject {
    Q_OBJECT
public:
    explicit WsServer(QObject* parent = nullptr);
    bool listen(quint16 port);
    quint16 port() const { return port_; }

signals:
    void newPeer(WsPeer* peer);

private slots:
    void onNewConnection();
    void onHandshakeReady();

private:
    QTcpServer srv_;
    quint16 port_ = 0;
};

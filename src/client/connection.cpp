#include "connection.h"
#include <QJsonDocument>
#include <QJsonObject>

Connection::Connection(QObject* parent) : QObject(parent) {}

void Connection::connectTo(const QString& host, quint16 port) {
    if (!sock_) {
        sock_ = new QTcpSocket(this);
        connect(sock_, &QTcpSocket::connected, this, &Connection::onConnected);
        connect(sock_, &QTcpSocket::readyRead, this, &Connection::onReadyRead);
        connect(sock_, &QTcpSocket::disconnected, this, &Connection::onDisconnected);
        connect(sock_, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                this, &Connection::onError);
    }
    handshaked_ = false;
    hsBuf_.clear();
    sock_->connectToHost(host, port);
}

void Connection::disconnectFromHost() {
    if (sock_) sock_->disconnectFromHost();
}

bool Connection::isConnected() const {
    return sock_ && sock_->state() == QAbstractSocket::ConnectedState && handshaked_;
}

void Connection::onConnected() {
    sock_->write(ws::clientHandshake("host", 0)); // host/port 仅用于 Host 头，不影响路由
}

void Connection::onReadyRead() {
    if (handshaked_) return;
    hsBuf_ += sock_->readAll();
    int idx = hsBuf_.indexOf("\r\n\r\n");
    if (idx < 0) return;
    QString accept = ws::parseAcceptFromResponse(hsBuf_.left(idx));
    Q_UNUSED(accept);
    QByteArray rest = hsBuf_.mid(idx + 4);
    handshaked_ = true;
    peer_ = new WsPeer(sock_, this);
    peer_->setMaskOutgoing(true);
    connect(peer_, &WsPeer::textMessage, this, [this](const QString& t) {
        QJsonDocument d = QJsonDocument::fromJson(t.toUtf8());
        if (d.isObject()) emit message(d.object());
    });
    connect(peer_, &WsPeer::binaryMessage, this, &Connection::binary);
    if (!rest.isEmpty()) peer_->feed(rest);
    emit connected();
}

void Connection::onDisconnected() {
    emit disconnected();
}

void Connection::onError(QAbstractSocket::SocketError) {
    emit errorOccurred(sock_ ? sock_->errorString() : "连接错误");
}

void Connection::sendJson(const QJsonObject& m) {
    if (peer_) peer_->sendText(QJsonDocument(m).toJson(QJsonDocument::Compact));
}

void Connection::sendBinary(const QByteArray& d) {
    if (peer_) peer_->sendBinary(d);
}

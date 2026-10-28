#include "wsserver.h"
#include <QRegularExpression>

WsPeer::WsPeer(QTcpSocket* sock, QObject* parent)
    : QObject(parent), sock_(sock) {
    connect(sock_, &QTcpSocket::readyRead, this, &WsPeer::onReadyRead);
    connect(sock_, &QTcpSocket::disconnected, this, &WsPeer::closed);
    connect(&parser_, &ws::FrameParser::textMessage, this, &WsPeer::textMessage);
    connect(&parser_, &ws::FrameParser::binaryMessage, this, &WsPeer::binaryMessage);
    connect(&parser_, &ws::FrameParser::closed, this, [this]() { sock_->close(); });
}

QString WsPeer::peerAddress() const {
    return sock_ ? sock_->peerAddress().toString() : QString();
}

void WsPeer::onReadyRead() {
    parser_.feed(sock_->readAll());
}

void WsPeer::sendText(const QString& s) {
    if (sock_ && sock_->state() == QAbstractSocket::ConnectedState)
        sock_->write(ws::encodeFrame(ws::OpText, s.toUtf8(), maskOutgoing_));
}

void WsPeer::sendBinary(const QByteArray& data) {
    if (sock_ && sock_->state() == QAbstractSocket::ConnectedState)
        sock_->write(ws::encodeFrame(ws::OpBinary, data, maskOutgoing_));
}

WsServer::WsServer(QObject* parent) : QObject(parent) {
    connect(&srv_, &QTcpServer::newConnection, this, &WsServer::onNewConnection);
}

bool WsServer::listen(quint16 port) {
    if (srv_.listen(QHostAddress::Any, port)) {
        port_ = port;
        return true;
    }
    return false;
}

void WsServer::onNewConnection() {
    while (srv_.hasPendingConnections()) {
        QTcpSocket* sock = srv_.nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead, this, &WsServer::onHandshakeReady);
    }
}

void WsServer::onHandshakeReady() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    QByteArray buf = sock->property("hsbuf").toByteArray();
    buf += sock->readAll();
    sock->setProperty("hsbuf", buf);

    int idx = buf.indexOf("\r\n\r\n");
    if (idx < 0) return; // 请求尚未收全，继续等待

    QByteArray header = buf.left(idx);
    static const QRegularExpression re("Sec-WebSocket-Key:\\s*([^\r\n]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = re.match(QString::fromUtf8(header));
    if (!m.hasMatch()) { sock->close(); return; }

    QString key = m.captured(1).trimmed();
    sock->write(ws::serverHandshake(key));
    sock->flush();

    disconnect(sock, &QTcpSocket::readyRead, this, &WsServer::onHandshakeReady);
    sock->setProperty("hsbuf", QByteArray());

    auto* peer = new WsPeer(sock, this);
    emit newPeer(peer);
}

#include "discovery.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QSet>
#include <QDebug>
#include <QDateTime>

// 默认端口常量 — 强制 mDNS 端口（Windows/macOS/Linux 防火墙默认放行）
static constexpr quint16 kMdnsPort = 5353;
static constexpr quint16 kLegacyPort = 4799;

LanDiscovery::LanDiscovery(QObject* parent) : QObject(parent) {}

bool LanDiscovery::isLoopback(const QHostAddress& a) {
    if (a == QHostAddress::LocalHost) return true;
    if (a.toString().startsWith("127.")) return true;
    if (a.protocol() == QAbstractSocket::IPv6Protocol && a == QHostAddress("::1")) return true;
    return false;
}

void LanDiscovery::bindSocket(QUdpSocket* sock, quint16 port) {
    sock->bind(QHostAddress::Any, port,
               QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
}

QByteArray LanDiscovery::makeAnnounce() const {
    QJsonObject obj;
    obj["type"] = "yihuan-im-announce";
    obj["port"] = serverPort_;
    obj["name"] = name_;
    obj["userId"] = userId_;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool LanDiscovery::isSelfAddr(const QHostAddress& a) const {
    for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
        if (addr == a) return true;
    }
    return false;
}

void LanDiscovery::start(quint16 lanPort, quint16 serverPort, const QString& name) {
    // 强制 mDNS 端口（5353，Windows 默认放行）；忽略 config 传入的 lanPort
    Q_UNUSED(lanPort);
    lanPort_ = kMdnsPort;
    lanPortLegacy_ = kLegacyPort;
    serverPort_ = serverPort;
    name_ = name;

    // 主通道：mDNS 端口（默认 5353）—— Windows / macOS / Linux 都默认放行
    if (!sock_) {
        sock_ = new QUdpSocket(this);
        connect(sock_, &QUdpSocket::readyRead, this, &LanDiscovery::onReadyRead);
    }
    bindSocket(sock_, lanPort_);
    sock_->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);

    // 兜底通道：旧 4799 端口
    if (!sockLegacy_) {
        sockLegacy_ = new QUdpSocket(this);
        connect(sockLegacy_, &QUdpSocket::readyRead, this, &LanDiscovery::onReadyRead);
    }
    bindSocket(sockLegacy_, lanPortLegacy_);

    // 加入 mDNS 组播组（每个非 loopback IPv4 网卡都加）
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
        if (!(iface.flags() & QNetworkInterface::IsRunning)) continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                sock_->joinMulticastGroup(mcastGroup_, iface);
                break;
            }
        }
    }

    if (!broadcastTimer_) {
        broadcastTimer_ = new QTimer(this);
        connect(broadcastTimer_, &QTimer::timeout, this, &LanDiscovery::onBroadcastTick);
    }
    broadcastTimer_->start(3000);
    onBroadcastTick();  // 立即广播一次

    if (!scanTimer_) {
        scanTimer_ = new QTimer(this);
        connect(scanTimer_, &QTimer::timeout, this, &LanDiscovery::onSubnetScanTick);
    }
    scanTimer_->start(15000);
    QTimer::singleShot(800, this, &LanDiscovery::onSubnetScanTick);

    qInfo() << "[LAN] discovery started: mDNS port=" << lanPort_
            << "legacy port=" << lanPortLegacy_
            << "server port=" << serverPort_;
}

void LanDiscovery::stop() {
    if (broadcastTimer_) broadcastTimer_->stop();
    if (scanTimer_) scanTimer_->stop();
    if (sock_) sock_->close();
    if (sockLegacy_) sockLegacy_->close();
    seen_.clear();
}

void LanDiscovery::setIdentity(const QString& userId, const QString& name) {
    userId_ = userId;
    name_ = name;
    onBroadcastTick();
    onSubnetScanTick();
}

void LanDiscovery::onBroadcastTick() {
    if (!sock_) return;
    QByteArray data = makeAnnounce();

    // 主：mDNS 组播 (224.0.0.251:5353) — Windows 默认放行
    qint64 sm = sock_->writeDatagram(data, mcastGroup_, lanPort_);

    // 兜底：广播到 255.255.255.255:4799
    qint64 s1 = -1;
    if (sockLegacy_) s1 = sockLegacy_->writeDatagram(data, QHostAddress::Broadcast, lanPortLegacy_);

    // 兜底：定向广播每子网 (192.168.x.255:4799)
    int dirBcastCount = 0;
    if (sockLegacy_) {
        QSet<quint32> sentNets;
        for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
            if (addr.protocol() != QAbstractSocket::IPv4Protocol) continue;
            quint32 ip = addr.toIPv4Address();
            if (ip == 0) continue;
            quint32 net = ip & 0xFFFFFF00;
            if (sentNets.contains(net)) continue;
            sentNets.insert(net);
            QHostAddress dirBcast(net | 0xFFu);
            sockLegacy_->writeDatagram(data, dirBcast, lanPortLegacy_);
            dirBcastCount++;
        }
    }

    qInfo() << "[LAN] announce" << name_ << "port=" << serverPort_
            << "mcast=" << sm << "bcast=" << s1 << "directed=" << dirBcastCount;
}

void LanDiscovery::onSubnetScanTick() {
    if (!sockLegacy_) return;
    QByteArray data = makeAnnounce();
    int sent = 0;
    QSet<quint32> sentNets;

    for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() != QAbstractSocket::IPv4Protocol) continue;
        quint32 ip = addr.toIPv4Address();
        if (ip == 0) continue;
        quint32 net = ip & 0xFFFFFF00;
        if (sentNets.contains(net)) continue;
        sentNets.insert(net);
        for (int i = 1; i < 255; ++i) {
            quint32 target = net | quint32(i);
            if (target == ip) continue;
            QHostAddress t(target);
            if (isSelfAddr(t)) continue;
            if (isLoopback(t)) continue;
            sockLegacy_->writeDatagram(data, t, lanPortLegacy_);
            sent++;
        }
    }
    qInfo() << "[LAN] subnet scan sent=" << sent;
}

void LanDiscovery::onReadyRead() {
    auto* sock = qobject_cast<QUdpSocket*>(sender());
    if (!sock) return;
    processDatagram(sock);
}

void LanDiscovery::processDatagram(QUdpSocket* sock) {
    while (sock->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize((int)sock->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        sock->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        QJsonDocument doc = QJsonDocument::fromJson(datagram);
        QJsonObject obj = doc.object();
        QString objType = obj.isEmpty() ? QString() : obj["type"].toString();
        if (objType != "yihuan-im-announce") continue;
        quint16 announcePort = (quint16)obj["port"].toInt();
        QString name = obj["name"].toString();
        QString userId = obj["userId"].toString();
        QString host = sender.toString();
        if (host.startsWith("::ffff:")) host = host.mid(7);

        // 注意：不要按 announcePort==serverPort_ 过滤"自己发的"——双方默认 8080 会误杀合法 peer；
        // 自身回环由下方 isSelfAddr(sender) 按 IP 过滤。
        if (announcePort == 0) continue;
        if (isSelfAddr(QHostAddress(host))) continue;     // 本机回环
        if (isLoopback(QHostAddress(host))) continue;     // 127.x / ::1

        QString key = host + ":" + QString::number(announcePort);
        QString sig = name + "|" + userId;
        // 每次收到 announce 都上报：对端重启、或本端连接断开后，靠下一次广播（≤3s）自愈重连。
        // seen_ 只用于避免日志刷屏，不再阻断上报；幂等/去重交给 MainWindow::onLanPeer 处理。
        if (seen_.value(key) != sig) {
            seen_[key] = sig;
            qInfo() << "[LAN] peer available" << host << "port=" << announcePort
                    << "name=" << name << "user=" << userId;
        }
        emit peerFound(host, announcePort, name, userId);
    }
}

#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QString>
#include <QMap>
#include <QSet>
#include <QHostAddress>
#include <QNetworkInterface>

// 局域网发现（与"内网通/飞秋"同思路：UDP 广播 + 子网单播，零配置、启动即用）：
// 1) mDNS 通道（UDP 5353 + 组播 224.0.0.251）— Windows 默认放行，无需 UAC/管理员。
// 2) 兼容通道（UDP 4799 + 广播 255.255.255.255 + 定向广播 + /24 子网单播）— 兜底。
// 发现包内容：{"type":"yihuan-im-announce","port":<serverPort>,
//              "name":<displayName>,"userId":<userId>}
class LanDiscovery : public QObject {
    Q_OBJECT
public:
    explicit LanDiscovery(QObject* parent = nullptr);
    void start(quint16 lanPort, quint16 serverPort, const QString& name);
    void stop();

    // 登录后设置本机身份，使广播携带 userId；立即广播一次让局域网内他人即时感知
    void setIdentity(const QString& userId, const QString& name);

signals:
    void peerFound(const QString& host, quint16 port, const QString& name,
                   const QString& userId);

private slots:
    void onReadyRead();           // 任意 UDP socket 收到都走这里
    void onBroadcastTick();       // 3s 一次：广播
    void onSubnetScanTick();      // 15s 一次：子网单播扫描

private:
    void bindSocket(QUdpSocket* sock, quint16 port);
    void processDatagram(QUdpSocket* sock);
    QByteArray makeAnnounce() const;
    bool isSelfAddr(const QHostAddress& a) const;
    static bool isLoopback(const QHostAddress& a);

    QUdpSocket* sock_ = nullptr;            // 主：5353 mDNS
    QUdpSocket* sockLegacy_ = nullptr;      // 兜底：4799 旧端口
    QTimer* broadcastTimer_ = nullptr;
    QTimer* scanTimer_ = nullptr;
    quint16 serverPort_ = 0;
    quint16 lanPort_ = 0;            // mDNS 端口（默认 5353）
    quint16 lanPortLegacy_ = 0;      // 兜底端口（4799）
    QString name_;
    QString userId_;
    QHostAddress mcastGroup_ = QHostAddress("224.0.0.251");

    // 身份变化追踪：key -> "name|userId"
    QMap<QString, QString> seen_;
};

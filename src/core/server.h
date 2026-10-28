#pragma once
#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QString>

#include "config.h"
#include "store.h"
#include "ai.h"
#include "bots.h"
#include "discovery.h"
#include "net/wsserver.h"

struct Client {
    WsPeer* peer = nullptr;
    QString userId;
    QString name;
    bool authed = false;
};

// 服务端编排：内置 WebSocket 服务 + UDP 发现 + 聊天/AI/Bot 路由。
class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(QObject* parent = nullptr);
    ~Server();

    void stop();                  // 安全关闭：先断开所有连接并清空状态表

    bool start();                 // 监听端口（8080 起自动避让）
    quint16 port() const { return ws_.port(); }
    QString localUrl() const;

    // 局域网发现必须在 MainWindow 构造完（连好 lanPeer 信号）之后再启动，
    // 否则登录对话框期间收到的首个 peer 广播会因为没有接收者而永久丢失。
    void startLanDiscovery();

    Config* config() { return &config_; }
    Store* store() { return &store_; }

    // 登录后由客户端设置本机身份，使局域网广播携带 userId
    void setLocalIdentity(const QString& userId, const QString& name);

signals:
    void logMessage(const QString& msg);
    // 局域网内发现到其他一幻IM实例
    void lanPeer(const QString& host, quint16 port, const QString& name,
                 const QString& userId);

private slots:
    void onNewPeer(WsPeer* peer);
    void onLanPeerFound(const QString& host, quint16 port, const QString& name,
                        const QString& userId);
    void onText(WsPeer* peer, const QString& text);
    void onBinary(WsPeer* peer, const QByteArray& data);
    void onPeerClosed(WsPeer* peer);

private:
    void handleHello(WsPeer* peer, const QJsonObject& msg);
    void handleChat(WsPeer* peer, const QJsonObject& msg);
    void handleTyping(WsPeer* peer, const QJsonObject& msg);
    void handleAiRequest(WsPeer* peer, const QJsonObject& msg);
    void handleBotMessage(WsPeer* peer, const QJsonObject& msg);

    void sendTo(const QString& userId, const QJsonObject& msg);
    void broadcastPresence();
    QJsonArray presenceUsers() const;
    void flushOffline(const QString& userId);

    Config config_;
    Store store_;
    AiProxy ai_;
    BotManager bots_;
    LanDiscovery discovery_;
    WsServer ws_;

    QHash<WsPeer*, Client*> clients_;        // peer -> client
    QHash<QString, WsPeer*> online_;         // userId -> peer
    QHash<QString, QJsonArray> offline_;     // userId -> 离线消息
    QHash<QString, QJsonArray> conversations_; // conversationId -> 历史
};

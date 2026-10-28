#include "server.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostInfo>
#include <QDateTime>
#include <QRandomGenerator>
#include <QSharedPointer>
#include <utility>
#include <QStandardPaths>

Server::Server(QObject* parent)
    : QObject(parent), bots_(&config_), ws_(this) {
    connect(&ws_, &WsServer::newPeer, this, &Server::onNewPeer);
    connect(&discovery_, &LanDiscovery::peerFound, this, &Server::onLanPeerFound);
}

void Server::setLocalIdentity(const QString& userId, const QString& name) {
    discovery_.setIdentity(userId, name);
}

void Server::onLanPeerFound(const QString& host, quint16 port, const QString& name,
                            const QString& userId) {
    emit lanPeer(host, port, name, userId);
}

Server::~Server() {
    stop();
}

void Server::stop() {
    // 清空业务状态表，使随后 ws_ 成员析构时触发的 closed 信号不再访问已释放的哈希
    qDeleteAll(clients_);
    clients_.clear();
    online_.clear();
    offline_.clear();
    conversations_.clear();
    discovery_.stop();
}

bool Server::start() {
    if (!config_.load(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config.json"))
        qWarning() << "配置加载失败";
    store_.setBaseDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/history");

    // 端口自动避让
    quint16 port = config_.serverPort();
    bool ok = false;
    for (int i = 0; i < 20 && !ok; ++i) {
        if (ws_.listen(port + i)) { ok = true; break; }
    }
    if (!ok) { emit logMessage("无法监听任何端口"); return false; }

    emit logMessage(QString("一幻IM 服务端已启动，端口 %1").arg(ws_.port()));
    return true;
}

// 由 main() 在 MainWindow 构造完成后调用：此时 lanPeer 信号已接上，
// 发现到的 peer 才不会因没有接收者而永久丢失。
void Server::startLanDiscovery() {
    if (config_.lanEnabled())
        discovery_.start(config_.lanPort(), ws_.port(), config_.displayName());
}

QString Server::localUrl() const {
    return QString("ws://127.0.0.1:%1").arg(ws_.port());
}

void Server::onNewPeer(WsPeer* peer) {
    auto* c = new Client();
    c->peer = peer;
    clients_[peer] = c;
    connect(peer, &WsPeer::textMessage, this, [this, peer](const QString& t) { onText(peer, t); });
    connect(peer, &WsPeer::binaryMessage, this, [this, peer](const QByteArray& d) { onBinary(peer, d); });
    connect(peer, &WsPeer::closed, this, [this, peer]() { onPeerClosed(peer); });
}

void Server::onText(WsPeer* peer, const QString& text) {
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject msg = doc.object();
    QString type = msg["type"].toString();
    Client* c = clients_.value(peer);

    if (type == "hello") { handleHello(peer, msg); return; }
    if (!c || !c->authed) { return; } // 必须先 hello

    if (type == "chat") handleChat(peer, msg);
    else if (type == "typing") handleTyping(peer, msg);
    else if (type == "ai-request") handleAiRequest(peer, msg);
    else if (type == "bot-message") handleBotMessage(peer, msg);
}

void Server::onBinary(WsPeer* peer, const QByteArray& data) {
    int nl = data.indexOf('\n');
    if (nl < 0) return;
    QJsonObject hdr = QJsonDocument::fromJson(data.left(nl)).object();
    QString to = hdr["to"].toString();
    WsPeer* target = online_.value(to);
    if (target) target->sendBinary(data); // 原样转发整帧（含头+内容）
}

void Server::onPeerClosed(WsPeer* peer) {
    Client* c = clients_.value(peer);
    if (c) {
        if (!c->userId.isEmpty()) online_.remove(c->userId);
        clients_.remove(peer);
        delete c;
    }
    broadcastPresence();
    peer->deleteLater();
}

void Server::handleHello(WsPeer* peer, const QJsonObject& msg) {
    Client* c = clients_.value(peer);
    if (!c) return;

    if (config_.authEnabled() && msg["token"].toString() != config_.authToken()) {
        QJsonObject err; err["type"] = "error";
        err["message"] = "认证失败";
        peer->sendText(QJsonDocument(err).toJson(QJsonDocument::Compact));
        peer->socket()->close();
        return;
    }

    QString userId = msg["userId"].toString().trimmed();
    if (userId.isEmpty()) userId = "u-" + QString::number(QRandomGenerator::global()->generate() % 1000000);
    // 保证唯一
    QString base = userId;
    int suffix = 1;
    while (online_.contains(userId)) { userId = base + "-" + QString::number(suffix++); }

    c->userId = userId;
    c->name = msg["name"].toString().trimmed();
    if (c->name.isEmpty()) c->name = userId;
    c->authed = true;
    online_[userId] = peer;

    QJsonObject welcome;
    welcome["type"] = "welcome";
    welcome["self"] = QJsonObject{{"userId", userId}, {"name", c->name}};
    welcome["users"] = presenceUsers();
    welcome["serverConfig"] = QJsonObject{{"webrtc", config_.webrtc()}};
    peer->sendText(QJsonDocument(welcome).toJson(QJsonDocument::Compact));

    emit logMessage(QString("用户上线: %1 (%2)").arg(c->name, userId));
    broadcastPresence();
    flushOffline(userId);
}

void Server::handleChat(WsPeer* peer, const QJsonObject& msg) {
    Client* c = clients_.value(peer);
    if (!c) return;
    QString to = msg["to"].toString();

    // 发给机器人的消息
    QJsonObject bot = config_.findBot(to);
    if (!bot.isEmpty()) {
        peer->sendText(QJsonDocument(QJsonObject{
            {"type", "chat-ack"}, {"id", msg["id"].toString()}}).toJson(QJsonDocument::Compact));
        QString fromId = c->userId;
        QString text = msg["text"].toString();
        QString cid = "bot:" + to + ":" + fromId;
        store_.append(cid, msg);
        bots_.handle(to, text, fromId,
            [this, peer, to, fromId, id = msg["id"].toString()](const QString& reply) {
                QJsonObject bm;
                bm["type"] = "bot-message";
                bm["id"] = id;
                bm["from"] = to;
                bm["to"] = fromId;
                bm["text"] = reply;
                bm["ts"] = QDateTime::currentMSecsSinceEpoch();
                peer->sendText(QJsonDocument(bm).toJson(QJsonDocument::Compact));
            },
            []() {}, this);
        return;
    }

    // 普通用户消息
    QJsonObject out = msg;
    out["from"] = c->userId;
    out["fromName"] = c->name;
    if (!out.contains("ts")) out["ts"] = QDateTime::currentMSecsSinceEpoch();

    peer->sendText(QJsonDocument(QJsonObject{
        {"type", "chat-ack"}, {"id", msg["id"].toString()}}).toJson(QJsonDocument::Compact));

    QString cid = (c->userId < to) ? (c->userId + "_" + to) : (to + "_" + c->userId);
    store_.append(cid, out);

    WsPeer* target = online_.value(to);
    if (target) {
        target->sendText(QJsonDocument(out).toJson(QJsonDocument::Compact));
    } else {
        offline_[to].append(out); // 对方离线，暂存
    }
}

void Server::handleTyping(WsPeer* peer, const QJsonObject& msg) {
    Client* c = clients_.value(peer);
    if (!c) return;
    QJsonObject out = msg;
    out["from"] = c->userId;
    out["fromName"] = c->name;
    WsPeer* target = online_.value(msg["to"].toString());
    if (target) target->sendText(QJsonDocument(out).toJson(QJsonDocument::Compact));
}

void Server::handleAiRequest(WsPeer* peer, const QJsonObject& msg) {
    Client* c = clients_.value(peer);
    if (!c) return;
    QString fromId = c->userId;
    QString providerId = msg["to"].toString();
    QString text = msg["text"].toString();
    QString reqId = msg["id"].toString();
    QString convId = msg["conversationId"].toString();
    if (convId.isEmpty()) convId = "ai:" + fromId;

    QJsonObject provider = config_.findAiProvider(providerId);
    if (provider.isEmpty()) provider = config_.aiProviders().first().toObject();

    QJsonArray& hist = conversations_[convId];
    hist.append(QJsonObject{{"role", "user"}, {"content", text}});

    QJsonArray openai;
    for (const auto& v : std::as_const(hist))
        openai.append(v);

    store_.append(convId, QJsonObject{{"role", "user"}, {"content", text}, {"ts", QDateTime::currentMSecsSinceEpoch()}});

    auto acc = QSharedPointer<QString>::create();
    ai_.stream(provider, openai,
        [this, fromId, reqId, acc](const QString& delta) {
            *acc += delta;
            QJsonObject tok;
            tok["type"] = "ai-token";
            tok["id"] = reqId;
            tok["delta"] = delta;
            sendTo(fromId, tok);
        },
        [this, fromId, reqId, convId, acc, &hist](const QString& err) {
            if (!err.isEmpty()) {
                sendTo(fromId, QJsonObject{{"type", "ai-error"}, {"id", reqId}, {"message", err}});
                return;
            }
            sendTo(fromId, QJsonObject{{"type", "ai-end"}, {"id", reqId}});
            if (!acc->isEmpty()) {
                hist.append(QJsonObject{{"role", "assistant"}, {"content", *acc}});
                store_.append(convId, QJsonObject{{"role", "assistant"}, {"content", *acc}, {"ts", QDateTime::currentMSecsSinceEpoch()}});
            }
        },
        this);
}

void Server::handleBotMessage(WsPeer* peer, const QJsonObject& msg) {
    Client* c = clients_.value(peer);
    if (!c) return;
    WsPeer* target = online_.value(msg["to"].toString());
    if (target) {
        QJsonObject out = msg;
        out["from"] = c->userId;
        target->sendText(QJsonDocument(out).toJson(QJsonDocument::Compact));
    }
}

void Server::sendTo(const QString& userId, const QJsonObject& msg) {
    WsPeer* target = online_.value(userId);
    if (target) target->sendText(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void Server::broadcastPresence() {
    QJsonObject p;
    p["type"] = "presence";
    p["users"] = presenceUsers();
    QByteArray data = QJsonDocument(p).toJson(QJsonDocument::Compact);
    for (auto it = online_.begin(); it != online_.end(); ++it)
        it.value()->sendText(data);
}

QJsonArray Server::presenceUsers() const {
    QJsonArray arr;
    for (auto it = online_.begin(); it != online_.end(); ++it) {
        Client* c = clients_.value(it.value());
        if (c) arr.append(QJsonObject{{"userId", c->userId}, {"name", c->name}, {"status", "online"}});
    }
    return arr;
}

void Server::flushOffline(const QString& userId) {
    QJsonArray msgs = offline_.value(userId);
    if (msgs.isEmpty()) return;
    WsPeer* target = online_.value(userId);
    if (!target) return;
    for (const auto& v : std::as_const(msgs))
        target->sendText(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    offline_.remove(userId);
}

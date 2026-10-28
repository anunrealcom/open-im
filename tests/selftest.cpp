// 无头自测：启动内置服务端，用两个最小 WebSocket 客户端验证核心链路。
// 覆盖：hello/welcome、在线列表、文本消息、送达确认、输入状态、机器人回显、二进制文件中继。
#include <QCoreApplication>
#include <QTcpSocket>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "net/wsserver.h"
#include "core/server.h"

class TestClient : public QObject {
    Q_OBJECT
public:
    WsPeer* peer = nullptr;
    QTcpSocket* sock;
    QByteArray buf;
    bool handshaked = false;
    QList<QJsonObject> received;
    QByteArray lastBinary;

    explicit TestClient(QObject* p = nullptr) : QObject(p) {
        sock = new QTcpSocket(this);
        connect(sock, &QTcpSocket::connected, this, [this]() {
            sock->write(ws::clientHandshake("127.0.0.1", port_));
        });
        connect(sock, &QTcpSocket::readyRead, this, &TestClient::onReady);
    }

    quint16 port_ = 0;
    void connectTo(quint16 port) {
        port_ = port;
        sock->connectToHost("127.0.0.1", port);
    }

    void onReady() {
        if (handshaked) return;
        buf += sock->readAll();
        int idx = buf.indexOf("\r\n\r\n");
        if (idx < 0) return;
        QByteArray resp = buf.left(idx);
        QString accept = ws::parseAcceptFromResponse(resp);
        buf = buf.mid(idx + 4);
        Q_UNUSED(accept);
        handshaked = true;
        peer = new WsPeer(sock, this);
        peer->setMaskOutgoing(true);
        connect(peer, &WsPeer::textMessage, this, [this](const QString& t) {
            QJsonDocument d = QJsonDocument::fromJson(t.toUtf8());
            if (d.isObject()) received.append(d.object());
        });
        connect(peer, &WsPeer::binaryMessage, this, [this](const QByteArray& d) {
            lastBinary = d;
        });
        if (!buf.isEmpty()) peer->feed(buf);
    }

    void send(const QJsonObject& m) {
        peer->sendText(QJsonDocument(m).toJson(QJsonDocument::Compact));
    }
};

    int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    qInfo() << "自测启动";

    Server server;
    if (!server.start()) {
        qCritical() << "服务端启动失败";
        return 1;
    }
    quint16 port = server.port();
    qInfo() << "服务端端口:" << port;

    TestClient a, b;
    QEventLoop loop;
    QTimer::singleShot(8000, &loop, [&loop]() { loop.exit(99); });

    bool pass = true;
    auto check = [&](bool cond, const QString& name) {
        qInfo() << (cond ? "[PASS]" : "[FAIL]") << name;
        if (!cond) pass = false;
    };

    a.connectTo(port);
    QTimer::singleShot(200, [&]() { b.connectTo(port); });

    // 等两个都连上并完成 hello
    QTimer::singleShot(500, [&]() {
        a.send(QJsonObject{{"type", "hello"}, {"userId", "alice"}, {"name", "Alice"}});
        b.send(QJsonObject{{"type", "hello"}, {"userId", "bob"}, {"name", "Bob"}});
    });

    // 500ms 后：文本消息 + 输入状态 + 机器人回显 + 文件二进制
    QTimer::singleShot(900, [&]() {
        if (!a.peer || !b.peer) { qWarning() << "peer 未就绪，跳过发送"; return; }
        a.send(QJsonObject{{"type", "chat"}, {"id", "m1"}, {"to", "bob"},
                           {"contentType", "text"}, {"text", "你好，这是一幻IM"}});
        a.send(QJsonObject{{"type", "typing"}, {"to", "bob"}, {"isTyping", true}});
        a.send(QJsonObject{{"type", "chat"}, {"id", "m2"}, {"to", "xiaohuan"},
                           {"contentType", "text"}, {"text", "echo test"}});
        // 文件：先发元数据，再发二进制帧（头 JSON + 内容）
        QJsonObject fmeta;
        fmeta["type"] = "chat"; fmeta["id"] = "m3"; fmeta["to"] = "bob";
        fmeta["contentType"] = "file"; fmeta["fileId"] = "f1";
        fmeta["fileName"] = "note.txt"; fmeta["fileSize"] = 11;
        a.send(fmeta);
        QByteArray header = "{\"to\":\"bob\",\"fileId\":\"f1\"}\n";
        QByteArray content = "hello world";
        a.peer->sendBinary(header + content);
    });

    // 收尾判定
    QTimer::singleShot(1600, [&]() {
        // presence：每个客户端都应收到含 2 人在线的 presence
        auto hasUsers = [&](const TestClient& c, int n) {
            for (const auto& m : c.received)
                if (m["type"] == "presence" && m["users"].toArray().size() == n) return true;
            return false;
        };
        check(hasUsers(a, 2), "A 收到 2 人在线 presence");
        check(hasUsers(b, 2), "B 收到 2 人在线 presence");

        // B 收到 A 的文本
        bool bGotText = false;
        for (const auto& m : b.received)
            if (m["type"] == "chat" && m["text"] == "你好，这是一幻IM" && m["from"] == "alice")
                bGotText = true;
        check(bGotText, "B 收到 A 的文本消息");

        // A 收到送达确认
        bool aAck = false;
        for (const auto& m : a.received)
            if (m["type"] == "chat-ack" && m["id"] == "m1") aAck = true;
        check(aAck, "A 收到 chat-ack");

        // B 收到输入状态
        bool bTyping = false;
        for (const auto& m : b.received)
            if (m["type"] == "typing" && m["isTyping"].toBool() && m["from"] == "alice") bTyping = true;
        check(bTyping, "B 收到 typing 状态");

        // A 收到机器人（小幻）回显
        bool aBot = false;
        for (const auto& m : a.received)
            if (m["type"] == "bot-message" && m["from"] == "xiaohuan" && m["text"] == "echo test")
                aBot = true;
        check(aBot, "A 收到机器人回显");

        // B 收到二进制文件帧
        bool bFile = false;
        if (b.lastBinary.startsWith("{\"to\":\"bob\",\"fileId\":\"f1\"}\n")) {
            QByteArray body = b.lastBinary.mid(b.lastBinary.indexOf('\n') + 1);
            if (body == "hello world") bFile = true;
        }
        check(bFile, "B 收到文件二进制帧");

        loop.exit(pass ? 0 : 1);
    });

    int rc = loop.exec();
    if (rc == 99) { qCritical() << "自测超时"; return 2; }
    qInfo() << (rc == 0 ? "全部自测通过" : "存在失败用例");
    return rc;
}

#include "selftest.moc"

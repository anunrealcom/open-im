#pragma once
#include <QWidget>
#include <QMap>
#include <QVector>
#include <QJsonObject>
#include <QString>

class Server;
class Connection;
class Sidebar;
class ChatWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

struct Conversation {
    QString title;
    bool isAi = false;
    QString peerId;                // 目标 id（userId / botId / "ai"）
    QVector<QJsonObject> messages;
    bool loaded = false;
};

// 局域网发现的对方实例（key 为 "host:port"）
struct PeerInfo {
    QString host;
    quint16 port = 0;
    QString name;
    QString userId;
    bool online = true;
};

// 无边框主窗口：自定义标题栏 + 三段式（导航栏 / 会话列表 / 聊天区），QQ 风格。
class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(Server* server,
                        const QString& name,
                        const QString& userId,
                        const QString& host,
                        quint16 port,
                        QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void onMessage(const QJsonObject& m);
    void onBinary(const QByteArray& data);
    void onConnected();
    void onDisconnected();
    void onSendText(const QString& text);
    void onSendFile(const QString& path);
    void onSendTick();                 // 分块发送驱动器
    void onFileSaveAs(const QString& fileId, const QString& fileName);
    void onSelect(const QString& id);
    void onFileClicked(const QString& fileId);
    void openSettings();
    void reconnect();
    void onToggleTheme();
    void onMinimize();
    void onToggleMax();
    void onClose();
    void onLanPeer(const QString& host, quint16 port, const QString& name,
                   const QString& userId);

private:
    void applyTheme(bool dark);
    void buildSidebar();
    Conversation& ensureConv(const QString& id, const QString& title, bool isAi);
    void loadHistory(Conversation& c, const QString& convId);
    QString convIdFor(const QString& id, bool isAi) const;
    void sendChat(const QString& to, const QString& text,
                  const QString& contentType = "text",
                  const QString& fileId = QString(),
                  const QString& fileName = QString(),
                  qint64 fileSize = 0,
                  const QString& mime = QString());
    void handleIncoming(const QJsonObject& m);
    void updateMask();

    // 局域网自动发现：为每个发现的 peer 建立独立连接并路由消息
    void onPeerConnected(Connection* c);
    void onPeerMessage(Connection* c, const QJsonObject& m);
    void onPeerDisconnected(Connection* c);
    Connection* targetFor(const QString& id, QString& wireTo);

    Server* server_;
    Connection* conn_;
    QString myId_;
    QString myName_;
    QString presetUserId_;
    QString serverHost_;
    quint16 serverPort_;

    QMap<QString, Conversation> convs_;
    QMap<QString, QString> onlineUsers_;   // userId -> name
    QMap<QString, int> unread_;            // 会话未读计数

    // 局域网自动发现
    QMap<QString, PeerInfo> peers_;            // key = "host:port"
    QMap<QString, Connection*> peerConns_;      // key = "host:port"
    QMap<QString, QString> userToPeer_;         // 对方 userId -> "host:port"
    QMap<QString, QString> filePaths_;      // fileId -> 已保存路径

    // 分块文件传输状态
    struct SendXfer {
        QString fid;
        class QFile* file = nullptr;
        qint64 total = 0;
        qint64 offset = 0;
        QString wireTo;
        class Connection* target = nullptr;
        QString path;
        QString name;
        bool isImage = false;
        bool first = true;
    };
    struct RecvXfer {
        QString name;
        QString mime;
        qint64 total = 0;
        qint64 got = 0;
        bool started = false;
        QByteArray buf;
    };
    QMap<QString, SendXfer> sendXfers_;
    QMap<QString, RecvXfer> recvXfers_;
    class QTimer* sendTimer_ = nullptr;

    // 无边框框架
    QWidget* contentFrame_ = nullptr;
    QWidget* titleBar_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QPushButton* minBtn_ = nullptr;
    QPushButton* maxBtn_ = nullptr;
    QPushButton* closeBtn_ = nullptr;

    // 三段式
    QWidget* leftRail_ = nullptr;
    QWidget* sidebarPanel_ = nullptr;
    QWidget* chatHeader_ = nullptr;
    QLabel* chatTitleLabel_ = nullptr;
    QLabel* chatAvatar_ = nullptr;
    QPushButton* settingsBtn_ = nullptr;
    QPushButton* reconnectBtn_ = nullptr;
    QPushButton* themeBtn_ = nullptr;

    Sidebar* sidebar_ = nullptr;
    ChatWidget* chat_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QString currentId_;
    int radius_ = 12;
    bool dark_ = false;
};

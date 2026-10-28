#include "mainwindow.h"
#include "sidebar.h"
#include "chatwidget.h"
#include "settingsdialog.h"
#include "connection.h"
#include "core/server.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QStandardPaths>
#include <QFileDialog>
#include <cstring>
#include <QDesktopServices>
#include <QUrl>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QApplication>
#include <QBitmap>
#include <QPainterPath>

static bool isImageFile(const QString& fileName) {
    static const QStringList ext = {"png","jpg","jpeg","gif","bmp","webp","svg"};
    return ext.contains(QFileInfo(fileName).suffix().toLower());
}
static QString guessMime(const QString& fileName) {
    QString s = QFileInfo(fileName).suffix().toLower();
    if (s == "png") return "image/png";
    if (s == "jpg" || s == "jpeg") return "image/jpeg";
    if (s == "gif") return "image/gif";
    if (s == "bmp") return "image/bmp";
    if (s == "webp") return "image/webp";
    if (s == "svg") return "image/svg+xml";
    return "application/octet-stream";
}
#include <QWindow>

// 标题栏：捕获鼠标拖动用于移动窗口（排除按钮区域）。
class TitleBar : public QWidget {
public:
    explicit TitleBar(QWidget* p = nullptr) : QWidget(p) {}
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton)
            if (auto* w = window()->windowHandle()) w->startSystemMove();
        QWidget::mousePressEvent(e);
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override {
        QWidget* w = window();
        w->setWindowState(w->windowState() ^ Qt::WindowMaximized);
        QWidget::mouseDoubleClickEvent(e);
    }
};

// 边缘 grip：用于无边框窗口的缩放。
class EdgeGrip : public QWidget {
public:
    explicit EdgeGrip(Qt::Edges edges, QWidget* p = nullptr)
        : QWidget(p), edges_(edges) { setCursor(cursorFor(edges)); }
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton)
            if (auto* w = window()->windowHandle()) w->startSystemResize(edges_);
        QWidget::mousePressEvent(e);
    }
private:
    Qt::Edges edges_;
    static Qt::CursorShape cursorFor(Qt::Edges e) {
        if (e == Qt::LeftEdge || e == Qt::RightEdge) return Qt::SizeHorCursor;
        if (e == Qt::TopEdge || e == Qt::BottomEdge) return Qt::SizeVerCursor;
        if (e == (Qt::TopEdge | Qt::LeftEdge) || e == (Qt::BottomEdge | Qt::RightEdge))
            return Qt::SizeFDiagCursor;
        if (e == (Qt::TopEdge | Qt::RightEdge) || e == (Qt::BottomEdge | Qt::LeftEdge))
            return Qt::SizeBDiagCursor;
        return Qt::ArrowCursor;
    }
};

MainWindow::MainWindow(Server* server, const QString& name, const QString& userId,
                       const QString& host, quint16 port, QWidget* parent)
    : QWidget(parent), server_(server), myName_(name), presetUserId_(userId),
      serverHost_(host.isEmpty() ? "127.0.0.1" : host), serverPort_(port ? port : server->port()) {

    conn_ = new Connection(this);
    connect(conn_, &Connection::message, this, &MainWindow::onMessage);
    connect(conn_, &Connection::binary, this, &MainWindow::onBinary);
    connect(conn_, &Connection::connected, this, &MainWindow::onConnected);
    connect(conn_, &Connection::disconnected, this, &MainWindow::onDisconnected);

    connect(server_, &Server::lanPeer, this, &MainWindow::onLanPeer);

    // ---- 无边框 + 透明 + 阴影空间 ----
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(720, 520);

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(14, 14, 14, 14);
    grid->setSpacing(0);
    grid->addWidget(new EdgeGrip(Qt::TopEdge | Qt::LeftEdge, this), 0, 0);
    grid->addWidget(new EdgeGrip(Qt::TopEdge, this), 0, 1);
    grid->addWidget(new EdgeGrip(Qt::TopEdge | Qt::RightEdge, this), 0, 2);
    grid->addWidget(new EdgeGrip(Qt::LeftEdge, this), 1, 0);
    contentFrame_ = new QWidget(this);
    grid->addWidget(contentFrame_, 1, 1);
    grid->addWidget(new EdgeGrip(Qt::RightEdge, this), 1, 2);
    grid->addWidget(new EdgeGrip(Qt::BottomEdge | Qt::LeftEdge, this), 2, 0);
    grid->addWidget(new EdgeGrip(Qt::BottomEdge, this), 2, 1);
    grid->addWidget(new EdgeGrip(Qt::BottomEdge | Qt::RightEdge, this), 2, 2);

    contentFrame_->setObjectName("contentFrame");
    auto* cv = new QVBoxLayout(contentFrame_);
    cv->setContentsMargins(0, 0, 0, 0);
    cv->setSpacing(0);

    // ---- 标题栏 ----
    titleBar_ = new TitleBar(contentFrame_);
    titleBar_->setObjectName("titleBar");
    titleBar_->setFixedHeight(42);
    auto* th = new QHBoxLayout(titleBar_);
    th->setContentsMargins(14, 0, 10, 0);
    th->setSpacing(6);
    titleLabel_ = new QLabel("一幻 IM", titleBar_);
    titleLabel_->setObjectName("titleLabel");
    titleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    th->addWidget(titleLabel_);
    th->addStretch(1);
    minBtn_ = new QPushButton("—", titleBar_);   // –
    maxBtn_ = new QPushButton("□", titleBar_);   // □
    closeBtn_ = new QPushButton("✕", titleBar_); // ✕
    minBtn_->setObjectName("minBtn");
    maxBtn_->setObjectName("maxBtn");
    closeBtn_->setObjectName("closeBtn");
    th->addWidget(minBtn_);
    th->addWidget(maxBtn_);
    th->addWidget(closeBtn_);
    connect(minBtn_, &QPushButton::clicked, this, &MainWindow::onMinimize);
    connect(maxBtn_, &QPushButton::clicked, this, &MainWindow::onToggleMax);
    connect(closeBtn_, &QPushButton::clicked, this, &MainWindow::onClose);

    // ---- 三段式 body ----
    auto* body = new QWidget(contentFrame_);
    auto* bh = new QHBoxLayout(body);
    bh->setContentsMargins(0, 0, 0, 0);
    bh->setSpacing(0);

    // 最左导航栏（深色）
    leftRail_ = new QWidget(body);
    leftRail_->setObjectName("leftRail");
    leftRail_->setFixedWidth(64);
    auto* lv = new QVBoxLayout(leftRail_);
    lv->setContentsMargins(0, 12, 0, 12);
    lv->setSpacing(10);
    auto* avatar = new QPushButton(myName_.isEmpty() ? "我" : myName_.left(1), leftRail_);
    avatar->setObjectName("railAvatar");
    lv->addWidget(avatar, 0, Qt::AlignHCenter);
    auto* msgBtn = new QPushButton("💬", leftRail_);
    msgBtn->setObjectName("railBtn");
    msgBtn->setToolTip("消息");
    lv->addWidget(msgBtn, 0, Qt::AlignHCenter);
    lv->addStretch(1);
    themeBtn_ = new QPushButton("🌙", leftRail_);
    themeBtn_->setObjectName("railTheme");
    themeBtn_->setToolTip("切换深浅色");
    lv->addWidget(themeBtn_, 0, Qt::AlignHCenter);
    connect(themeBtn_, &QPushButton::clicked, this, &MainWindow::onToggleTheme);
    settingsBtn_ = new QPushButton("⚙", leftRail_);
    settingsBtn_->setObjectName("railSettings");
    settingsBtn_->setToolTip("设置");
    lv->addWidget(settingsBtn_, 0, Qt::AlignHCenter);
    connect(settingsBtn_, &QPushButton::clicked, this, &MainWindow::openSettings);

    // 会话列表区
    sidebarPanel_ = new QWidget(body);
    sidebarPanel_->setObjectName("sidebarPanel");
    sidebarPanel_->setFixedWidth(250);
    auto* sv = new QVBoxLayout(sidebarPanel_);
    sv->setContentsMargins(0, 0, 0, 0);
    sv->setSpacing(0);
    auto* search = new QLineEdit(sidebarPanel_);
    search->setObjectName("searchEdit");
    search->setPlaceholderText("搜索");
    sv->addWidget(search);
    connect(search, &QLineEdit::textChanged, sidebar_, &Sidebar::filter);
    sv->addSpacing(6);
    sidebar_ = new Sidebar(sidebarPanel_);
    sidebar_->setObjectName("sidebar");
    connect(sidebar_, &Sidebar::selected, this, &MainWindow::onSelect);
    sv->addWidget(sidebar_, 1);

    // 右侧聊天区
    auto* rightArea = new QWidget(body);
    rightArea->setObjectName("rightArea");
    auto* rv = new QVBoxLayout(rightArea);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);
    chatHeader_ = new QWidget(rightArea);
    chatHeader_->setObjectName("chatHeader");
    chatHeader_->setFixedHeight(58);
    auto* ch = new QHBoxLayout(chatHeader_);
    ch->setContentsMargins(18, 0, 14, 0);
    ch->setSpacing(10);
    chatAvatar_ = new QLabel(rightArea);
    chatAvatar_->setObjectName("chatAvatar");
    ch->addWidget(chatAvatar_);
    chatTitleLabel_ = new QLabel("AI 对话", chatHeader_);
    chatTitleLabel_->setObjectName("chatTitleLabel");
    ch->addWidget(chatTitleLabel_, 1);
    statusLabel_ = new QLabel("未连接", chatHeader_);
    statusLabel_->setObjectName("statusLabel");
    ch->addWidget(statusLabel_);
    reconnectBtn_ = new QPushButton("↻ 重连", chatHeader_);
    reconnectBtn_->setObjectName("reconnectBtn");
    ch->addWidget(reconnectBtn_);
    connect(reconnectBtn_, &QPushButton::clicked, this, &MainWindow::reconnect);
    rv->addWidget(chatHeader_);
    chat_ = new ChatWidget(rightArea);
    rv->addWidget(chat_, 1);
    connect(chat_, &ChatWidget::sendText, this, &MainWindow::onSendText);
    connect(chat_, &ChatWidget::sendFile, this, &MainWindow::onSendFile);
    connect(chat_, &ChatWidget::fileClicked, this, &MainWindow::onFileClicked);
    connect(chat_, &ChatWidget::fileSaveAs, this, &MainWindow::onFileSaveAs);

    bh->addWidget(leftRail_);
    bh->addWidget(sidebarPanel_);
    bh->addWidget(rightArea, 1);

    cv->addWidget(titleBar_);
    cv->addWidget(body, 1);

    resize(960, 680);
    applyTheme(dark_);
    buildSidebar();
    updateMask();
    conn_->connectTo(serverHost_, serverPort_);
}

void MainWindow::applyTheme(bool dark) {
    dark_ = dark;
    QFont f("Microsoft YaHei UI");
    if (f.family().isEmpty()) f.setFamily("PingFang SC");
    f.setPixelSize(13);
    qApp->setFont(f);

    // 气泡 / AI 流式气泡配色，供 MessageBubble、ChatWidget 读取
    qApp->setProperty("bubbleOut", dark ? "#3E8E5A" : "#95EC69");
    qApp->setProperty("bubbleIn", dark ? "#3A3A3A" : "#FFFFFF");
    qApp->setProperty("bubbleOutBorder", dark ? "#347A4D" : "#8FD94E");
    qApp->setProperty("bubbleInBorder", dark ? "#4A4A4A" : "#EBEBEB");
    qApp->setProperty("bubbleText", dark ? "#E6E6E6" : "#1f1f1f");
    qApp->setProperty("streamBg", dark ? "#3A3A3A" : "#FFFFFF");
    qApp->setProperty("streamBorder", dark ? "#4A4A4A" : "#EBEBEB");

    // 主题色板
    QString cFrame   = dark ? "#1E1E1E" : "#FFFFFF";
    QString cText    = dark ? "#E6E6E6" : "#1f1f1f";
    QString cTitle   = dark ? "#262626" : "#F7F8FA";
    QString cRail    = dark ? "#151515" : "#2E2E2E";
    QString cRailIco = dark ? "#B6B6B6" : "#9a9a9a";
    QString cRailHov = dark ? "#333333" : "#3A3A3A";
    QString cRailBg  = dark ? "#2DA7E0" : "#12B7F5";
    QString cSide    = dark ? "#242424" : "#FFFFFF";
    QString cSideBd  = dark ? "#333333" : "#ECECEC";
    QString cSearch  = dark ? "#2A2A2A" : "#F2F3F5";
    QString cSearchTx= dark ? "#E6E6E6" : "#333333";
    QString cHeader  = dark ? "#262626" : "#F7F8FA";
    QString cHeaderBd= dark ? "#333333" : "#ECECEC";
    QString cAvatar  = dark ? "#2DA7E0" : "#12B7F5";
    QString cAccent  = dark ? "#2DA7E0" : "#12B7F5";
    QString cAccentH = dark ? "#1E2A30" : "#E6F6FF";

    QString qss = QString(R"(
        QWidget { font-family: "Microsoft YaHei UI", "PingFang SC", sans-serif; color: %1; }
        #contentFrame { background: %2; }
        #titleBar { background: %3; border-top-left-radius: %9px; border-top-right-radius: %9px; }
        #titleLabel { color: %1; font-size: 13px; font-weight: 600; }
        #minBtn, #maxBtn, #closeBtn { background: transparent; border: none; width: 30px; height: 26px;
                                       border-radius: 6px; font-size: 13px; color: #777; }
        #minBtn:hover, #maxBtn:hover { background: #E6E6E6; }
        #closeBtn:hover { background: #F5483B; color: #fff; }
        #leftRail { background: %4; }
        #railAvatar { background: %10; color: #fff; border-radius: 22px; font-size: 16px; font-weight: 700;
                      min-width: 44px; max-width: 44px; min-height: 44px; max-height: 44px; }
        #railBtn { background: transparent; border: none; color: %5; font-size: 19px;
                   border-radius: 10px; min-height: 44px; }
        #railBtn:hover { background: %6; color: #fff; }
        #railTheme, #railSettings { background: transparent; border: none; color: %5; font-size: 19px;
                        border-radius: 10px; min-height: 44px; }
        #railTheme:hover, #railSettings:hover { background: %6; color: #fff; }
        #sidebarPanel { background: %7; border-right: 1px solid %8; }
        #searchEdit { background: %11; border: none; border-radius: 16px; padding: 7px 14px;
                      font-size: 12px; color: %12; margin: 10px 12px 0 12px; }
        #chatHeader { background: %13; border-bottom: 1px solid %14; }
        #chatTitleLabel { color: %1; font-size: 15px; font-weight: 600; }
        #statusLabel { color: #999; font-size: 11px; }
        #chatAvatar { background: %15; color: #fff; border-radius: 18px; font-size: 13px; font-weight: 700;
                      min-width: 36px; max-width: 36px; min-height: 36px; max-height: 36px; }
        #reconnectBtn { background: transparent; border: none; color: %16; font-size: 13px;
                        padding: 5px 10px; border-radius: 6px; }
        #reconnectBtn:hover { background: %17; }
    )")
        .arg(cText, cFrame, cTitle, cRail, cRailIco, cRailHov, cSide, cSideBd,
             QString::number(radius_), cRailBg, cSearch, cSearchTx, cHeader, cHeaderBd,
             cAvatar, cAccent, cAccentH);
    setStyleSheet(qss);

    titleBar_->setStyleSheet(QString("#titleBar{background:%1;border-top-left-radius:%2px;border-top-right-radius:%2px;}")
                             .arg(cTitle).arg(radius_));

    sidebar_->applyTheme(dark);
    chat_->applyTheme(dark);
}

void MainWindow::onToggleTheme() {
    applyTheme(!dark_);
    themeBtn_->setText(dark_ ? "☀" : "🌙");
}

void MainWindow::updateMask() {
    if (!contentFrame_) return;
    QRect r = contentFrame_->rect();
    QBitmap bmp(r.size());
    bmp.clear();
    {
        QPainter p(&bmp);
        p.setPen(Qt::color1);
        p.setBrush(Qt::color1);
        p.drawRoundedRect(r, radius_, radius_);
    }
    contentFrame_->setMask(bmp);
}

void MainWindow::paintEvent(QPaintEvent* e) {
    Q_UNUSED(e);
    if (isMaximized()) return;
    if (!contentFrame_) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRect r = contentFrame_->geometry();
    const int layers = 12;
    for (int i = layers; i >= 1; --i) {
        qreal spread = i * 1.1;
        int alpha = qRound(30 * (1.0 - (qreal)i / (layers + 1)));
        QRectF sr = QRectF(r).adjusted(-spread, -spread + 1, spread, spread + 3);
        p.setBrush(QColor(0, 0, 0, alpha));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(sr, radius_ + spread * 0.4, radius_ + spread * 0.4);
    }
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    QString bg = dark_ ? "#262626" : "#F7F8FA";
    if (isMaximized()) {
        contentFrame_->clearMask();
        titleBar_->setStyleSheet(QString("#titleBar{background:%1;border-top-left-radius:0px;border-top-right-radius:0px;}")
                                 .arg(bg));
    } else {
        titleBar_->setStyleSheet(QString("#titleBar{background:%1;border-top-left-radius:%2px;border-top-right-radius:%2px;}")
                                 .arg(bg).arg(radius_));
        updateMask();
    }
}

void MainWindow::onMinimize() { showMinimized(); }
void MainWindow::onToggleMax() {
    if (isMaximized()) showNormal();
    else showMaximized();
}
void MainWindow::onClose() { close(); }

void MainWindow::onConnected() {
    QJsonObject hello;
    hello["type"] = "hello";
    hello["name"] = myName_;
    if (!presetUserId_.isEmpty()) hello["userId"] = presetUserId_;
    conn_->sendJson(hello);
    statusLabel_->setText("已连接，等待欢迎消息…");
}

void MainWindow::onDisconnected() {
    statusLabel_->setText("连接已断开");
}

// ---- 局域网自动发现 ----
void MainWindow::onLanPeer(const QString& host, quint16 port, const QString& name,
                           const QString& userId) {
    QString key = host + ":" + QString::number(port);
    // 发现层每收到一次广播都会上报（≤3s），这里做幂等：
    // 只有"新 peer"或"曾掉线又恢复"才记日志、重建侧边栏；否则只确保连接存在。
    const bool isNew = !peers_.contains(key);
    const bool wasOffline = !isNew && !peers_[key].online;
    // 名字/userId 变了也要刷新（对端登录后 userId 从空变有，第一次没去重掉线上条目）
    const bool infoChanged = !isNew && (peers_[key].name != name || peers_[key].userId != userId);

    PeerInfo p;
    p.host = host; p.port = port; p.name = name; p.userId = userId; p.online = true;
    peers_[key] = p;
    if (isNew || wasOffline || infoChanged)
        qInfo() << "[LAN] peer available" << key << name << userId;
    if (!userId.isEmpty()) userToPeer_[userId] = key;

    if (!peerConns_.contains(key)) {
        auto* c = new Connection(this);
        connect(c, &Connection::message, this, [this, c](const QJsonObject& m) {
            onPeerMessage(c, m);
        });
        connect(c, &Connection::binary, this, &MainWindow::onBinary);
        connect(c, &Connection::connected, this, [this, c]() { onPeerConnected(c); });
        connect(c, &Connection::disconnected, this, [this, c]() { onPeerDisconnected(c); });
        c->connectTo(host, port);
        peerConns_[key] = c;
    }
    if (isNew || wasOffline || infoChanged) buildSidebar();
}

void MainWindow::onPeerConnected(Connection* c) {
    QJsonObject hello;
    hello["type"] = "hello";
    hello["name"] = myName_;
    // 必须用 myId_（本地服务端 welcome 分配的真实 ID），不能用 presetUserId_（新流程下永远是空）。
    // 否则对端 server 收到 hello 没 userId，会随机生成一个，和我们 LAN announce 里的 userId 对不上，
    // 消息按 userId 路由时查不到对方，丢消息。
    hello["userId"] = myId_;
    c->sendJson(hello);
}

void MainWindow::onPeerMessage(Connection* c, const QJsonObject& m) {
    QString type = m["type"].toString();
    if (type == "welcome") {
        // peer 连接握手成功，用对端真实昵称/ID 刷新侧边栏（首次广播时可能还是默认名）
        QString key;
        for (auto it = peerConns_.begin(); it != peerConns_.end(); ++it)
            if (it.value() == c) { key = it.key(); break; }
        if (!key.isEmpty() && peers_.contains(key)) {
            QJsonObject self = m["self"].toObject();
            peers_[key].name = self["name"].toString();
            peers_[key].userId = self["userId"].toString();
            if (!peers_[key].userId.isEmpty())
                userToPeer_[peers_[key].userId] = key;
            buildSidebar();
        }
        return;
    }
    if (type == "chat" || type == "bot-message") {
        handleIncoming(m);
    } else if (type == "typing") {
        QString fromCid = userToPeer_.value(m["from"].toString(), m["from"].toString());
        if (fromCid == currentId_) chat_->showTyping(m["isTyping"].toBool());
    }
}

void MainWindow::onPeerDisconnected(Connection* c) {
    QString key;
    for (auto it = peerConns_.begin(); it != peerConns_.end(); ++it) {
        if (it.value() == c) { key = it.key(); break; }
    }
    if (key.isEmpty()) return;
    peerConns_.remove(key);
    if (peers_.contains(key)) peers_[key].online = false;
    buildSidebar();
}

void MainWindow::onMessage(const QJsonObject& m) {
    QString type = m["type"].toString();
    if (type == "welcome") {
        myId_ = m["self"].toObject()["userId"].toString();
        statusLabel_->setText("已登录：" + myName_ + " (" + myId_ + ")");
        server_->setLocalIdentity(myId_, myName_);
        buildSidebar();
        onSelect("ai");
    } else if (type == "presence") {
        onlineUsers_.clear();
        for (const auto& v : m["users"].toArray()) {
            QJsonObject u = v.toObject();
            QString uid = u["userId"].toString();
            if (uid != myId_) onlineUsers_[uid] = u["name"].toString();
        }
        buildSidebar();
    } else if (type == "chat") {
        handleIncoming(m);
    } else if (type == "bot-message") {
        handleIncoming(m);
    } else if (type == "typing") {
        if (m["from"].toString() == currentId_)
            chat_->showTyping(m["isTyping"].toBool());
    } else if (type == "ai-token") {
        if (currentId_ == "ai") chat_->appendAssistantDelta(m["delta"].toString());
    } else if (type == "ai-end") {
        chat_->finalizeAssistant();
    } else if (type == "ai-error") {
        if (currentId_ == "ai") {
            chat_->appendAssistantDelta("⚠️ " + m["message"].toString());
            chat_->finalizeAssistant();
        }
    }
}

void MainWindow::handleIncoming(const QJsonObject& m) {
    QString from = m["from"].toString();
    QString cid = userToPeer_.value(from, from);   // 对方 userId -> 局域网 peerKey
    QString title = m["fromName"].toString();
    if (title.isEmpty()) {
        QJsonObject bot = server_->config()->findBot(from);
        if (!bot.isEmpty()) title = bot["name"].toString();
        else if (onlineUsers_.contains(from)) title = onlineUsers_[from];
        else if (peers_.contains(cid)) title = peers_[cid].name;
        else title = from;
    }
    bool isCurrent = (currentId_ == cid);
    Conversation& c = ensureConv(cid, title, false);
    c.messages.append(m);
    if (isCurrent) chat_->appendMessage(m, false);
    else {
        unread_[cid]++;
        sidebar_->setUnread(cid, unread_[cid]);
    }
    // 文件/图片的实际落盘在 onBinary 分块接收完成后处理（见 onBinary）
}

void MainWindow::onBinary(const QByteArray& data) {
    int nl = data.indexOf('\n');
    if (nl < 0) return;
    QJsonObject hdr = QJsonDocument::fromJson(data.left(nl)).object();
    QString fid = hdr["fileId"].toString();
    if (fid.isEmpty()) return;
    QByteArray payload = data.mid(nl + 1);

    RecvXfer& r = recvXfers_[fid];
    if (!r.started) {                       // 首块：记录元信息
        r.started = true;
        r.name = hdr["name"].toString();
        r.total = hdr["total"].toInteger();
        r.mime = hdr["mime"].toString();
    }
    qint64 off = hdr["offset"].toInteger();
    if (r.buf.size() < off + payload.size()) r.buf.resize(off + payload.size());
    memcpy(r.buf.data() + off, payload.constData(), payload.size());
    r.got = off + payload.size();

    int pct = r.total > 0 ? int(100 * r.got / r.total) : 100;
    chat_->setFileProgress(fid, pct);

    if (r.total > 0 && r.got >= r.total) {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString saveName = fid + "_" + r.name;
        QString path = dir + "/" + saveName;
        QFile f(path);
        bool ok = false;
        if (f.open(QIODevice::WriteOnly)) { f.write(r.buf); f.close(); ok = true; }
        if (ok) filePaths_[fid] = path;
        chat_->setFileDone(fid, ok, path);
        recvXfers_.remove(fid);
    }
}

void MainWindow::onSendText(const QString& text) {
    QString id = currentId_;
    if (id.isEmpty()) return;
    if (id == "ai") {
        Conversation& c = ensureConv("ai", "AI 对话", true);
        QJsonObject um;
        um["from"] = myId_; um["fromName"] = myName_;
        um["contentType"] = "text"; um["text"] = text;
        um["role"] = "user";
        c.messages.append(um);
        chat_->appendMessage(um, true);

        QString pid = server_->config()->aiProviders().first().toObject()["id"].toString();
        conn_->sendJson(QJsonObject{
            {"type", "ai-request"},
            {"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"to", pid},
            {"text", text},
            {"conversationId", "ai:" + myId_}});
    } else {
        sendChat(id, text);
    }
}

void MainWindow::onSendFile(const QString& path) {
    if (currentId_.isEmpty()) return;
    auto* file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法读取文件");
        delete file; return;
    }
    qint64 total = file->size();
    QString fid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString fname = QFileInfo(path).fileName();
    bool isImg = isImageFile(fname);
    QString mime = isImg ? guessMime(fname) : "application/octet-stream";

    QString wireTo;
    Connection* target = targetFor(currentId_, wireTo);
    sendChat(currentId_, "", isImg ? "image" : "file", fid, fname, total, mime);

    // 自己发的文件：原始本地路径直接可打开
    filePaths_[fid] = path;

    SendXfer x;
    x.fid = fid; x.file = file; x.total = total; x.offset = 0;
    x.wireTo = wireTo; x.target = target; x.path = path;
    x.name = fname; x.isImage = isImg; x.first = true;
    sendXfers_[fid] = x;

    if (!sendTimer_) {
        sendTimer_ = new QTimer(this);
        connect(sendTimer_, &QTimer::timeout, this, &MainWindow::onSendTick);
    }
    if (!sendTimer_->isActive()) sendTimer_->start(15);
}

void MainWindow::onSendTick() {
    if (sendXfers_.isEmpty()) { sendTimer_->stop(); return; }
    const int kChunk = 64 * 1024;
    for (auto it = sendXfers_.begin(); it != sendXfers_.end(); ) {
        SendXfer& x = it.value();
        if (!x.file) { it = sendXfers_.erase(it); continue; }
        QByteArray payload = x.file->read(kChunk);
        qint64 len = payload.size();
        if (len <= 0) {                       // 全部发完
            x.file->close(); delete x.file; x.file = nullptr;
            chat_->setFileDone(x.fid, true, x.path);
            it = sendXfers_.erase(it);
            continue;
        }
        QJsonObject hdr;
        hdr["fileId"] = x.fid;
        hdr["to"] = x.wireTo;
        hdr["offset"] = x.offset;
        if (x.first) {
            hdr["name"] = x.name;
            hdr["total"] = x.total;
            hdr["mime"] = x.isImage ? guessMime(x.name) : "application/octet-stream";
            x.first = false;
        }
        QByteArray frame = QJsonDocument(hdr).toJson(QJsonDocument::Compact)
                           + QByteArray("\n") + payload;
        x.target->sendBinary(frame);
        x.offset += len;
        int pct = x.total > 0 ? int(100 * x.offset / x.total) : 100;
        chat_->setFileProgress(x.fid, pct);
        ++it;
    }
    if (sendXfers_.isEmpty()) sendTimer_->stop();
}

// 根据目标 id 选择发送连接：局域网 peer 走其专用连接，其余走本机主连接。
// wireTo 返回实际写入报文的 to 字段（peerKey 需替换为对方 userId）。
Connection* MainWindow::targetFor(const QString& id, QString& wireTo) {
    wireTo = id;
    if (peers_.contains(id)) {                       // 来自发现列表的 peerKey
        wireTo = peers_[id].userId;
        return peerConns_.value(id, conn_);
    }
    if (userToPeer_.contains(id)) {                  // 来自在线列表的对方 userId
        return peerConns_.value(userToPeer_[id], conn_);
    }
    return conn_;                                    // 本机用户 / bot / ai
}

void MainWindow::sendChat(const QString& to, const QString& text,
                         const QString& contentType, const QString& fileId,
                         const QString& fileName, qint64 fileSize,
                         const QString& mime) {
    QJsonObject out;
    out["type"] = "chat";
    out["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    out["to"] = to;
    out["contentType"] = contentType;
    out["text"] = text;
    if (!fileId.isEmpty()) {
        out["fileId"] = fileId; out["fileName"] = fileName;
        out["fileSize"] = fileSize; out["mime"] = mime;
    }

    QString wireTo;
    Connection* target = targetFor(to, wireTo);
    out["to"] = wireTo;
    target->sendJson(out);

    QJsonObject disp = out;
    disp["from"] = myId_;
    disp["fromName"] = myName_;
    Conversation& c = ensureConv(to, to, false);
    c.messages.append(disp);
    if (currentId_ == to) chat_->appendMessage(disp, true);
}

void MainWindow::onSelect(const QString& id) {
    if (id.startsWith("__")) return;   // 分组标题不可选
    currentId_ = id;
    unread_[id] = 0;
    sidebar_->setUnread(id, 0);
    QString title = id;
    if (id == "ai") title = "AI 对话";
    else if (peers_.contains(id)) title = peers_[id].name;
    Conversation& c = ensureConv(id, title, id == "ai");
    chatTitleLabel_->setText(title);
    // 聊天头部头像：AI 固定蓝色，BOT 灰色，否则按名字哈希配色
    if (id == "ai") {
        chatAvatar_->setText("AI");
        chatAvatar_->setStyleSheet("background:#12B7F5;color:#fff;border-radius:18px;"
                                  "font-size:13px;font-weight:700;"
                                  "min-width:36px;max-width:36px;"
                                  "min-height:36px;max-height:36px;");
    } else {
        chatAvatar_->setText(avatarTextFor(title));
        chatAvatar_->setStyleSheet(QString(
            "background:%1;color:#fff;border-radius:18px;font-size:14px;font-weight:700;"
            "min-width:36px;max-width:36px;min-height:36px;max-height:36px;")
            .arg(avatarColorFor(title)));
    }
    chat_->clear();
    for (const auto& m : c.messages)
        chat_->appendMessage(m, m["from"].toString() == myId_ ||
                                 (c.isAi && m["role"].toString() == "user"));
    sidebar_->setCurrentById(id);
}

void MainWindow::onFileClicked(const QString& fileId) {
    if (filePaths_.contains(fileId)) {
        const QString& p = filePaths_[fileId];
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(p))) {
            // 打不开就打开所在目录
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(p).absolutePath()));
        }
        return;
    }
    // 没拿到本地副本（极少见：旧版本互发 / 服务器未投递 binary）
    QMessageBox::information(this, "文件未就绪",
        "这个文件还没保存在本地。\n常见原因：\n"
        "  • 旧版本客户端发来的（不自动下载）\n"
        "  • 局域网连接中断后未传输完成\n\n"
        "可以重新发送一次，或者让对方再发一次。");
}

void MainWindow::onFileSaveAs(const QString& fileId, const QString& fileName) {
    if (!filePaths_.contains(fileId)) {
        QMessageBox::information(this, "无法另存",
            "该文件尚未在本地保存（可能是旧版本互发，或传输尚未完成）。\n可让对方重新发送一次。");
        return;
    }
    QString dest = QFileDialog::getSaveFileName(this, "另存为", fileName);
    if (!dest.isEmpty()) QFile::copy(filePaths_[fileId], dest);
}

void MainWindow::buildSidebar() {
    QList<ConvItem> items;
    items.append({"ai", "AI 对话", "AI", false, false, true, unread_.value("ai", 0)});
    for (const auto& v : server_->config()->bots())
        items.append({v.toObject()["id"].toString(),
                      v.toObject()["name"].toString(), "🤖", false, true, false,
                      unread_.value(v.toObject()["id"].toString(), 0)});
    for (auto it = onlineUsers_.begin(); it != onlineUsers_.end(); ++it) {
        // 属于局域网 peer 的用户交由下方 peers_ 统一展示，避免重复。
        // 兜底：也按名字判断（对端 announce 的 userId 还没到/为空时 userToPeer_ 没建好）。
        if (userToPeer_.contains(it.key())) continue;
        bool isLan = false;
        for (auto pit = peers_.begin(); pit != peers_.end(); ++pit) {
            if (pit.value().online && pit.value().name == it.value()) {
                isLan = true; break;
            }
        }
        if (isLan) continue;
        items.append({it.key(), it.value(), avatarTextFor(it.value()),
                      true, false, false, false, unread_.value(it.key(), 0), ""});
    }
    // 局域网在线设备
    if (!peers_.isEmpty()) {
        int onlineN = 0;
        for (auto it = peers_.begin(); it != peers_.end(); ++it)
            if (it.value().online) ++onlineN;
        items.append({"__lan_header__", QString("局域网在线 (%1)").arg(onlineN),
                      "", false, false, false, true, 0, ""});
        for (auto it = peers_.begin(); it != peers_.end(); ++it) {
            const PeerInfo& p = it.value();
            QString label = p.name + (p.online ? "" : " (离线)");
            items.append({it.key(), label, avatarTextFor(p.name),
                          p.online, false, false, false,
                          unread_.value(it.key(), 0), p.online ? "LAN" : ""});
        }
    }
    sidebar_->setItems(items);
    if (!currentId_.isEmpty()) sidebar_->setCurrentById(currentId_);
}

Conversation& MainWindow::ensureConv(const QString& id, const QString& title, bool isAi) {
    if (!convs_.contains(id)) {
        Conversation c;
        c.title = title;
        c.isAi = isAi;
        c.peerId = id;
        convs_[id] = c;
    }
    Conversation& c = convs_[id];
    if (!c.loaded) {
        c.loaded = true;
        loadHistory(c, convIdFor(id, isAi));
    }
    return c;
}

QString MainWindow::convIdFor(const QString& id, bool isAi) const {
    if (isAi) return "ai:" + myId_;
    if (!server_->config()->findBot(id).isEmpty()) return "bot:" + id + ":" + myId_;
    if (id.contains(':')) {                          // 局域网 peerKey(host:port) -> 用对方 userId 组合
        QString uid = peers_.value(id).userId;
        if (!uid.isEmpty())
            return (myId_ < uid) ? (myId_ + "_" + uid) : (uid + "_" + myId_);
    }
    return (myId_ < id) ? (myId_ + "_" + id) : (id + "_" + myId_);
}

void MainWindow::loadHistory(Conversation& c, const QString& cid) {
    QJsonArray arr = server_->store()->loadRecent(cid, 50);
    for (const auto& v : arr) {
        QJsonObject m = v.toObject();
        if (m.contains("text") || m.contains("contentType"))
            c.messages.append(m);
    }
}

void MainWindow::openSettings() {
    QString oldName = myName_;
    SettingsDialog dlg(server_->config(), this);
    if (dlg.exec() == QDialog::Accepted) {
        // 昵称改了 → 立即更新本机身份、刷新 LAN 广播、刷新头像/标题
        QString newName = server_->config()->displayName();
        if (newName != oldName) {
            myName_ = newName;
            if (!myId_.isEmpty())
                server_->setLocalIdentity(myId_, myName_);
        }
        // 左侧栏头像
        if (auto* a = leftRail_->findChild<QPushButton*>("railAvatar")) {
            a->setText(myName_.isEmpty() ? "我" : myName_.left(1));
        }
        buildSidebar();
        // 当前聊天标题若是局域网 peer，也跟着刷新
        if (currentId_ != "ai" && peers_.contains(currentId_)) {
            chatTitleLabel_->setText(peers_[currentId_].name);
        }
    }
}

void MainWindow::reconnect() {
    conn_->connectTo(serverHost_, serverPort_);
    statusLabel_->setText("重连中…");
}

#include "chatwidget.h"
#include "messagebubble.h"
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFileDialog>
#include <QResizeEvent>
#include <QApplication>

ChatWidget::ChatWidget(QWidget* parent) : QWidget(parent) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 75);   // 底部留 75px，让输入栏不要紧贴窗口底边
    v->setSpacing(0);

    list_ = new QListWidget(this);
    list_->setObjectName("msgList");
    list_->setSpacing(10);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setStyleSheet(
        "QListWidget{background:#F5F6F7;border:none;outline:none;}"
        "QListWidget::item{border:none;}");
    v->addWidget(list_, 1);

    typingLabel_ = new QLabel("", this);
    typingLabel_->setStyleSheet("color:#9aa0a6;font-size:11px;padding:2px 16px 6px 16px;");
    v->addWidget(typingLabel_);

    // 输入区
    inputBar_ = new QWidget(this);
    inputBar_->setObjectName("inputBar");
    inputBar_->setStyleSheet("#inputBar{background:#FFFFFF;border-top:1px solid #ECECEC;padding:10px 14px;}");
    auto* bh = new QHBoxLayout(inputBar_);
    bh->setContentsMargins(0, 0, 0, 0);
    bh->setSpacing(10);

    input_ = new QLineEdit(this);
    input_->setObjectName("chatInput");
    input_->setPlaceholderText("输入消息，Enter 发送");
    input_->setStyleSheet(
        "#chatInput{background:#F2F3F5;border:1px solid #E6E7E9;border-radius:18px;"
        "padding:8px 14px;font-size:13px;color:#1f1f1f;}"
        "#chatInput:focus{border:1px solid #12B7F5;}");
    input_->setFixedHeight(38);

    fileBtn_ = new QPushButton("➕", this);
    fileBtn_->setObjectName("fileBtn");
    fileBtn_->setToolTip("发送文件");
    fileBtn_->setStyleSheet(
        "#fileBtn{background:#F2F3F5;color:#5b5b5b;border:none;border-radius:19px;"
        "min-width:38px;max-width:38px;min-height:38px;max-height:38px;font-size:18px;}"
        "#fileBtn:hover{background:#E6E6E6;}");

    emojiBtn_ = new QPushButton("😊", this);
    emojiBtn_->setObjectName("emojiBtn");
    emojiBtn_->setToolTip("表情");
    emojiBtn_->setStyleSheet(
        "#emojiBtn{background:#F2F3F5;color:#5b5b5b;border:none;border-radius:19px;"
        "min-width:38px;max-width:38px;min-height:38px;max-height:38px;font-size:18px;}"
        "#emojiBtn:hover{background:#E6E6E6;}");

    sendBtn_ = new QPushButton("发送", this);
    sendBtn_->setObjectName("sendBtn");
    sendBtn_->setStyleSheet(
        "#sendBtn{background:#12B7F5;color:#fff;border:none;border-radius:18px;"
        "padding:8px 20px;font-size:13px;font-weight:600;}"
        "#sendBtn:hover{background:#0FA3DC;}");
    sendBtn_->setFixedHeight(38);

    bh->addWidget(input_, 1);
    bh->addWidget(emojiBtn_);
    bh->addWidget(fileBtn_);
    bh->addWidget(sendBtn_);
    v->addWidget(inputBar_);

    connect(sendBtn_, &QPushButton::clicked, this, &ChatWidget::onSend);
    connect(input_, &QLineEdit::returnPressed, this, &ChatWidget::onSend);
    connect(fileBtn_, &QPushButton::clicked, this, &ChatWidget::onPickFile);
    connect(emojiBtn_, &QPushButton::clicked, this, [this]() {
        if (!emojiPanel_) buildEmojiPanel();
        bool show = !emojiPanel_->isVisible();
        emojiPanel_->setVisible(show);
        if (show) positionEmojiPanel();
    });
}

void ChatWidget::buildEmojiPanel() {
    emojiPanel_ = new QWidget(this);
    emojiPanel_->setObjectName("emojiPanel");
    emojiPanel_->setStyleSheet("#emojiPanel{background:#FFFFFF;border:1px solid #E0E0E0;"
                               "border-radius:10px;}");
    auto* gl = new QGridLayout(emojiPanel_);
    gl->setContentsMargins(8, 8, 8, 8);
    gl->setSpacing(4);
    const QStringList emo = {
        "😀","😁","😂","🤣","😊","😍","😘","😎",
        "🤔","😅","😭","😡","👍","👎","👏","🙏",
        "💪","❤️","💔","🎉","🔥","✨","🌹","🍻",
        "😴","🤝","😇","🥳","😋","🤩","😜","😏"
    };
    const int cols = 8;
    for (int i = 0; i < emo.size(); ++i) {
        auto* b = new QPushButton(emo[i], emojiPanel_);
        b->setFixedSize(34, 34);
        b->setStyleSheet("QPushButton{background:transparent;border:none;font-size:20px;"
                         "border-radius:6px;} QPushButton:hover{background:#F2F3F5;}");
        connect(b, &QPushButton::clicked, this, [this, e = emo[i]]() { onEmojiClicked(e); });
        gl->addWidget(b, i / cols, i % cols);
    }
    emojiPanel_->setFixedSize(cols * 34 + 16, (emo.size() / cols) * 34 + 16);
    emojiPanel_->hide();
}

void ChatWidget::positionEmojiPanel() {
    int pw = emojiPanel_->width(), ph = emojiPanel_->height();
    int x = qMax(8, width() - pw - 16);
    int y = qMax(8, height() - 64 - ph);
    emojiPanel_->move(x, y);
    emojiPanel_->raise();
}

void ChatWidget::onEmojiClicked(const QString& e) {
    int pos = input_->cursorPosition();
    QString t = input_->text();
    t.insert(pos, e);
    input_->setText(t);
    input_->setCursorPosition(pos + e.length());
    input_->setFocus();
}

void ChatWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (emojiPanel_ && emojiPanel_->isVisible()) positionEmojiPanel();
}

void ChatWidget::applyTheme(bool dark) {
    list_->setStyleSheet(dark
        ? "QListWidget{background:#1A1A1A;border:none;outline:none;}"
          "QListWidget::item{border:none;}"
        : "QListWidget{background:#F5F6F7;border:none;outline:none;}"
          "QListWidget::item{border:none;}");
    QString barBg = dark ? "#242424" : "#FFFFFF";
    QString barBorder = dark ? "#333333" : "#ECECEC";
    QString roundBg = dark ? "#2A2A2A" : "#F2F3F5";
    QString roundHover = dark ? "#3A3A3A" : "#E6E6E6";
    QString inputBg = dark ? "#2A2A2A" : "#F2F3F5";
    QString inputBorder = dark ? "#3A3A3A" : "#E6E7E9";
    QString inputText = dark ? "#E6E6E6" : "#1f1f1f";
    QString sendBg = dark ? "#2DA7E0" : "#12B7F5";
    QString sendHover = dark ? "#2493C9" : "#0FA3DC";
    inputBar_->setStyleSheet(QString("#inputBar{background:%1;border-top:1px solid %2;padding:10px 14px;}")
                             .arg(barBg).arg(barBorder));
    input_->setStyleSheet(QString(
        "#chatInput{background:%1;border:1px solid %2;border-radius:18px;"
        "padding:8px 14px;font-size:13px;color:%3;}"
        "#chatInput:focus{border:1px solid %4;}")
        .arg(inputBg).arg(inputBorder).arg(inputText).arg(sendBg));
    QString roundStyle = QString(
        "QPushButton{background:%1;color:#5b5b5b;border:none;border-radius:19px;"
        "min-width:38px;max-width:38px;min-height:38px;max-height:38px;font-size:18px;}"
        "QPushButton:hover{background:%2;}")
        .arg(roundBg).arg(roundHover);
    fileBtn_->setStyleSheet(roundStyle);
    emojiBtn_->setStyleSheet(roundStyle);
    sendBtn_->setStyleSheet(QString(
        "#sendBtn{background:%1;color:#fff;border:none;border-radius:18px;"
        "padding:8px 20px;font-size:13px;font-weight:600;}"
        "#sendBtn:hover{background:%2;}").arg(sendBg).arg(sendHover));
    if (emojiPanel_) {
        emojiPanel_->setStyleSheet(dark
            ? "#emojiPanel{background:#242424;border:1px solid #3A3A3A;border-radius:10px;}"
            : "#emojiPanel{background:#FFFFFF;border:1px solid #E0E0E0;border-radius:10px;}");
    }
}

void ChatWidget::onSend() {
    QString t = input_->text().trimmed();
    if (t.isEmpty()) return;
    input_->clear();
    emit sendText(t);
}

void ChatWidget::onPickFile() {
    QString path = QFileDialog::getOpenFileName(this, "选择文件");
    if (!path.isEmpty()) emit sendFile(path);
}

void ChatWidget::clear() {
    list_->clear();
    fidBubbles_.clear();
    streamWidget_ = nullptr;
    streamLabel_ = nullptr;
    streamText_.clear();
}

void ChatWidget::appendMessage(const QJsonObject& msg, bool outgoing) {
    auto* item = new QListWidgetItem(list_);
    auto* bubble = new MessageBubble(msg, outgoing, this);
    connect(bubble, &MessageBubble::fileClicked, this, &ChatWidget::fileClicked);
    connect(bubble, &MessageBubble::fileSaveAs, this, &ChatWidget::fileSaveAs);
    item->setSizeHint(bubble->sizeHint());
    list_->setItemWidget(item, bubble);
    list_->addItem(item);
    QString fid = msg["fileId"].toString();
    if (!fid.isEmpty()) fidBubbles_[fid] = bubble;
    scrollToBottom();
}

void ChatWidget::setFileProgress(const QString& fileId, int pct) {
    auto* b = fidBubbles_.value(fileId);
    if (b) b->setProgress(pct);
}

void ChatWidget::setFileDone(const QString& fileId, bool ok, const QString& path) {
    auto* b = fidBubbles_.value(fileId);
    if (b) b->setDone(ok, path);
}

void ChatWidget::appendAssistantDelta(const QString& delta) {
    streamText_ += delta;
    if (!streamWidget_) {
        auto* item = new QListWidgetItem(list_);
        streamWidget_ = new QWidget(this);
        auto* root = new QHBoxLayout(streamWidget_);
        root->setContentsMargins(4, 4, 4, 4);
        root->addStretch(1);
        auto* box = new QWidget(streamWidget_);
        auto* bv = new QVBoxLayout(box);
        bv->setContentsMargins(12, 8, 12, 8);
        bv->setSpacing(2);
        streamLabel_ = new QLabel("", box);
        streamLabel_->setWordWrap(true);
        streamLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QVariant sv = qApp->property("streamBg");
        QVariant sb = qApp->property("streamBorder");
        QString streamBg = sv.isValid() ? sv.toString() : "#FFFFFF";
        QString streamBorder = sb.isValid() ? sb.toString() : "#EBEBEB";
        box->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:12px;")
                           .arg(streamBg).arg(streamBorder));
        bv->addWidget(streamLabel_);
        root->addWidget(box);
        root->addStretch(0);
        item->setSizeHint(streamWidget_->sizeHint());
        list_->setItemWidget(item, streamWidget_);
        list_->addItem(item);
    }
    streamLabel_->setText(streamText_.toHtmlEscaped());
    scrollToBottom();
}

void ChatWidget::finalizeAssistant() {
    streamWidget_ = nullptr;
    streamLabel_ = nullptr;
    streamText_.clear();
}

void ChatWidget::showTyping(bool on) {
    typingLabel_->setText(on ? "对方正在输入…" : "");
}

void ChatWidget::scrollToBottom() {
    list_->scrollToBottom();
}

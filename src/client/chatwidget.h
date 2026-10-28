#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QString>

class QListWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QWidget;
class QResizeEvent;

// 单个会话的聊天视图：消息列表 + 输入框 + 发送/附件按钮 + 输入状态。
class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget* parent = nullptr);

public slots:
    void clear();
    void appendMessage(const QJsonObject& msg, bool outgoing);
    void appendAssistantDelta(const QString& delta);
    void finalizeAssistant();
    void showTyping(bool on);
    void applyTheme(bool dark);
    void setFileProgress(const QString& fileId, int pct);
    void setFileDone(const QString& fileId, bool ok, const QString& path);

signals:
    void sendText(const QString& text);
    void sendFile(const QString& path);
    void fileClicked(const QString& fileId);
    void fileSaveAs(const QString& fileId, const QString& fileName);

private slots:
    void onSend();
    void onPickFile();
    void onEmojiClicked(const QString& e);

private:
    void scrollToBottom();
    void resizeEvent(QResizeEvent* e) override;
    void buildEmojiPanel();
    void positionEmojiPanel();

    QListWidget* list_ = nullptr;
    QLineEdit* input_ = nullptr;
    QLabel* typingLabel_ = nullptr;
    QPushButton* sendBtn_ = nullptr;
    QPushButton* fileBtn_ = nullptr;
    QPushButton* emojiBtn_ = nullptr;
    QWidget* emojiPanel_ = nullptr;
    QWidget* inputBar_ = nullptr;
    QWidget* streamWidget_ = nullptr;
    QLabel* streamLabel_ = nullptr;
    QString streamText_;

    QMap<QString, class MessageBubble*> fidBubbles_;
};

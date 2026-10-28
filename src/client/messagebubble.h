#pragma once
#include <QWidget>
#include <QJsonObject>

class QProgressBar;
class QLabel;

// 单条消息气泡：支持文本、文件、图片。outgoing=true 时靠右、绿色。
class MessageBubble : public QWidget {
    Q_OBJECT
public:
    explicit MessageBubble(const QJsonObject& msg, bool outgoing, QWidget* parent = nullptr);

    // 文件传输进度（0-100）。仅当本条是 file/image 时有意义。
    void setProgress(int pct);
    // 传输完成/失败：完成且本地已有 filePath 时显示可打开状态。
    void setDone(bool ok, const QString& filePath = QString());

signals:
    void fileClicked(const QString& fileId);
    void fileOpenFolder(const QString& filePath);
    void fileSaveAs(const QString& fileId, const QString& fileName);

private:
    void openWithSystem();      // 用系统默认程序打开
    void showContextMenu(const QPoint& pos);
    bool eventFilter(QObject* watched, QEvent* event) override;

    QString fileId_;
    QString fileName_;
    QString filePath_;          // 本地已保存/可打开的路径
    bool isImage_ = false;
    QProgressBar* progress_ = nullptr;
    QLabel* imgLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QWidget* box_ = nullptr;
};

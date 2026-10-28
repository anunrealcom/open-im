#pragma once
#include <QObject>
#include <QJsonObject>
#include <QString>

// 消息持久化：按会话（对方 userId 或 "ai"/botId）追加到 JSONL 文件。
// 选 JSONL 而非 SQLite，是为了零额外依赖、便于运维时直接查看。
class Store : public QObject {
    Q_OBJECT
public:
    explicit Store(QObject* parent = nullptr);
    void setBaseDir(const QString& dir);

    void append(const QString& conversationId, const QJsonObject& msg);
    QJsonArray loadRecent(const QString& conversationId, int limit = 50);

private:
    QString filePath(const QString& conversationId);
    QString baseDir_;
};

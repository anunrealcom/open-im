#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

// 运行时配置：JSON 文件 + 内存对象，支持保存与变更信号。
// 配置项完全对应原 Web 版：服务端端口、鉴权、AI Provider、机器人、WebRTC、局域网发现。
class Config : public QObject {
    Q_OBJECT
public:
    explicit Config(QObject* parent = nullptr);

    bool load(const QString& path);
    bool save();
    QString path() const { return path_; }

    // ---- 通用访问 ----
    QJsonObject root() const { return root_; }
    QJsonValue get(const QString& key, const QJsonValue& def = QJsonValue()) const;
    void set(const QString& key, const QJsonValue& value);

    // ---- 便捷访问 ----
    quint16 serverPort() const { return (quint16)get("server/port", 8080).toInt(); }
    QString displayName() const { return get("server/name", "一幻IM").toString(); }
    bool authEnabled() const { return get("auth/enabled", false).toBool(); }
    QString authToken() const { return get("auth/token", "").toString(); }
    QJsonArray aiProviders() const { return get("aiProviders", QJsonArray()).toArray(); }
    QJsonArray bots() const { return get("bots", QJsonArray()).toArray(); }
    QJsonArray webrtc() const { return get("webrtc", QJsonArray()).toArray(); }
    bool lanEnabled() const { return get("lan/enabled", true).toBool(); }
    quint16 lanPort() const { return (quint16)get("lan/port", 4799).toInt(); }

    QJsonObject findAiProvider(const QString& id) const;
    QJsonObject findBot(const QString& id) const;

signals:
    void changed();

private:
    QString path_;
    QJsonObject root_;
};

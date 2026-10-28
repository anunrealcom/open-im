#pragma once
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QNetworkAccessManager>
#include <functional>

// OpenAI 兼容的流式对话代理：向 baseUrl/chat/completions 发起 SSE 请求，
// 通过回调增量返回 token。依赖 QtNetwork 的 QNetworkAccessManager。
class AiProxy : public QObject {
    Q_OBJECT
public:
    using TokenCb = std::function<void(const QString& delta)>;
    using EndCb   = std::function<void(const QString& error)>; // error 为空表示成功

    explicit AiProxy(QObject* parent = nullptr);

    // provider: {baseUrl, apiKey, model}
    // history: [{role:"system"|"user"|"assistant", content:"..."}]
    void stream(const QJsonObject& provider,
                const QJsonArray& history,
                const TokenCb& onToken,
                const EndCb& onEnd,
                QObject* ctx = nullptr);

private:
    QNetworkAccessManager mgr_;
};

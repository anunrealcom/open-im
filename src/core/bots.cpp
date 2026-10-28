#include "bots.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>

BotManager::BotManager(Config* config, QObject* parent)
    : QObject(parent), config_(config) {}

void BotManager::handle(const QString& botId,
                        const QString& text,
                        const QString& fromId,
                        const std::function<void(const QString&)>& onReply,
                        const std::function<void()>& onDone,
                        QObject* ctx) {
    Q_UNUSED(fromId);
    QObject* owner = ctx ? ctx : (QObject*)this;
    QJsonObject bot = config_->findBot(botId);
    if (bot.isEmpty()) { onDone(); return; }
    QString type = bot["type"].toString();

    if (type == "echo") {
        onReply(text);
        onDone();
        return;
    }

    if (type == "webhook") {
        QUrl url(bot["webhook"].toString());
        if (!url.isValid()) { onReply("[webhook 地址无效]"); onDone(); return; }
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonObject payload;
        payload["message"] = text;
        payload["bot"] = botId;
        QNetworkReply* reply = mgr_.post(req, QJsonDocument(payload).toJson());
        QObject::connect(reply, &QNetworkReply::finished, owner, [=]() {
            QString body;
            if (reply->error() == QNetworkReply::NoError)
                body = QString::fromUtf8(reply->readAll());
            else
                body = "[webhook 错误: " + reply->errorString() + "]";
            onReply(body);
            onDone();
            reply->deleteLater();
        });
        return;
    }

    if (type == "agent") {
        QJsonObject provider = config_->findAiProvider(bot["provider"].toString());
        if (provider.isEmpty()) provider = config_->aiProviders().first().toObject();
        if (provider["apiKey"].toString().isEmpty()) {
            // 用户还没填 apiKey 时优雅回退到 echo，让 bot 仍能立刻回话
            // 同时给一个提示，引导去设置里填 key
            onReply(text + "\n\n（提示：在左上角打开设置 → AI 提供商，填入 apiKey 即可启用真正的智能回复）");
            onDone();
            return;
        }
        QJsonArray history;
        if (!bot["prompt"].toString().isEmpty())
            history.append(QJsonObject{{"role", "system"}, {"content", bot["prompt"].toString()}});
        history.append(QJsonObject{{"role", "user"}, {"content", text}});
        QString acc;
        ai_.stream(provider, history,
            [&acc](const QString& delta) { acc += delta; },
            [onReply, onDone, &acc](const QString& err) {
                if (!err.isEmpty()) onReply("[agent 错误: " + err + "]");
                else if (!acc.isEmpty()) onReply(acc);
                onDone();
            },
            owner);
        return;
    }

    onReply("[未知机器人类型: " + type + "]");
    onDone();
}

#pragma once
#include <QObject>
#include <QString>
#include <functional>
#include "config.h"
#include "ai.h"
#include <QNetworkAccessManager>

// 机器人框架：支持 echo / webhook / agent 三类适配器。
// - echo：原样回显
// - webhook：把消息 POST 到外部 URL，回复取响应体
// - agent：转发给某个 AI Provider（复用 AiProxy）
class BotManager : public QObject {
    Q_OBJECT
public:
    explicit BotManager(Config* config, QObject* parent = nullptr);

    // 处理发给 botId 的文本；onReply 返回回复文本（可能多次/一次），onDone 表示结束。
    void handle(const QString& botId,
                const QString& text,
                const QString& fromId,
                const std::function<void(const QString&)>& onReply,
                const std::function<void()>& onDone,
                QObject* ctx = nullptr);

private:
    Config* config_;
    AiProxy ai_;
    QNetworkAccessManager mgr_;
};

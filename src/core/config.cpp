#include "config.h"
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QStandardPaths>

Config::Config(QObject* parent) : QObject(parent) {}

bool Config::load(const QString& path) {
    path_ = path;
    QFile f(path);
    if (!f.exists()) {
        // 首次运行：写入默认配置
        root_ = QJsonObject{
            {"server", QJsonObject{{"port", 8080}, {"name", "一幻IM"}}},
            {"auth", QJsonObject{{"enabled", false}, {"token", ""}}},
            {"lan", QJsonObject{{"enabled", true}, {"port", 4799}}},
            {"webrtc", QJsonArray{
                QJsonObject{{"url", "stun:stun.l.google.com:19302"}}
            }},
            // 默认 AI 走 DeepSeek（OpenAI 兼容协议），在设置里填入自己的 apiKey 即可使用
            {"aiProviders", QJsonArray{
                QJsonObject{
                    {"id", "default"}, {"name", "DeepSeek(OpenAI兼容)"},
                    {"baseUrl", "https://api.deepseek.com/v1"},
                    {"apiKey", ""},
                    {"model", "deepseek-chat"}
                }
            }},
            {"bots", QJsonArray{
                QJsonObject{
                    {"id", "xiaohuan"}, {"name", "小幻"},
                    {"type", "agent"},
                    {"prompt", "你是一幻——一个友好、简洁、略带幽默感的智能助手。用简体中文回复，问题尽量短而准确。"},
                    {"webhook", ""}, {"provider", "default"}
                }
            }}
        };
        return save();
    }
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return false;
    root_ = doc.object();

    // 治愈旧版本默认值（2026-08-31 升级）：
    //   1) aiProviders[0].baseUrl 为空时升级到 DeepSeek
    //   2) 小幻 bot 默认从 echo 升到 agent
    bool changed = false;
    auto aiProviders = root_["aiProviders"].toArray();
    if (!aiProviders.isEmpty()) {
        auto p = aiProviders[0].toObject();
        if (p["baseUrl"].toString().isEmpty()) {
            p["baseUrl"] = "https://api.deepseek.com/v1";
            p["model"]  = "deepseek-chat";
            p["name"]   = "DeepSeek(OpenAI兼容)";
            aiProviders[0] = p;
            root_["aiProviders"] = aiProviders;
            changed = true;
        }
    }
    auto bots = root_["bots"].toArray();
    for (int i = 0; i < bots.size(); ++i) {
        auto b = bots[i].toObject();
        if (b["id"].toString() == "xiaohuan" && b["type"].toString() == "echo") {
            b["type"] = "agent";
            b["prompt"] = "你是一幻——一个友好、简洁、略带幽默感的智能助手。用简体中文回复，问题尽量短而准确。";
            bots[i] = b;
            root_["bots"] = bots;
            changed = true;
        }
    }
    if (changed) save();
    return true;
}

bool Config::save() {
    QFile f(path_);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root_).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

QJsonValue Config::get(const QString& key, const QJsonValue& def) const {
    QStringList parts = key.split('/');
    QJsonObject obj = root_;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj.contains(parts[i]) || !obj[parts[i]].isObject()) return def;
        obj = obj[parts[i]].toObject();
    }
    QString last = parts.last();
    return obj.contains(last) ? obj[last] : def;
}

void Config::set(const QString& key, const QJsonValue& value) {
    QStringList parts = key.split('/');
    QJsonObject obj = root_;
    QJsonObject* p = &obj;
    // 逐层确保对象存在
    QList<QJsonObject> stack;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!p->contains(parts[i]) || !(*p)[parts[i]].isObject())
            (*p)[parts[i]] = QJsonObject();
        stack.push_back(*p);
        *p = (*p)[parts[i]].toObject();
    }
    (*p)[parts.last()] = value;
    // 回写
    for (int i = parts.size() - 2; i >= 0; --i) {
        QJsonObject top = stack[i];
        top[parts[i]] = *p;
        *p = top;
    }
    root_ = obj;
    save();
    emit changed();
}

QJsonObject Config::findAiProvider(const QString& id) const {
    for (const auto& v : aiProviders()) {
        QJsonObject o = v.toObject();
        if (o["id"].toString() == id) return o;
    }
    return QJsonObject();
}

QJsonObject Config::findBot(const QString& id) const {
    for (const auto& v : bots()) {
        QJsonObject o = v.toObject();
        if (o["id"].toString() == id) return o;
    }
    return QJsonObject();
}

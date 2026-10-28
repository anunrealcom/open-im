#include "ai.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

AiProxy::AiProxy(QObject* parent) : QObject(parent) {}

void AiProxy::stream(const QJsonObject& provider,
                     const QJsonArray& history,
                     const TokenCb& onToken,
                     const EndCb& onEnd,
                     QObject* ctx) {
    QObject* owner = ctx ? ctx : (QObject*)this;

    if (provider["apiKey"].toString().isEmpty()) {
        onEnd("provider 未配置 apiKey");
        return;
    }

    QUrl url(provider["baseUrl"].toString().trimmed());
    QString path = url.path();
    if (path.isEmpty() || path.endsWith('/'))
        path += "chat/completions";
    else
        path += "/chat/completions";
    url.setPath(path);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + provider["apiKey"].toString()).toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject body;
    body["model"] = provider["model"].toString();
    body["messages"] = history;
    body["stream"] = true;

    QNetworkReply* reply = mgr_.post(req, QJsonDocument(body).toJson());

    auto* buf = new QString;
    auto* done = new bool(false);

    QObject::connect(reply, &QNetworkReply::readyRead, owner, [=]() {
        *buf += QString::fromUtf8(reply->readAll());
        int nl;
        while ((nl = buf->indexOf('\n')) >= 0) {
            QString line = buf->left(nl).trimmed();
            *buf = buf->mid(nl + 1);
            if (!line.startsWith("data:")) continue;
            QString data = line.mid(5).trimmed();
            if (data == "[DONE]") {
                if (!*done) { *done = true; onEnd(""); }
                return;
            }
            QJsonDocument d = QJsonDocument::fromJson(data.toUtf8());
            if (!d.isObject()) continue;
            QJsonArray choices = d.object()["choices"].toArray();
            if (choices.isEmpty()) continue;
            QJsonObject delta = choices[0].toObject()["delta"].toObject();
            QString content = delta["content"].toString();
            if (!content.isEmpty()) onToken(content);
        }
    });

    QObject::connect(reply, &QNetworkReply::finished, owner, [=]() {
        if (!*done) {
            *done = true;
            onEnd(reply->error() == QNetworkReply::NoError ? "" : reply->errorString());
        }
        reply->deleteLater();
        delete buf;
        delete done;
    });
}

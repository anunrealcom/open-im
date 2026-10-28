#include "store.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>

Store::Store(QObject* parent) : QObject(parent) {
    baseDir_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/history";
}

void Store::setBaseDir(const QString& dir) {
    baseDir_ = dir;
    QDir().mkpath(baseDir_);
}

void Store::append(const QString& conversationId, const QJsonObject& msg) {
    QDir().mkpath(baseDir_);
    QFile f(filePath(conversationId));
    if (!f.open(QIODevice::Append)) return;
    f.write(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    f.write("\n");
    f.close();
}

QJsonArray Store::loadRecent(const QString& conversationId, int limit) {
    QJsonArray out;
    QFile f(filePath(conversationId));
    if (!f.open(QIODevice::ReadOnly)) return out;
    QStringList lines;
    while (!f.atEnd()) lines.append(QString::fromUtf8(f.readLine()).trimmed());
    f.close();
    int start = qMax(0, lines.size() - limit);
    for (int i = start; i < lines.size(); ++i) {
        QJsonDocument d = QJsonDocument::fromJson(lines[i].toUtf8());
        if (d.isObject()) out.append(d.object());
    }
    return out;
}

QString Store::filePath(const QString& conversationId) {
    QString safe = conversationId;
    safe.replace(QRegularExpression("[^A-Za-z0-9_\\-]"), "_");
    return baseDir_ + "/" + safe + ".jsonl";
}

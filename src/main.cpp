#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QSysInfo>
#include <cstdio>
#include "core/server.h"
#include "client/mainwindow.h"

// 把 qInfo/qWarning 等写到 %APPDATA%/YiHuanIM/run.log，便于无头/后台调试
void lanLogHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/run.log";
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        QString tag = (type == QtWarningMsg) ? "WARN" : (type == QtCriticalMsg ? "CRIT" : "INFO");
        ts << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
           << " [" << tag << "] " << msg << "\n";
    }
    fprintf(stderr, "%s\n", msg.toUtf8().constData());
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("一幻IM");
    qInstallMessageHandler(lanLogHandler);

    // 自检参数：--autologin=昵称 --userid=xxx --serverport=NNNN
    QString autoName, autoUid, autoPortStr;
    for (int i = 1; i < argc; ++i) {
        QString a = QString::fromLocal8Bit(argv[i]);
        if (a.startsWith("--autologin=")) autoName = a.mid(12);
        else if (a.startsWith("--userid=")) autoUid = a.mid(9);
        else if (a.startsWith("--serverport=")) autoPortStr = a.mid(13);
    }

    Server server;
    if (!server.start()) {
        QMessageBox::critical(nullptr, "启动失败", "无法启动内置服务端，请检查端口占用。");
        return 1;
    }

    QString name, uid, host;
    quint16 port = 0;
    if (!autoName.isEmpty()) {
        name = autoName;
        uid = autoUid;
        host = "127.0.0.1";
        if (!autoPortStr.isEmpty()) port = (quint16)autoPortStr.toUInt();
    } else {
        // 无登录框：优先用上次保存的昵称；空则用电脑名（Windows COMPUTERNAME，兜底 QSysInfo）
        name = server.config()->displayName();
        if (name.isEmpty() || name == "一幻IM") {
            QString pc = qEnvironmentVariable("COMPUTERNAME");
            if (pc.isEmpty()) pc = QSysInfo::machineHostName();
            if (pc.isEmpty()) pc = "User";
            name = pc;
            server.config()->set("server/name", name);   // 落盘，下次启动直接用
        }
        host = "127.0.0.1";
        // 用户ID 留空，让服务端 welcome 时自动分配
        uid = QString();
    }

    MainWindow w(&server, name, uid, host, port);
    // 必须等 MainWindow 构造完（其中已 connect server_ 的 lanPeer 信号）再启动局域网发现，
    // 否则首个 peer 广播会因为没有接收者而被 seen_ 去重永久丢失。
    server.startLanDiscovery();
    w.show();
    return app.exec();
}

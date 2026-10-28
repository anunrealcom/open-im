#pragma once
#include <QDialog>
#include <QString>

// 登录对话框：昵称（必填）、可选用户ID、可选服务器地址/端口。
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);

    QString name() const;
    QString userId() const;
    QString host() const;     // 留空默认 127.0.0.1
    quint16 port() const;    // 0 表示用默认

private:
    class QLineEdit* nameEdit_;
    class QLineEdit* idEdit_;
    class QLineEdit* hostEdit_;
    class QLineEdit* portEdit_;
};

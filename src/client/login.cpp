#include "login.h"
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QWindow>
#include <QMouseEvent>

namespace {
class DlgTitleBar : public QWidget {
public:
    explicit DlgTitleBar(QWidget* p = nullptr) : QWidget(p) {}
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton)
            if (auto* w = window()->windowHandle()) w->startSystemMove();
        QWidget::mousePressEvent(e);
    }
};
}

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumWidth(380);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(26);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 55));

    auto* frame = new QWidget(this);
    frame->setObjectName("dlgFrame");
    frame->setGraphicsEffect(shadow);

    auto* fl = new QVBoxLayout(frame);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setSpacing(0);

    // 标题栏
    auto* tb = new DlgTitleBar(frame);
    tb->setObjectName("dlgTitle");
    tb->setFixedHeight(48);
    auto* th = new QHBoxLayout(tb);
    th->setContentsMargins(20, 0, 14, 0);
    auto* t = new QLabel("登录一幻 IM", tb);
    t->setStyleSheet("font-size:15px;font-weight:600;color:#1f1f1f;");
    th->addWidget(t);
    th->addStretch(1);
    auto* closeB = new QPushButton("✕", tb);
    closeB->setObjectName("dlgClose");
    th->addWidget(closeB);
    connect(closeB, &QPushButton::clicked, this, &QDialog::reject);
    fl->addWidget(tb);

    // 主体
    auto* body = new QWidget(frame);
    body->setObjectName("dlgBody");
    auto* bv = new QVBoxLayout(body);
    bv->setContentsMargins(28, 14, 28, 10);
    bv->setSpacing(14);

    auto* logo = new QLabel("幻", body);
    logo->setObjectName("dlgLogo");
    logo->setAlignment(Qt::AlignCenter);
    bv->addWidget(logo, 0, Qt::AlignHCenter);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setSpacing(14);
    form->setContentsMargins(0, 0, 0, 0);
    nameEdit_ = new QLineEdit(this);
    idEdit_ = new QLineEdit(this);
    hostEdit_ = new QLineEdit(this);
    portEdit_ = new QLineEdit(this);
    nameEdit_->setObjectName("dlgEdit"); nameEdit_->setPlaceholderText("你的昵称");
    idEdit_->setObjectName("dlgEdit"); idEdit_->setPlaceholderText("可选，留空自动生成");
    hostEdit_->setObjectName("dlgEdit"); hostEdit_->setPlaceholderText("默认 127.0.0.1（本机）");
    portEdit_->setObjectName("dlgEdit"); portEdit_->setPlaceholderText("默认 8080");
    form->addRow("昵称", nameEdit_);
    form->addRow("用户ID", idEdit_);
    form->addRow("服务器", hostEdit_);
    form->addRow("端口", portEdit_);
    bv->addLayout(form);
    fl->addWidget(body);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, frame);
    box->setObjectName("dlgBtns");
    box->button(QDialogButtonBox::Ok)->setObjectName("dlgOk");
    box->button(QDialogButtonBox::Cancel)->setObjectName("dlgCancel");
    connect(box, &QDialogButtonBox::accepted, this, &LoginDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &LoginDialog::reject);
    fl->addWidget(box);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->addWidget(frame);

    frame->setStyleSheet(R"(
        QLabel{color:#5b5b5b;font-size:12px;}
        #dlgTitle{background:#FFFFFF;border-top-left-radius:16px;border-top-right-radius:16px;}
        #dlgClose{background:transparent;border:none;color:#999;font-size:14px;border-radius:6px;padding:4px 9px;}
        #dlgClose:hover{background:#F2F3F5;color:#333;}
        #dlgLogo{background:#12B7F5;color:#fff;border-radius:32px;font-size:24px;font-weight:700;
                 min-width:64px;max-width:64px;min-height:64px;max-height:64px;margin-bottom:4px;}
        #dlgBody{background:#FFFFFF;}
        #dlgEdit{background:#F2F3F5;border:1px solid #E6E7E9;border-radius:10px;padding:9px 12px;font-size:13px;color:#1f1f1f;}
        #dlgEdit:focus{border:1px solid #12B7F5;}
        #dlgBtns{background:#FFFFFF;border-bottom-left-radius:16px;border-bottom-right-radius:16px;
                 padding:8px 18px 18px 18px;}
        #dlgOk{background:#12B7F5;color:#fff;border:none;border-radius:10px;padding:9px 26px;font-size:13px;font-weight:600;}
        #dlgOk:hover{background:#0FA3DC;}
        #dlgCancel{background:#F2F3F5;color:#555;border:none;border-radius:10px;padding:9px 26px;font-size:13px;}
        #dlgCancel:hover{background:#E6E6E6;}
    )");
    setStyleSheet("QDialogButtonBox{border:none;}");

    connect(nameEdit_, &QLineEdit::textChanged, [this, box]() {
        box->button(QDialogButtonBox::Ok)->setEnabled(!nameEdit_->text().trimmed().isEmpty());
    });
    box->button(QDialogButtonBox::Ok)->setEnabled(false);
}

QString LoginDialog::name() const { return nameEdit_->text().trimmed(); }
QString LoginDialog::userId() const { return idEdit_->text().trimmed(); }
QString LoginDialog::host() const { return hostEdit_->text().trimmed(); }
quint16 LoginDialog::port() const {
    bool ok = false;
    quint16 p = portEdit_->text().trimmed().toUShort(&ok);
    return ok ? p : 0;
}

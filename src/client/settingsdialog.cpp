#include "settingsdialog.h"
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QWindow>
#include <QMouseEvent>
#include <QJsonObject>
#include <QJsonArray>

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

SettingsDialog::SettingsDialog(Config* config, QWidget* parent)
    : QDialog(parent), cfg_(config) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumWidth(440);

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

    auto* tb = new DlgTitleBar(frame);
    tb->setObjectName("dlgTitle");
    tb->setFixedHeight(48);
    auto* th = new QHBoxLayout(tb);
    th->setContentsMargins(20, 0, 14, 0);
    auto* t = new QLabel("设置", tb);
    t->setStyleSheet("font-size:15px;font-weight:600;color:#1f1f1f;");
    th->addWidget(t);
    th->addStretch(1);
    auto* closeB = new QPushButton("✕", tb);
    closeB->setObjectName("dlgClose");
    th->addWidget(closeB);
    connect(closeB, &QPushButton::clicked, this, &QDialog::reject);
    fl->addWidget(tb);

    auto* body = new QWidget(frame);
    body->setObjectName("dlgBody");
    auto* bv = new QVBoxLayout(body);
    bv->setContentsMargins(24, 16, 24, 12);
    bv->setSpacing(14);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setSpacing(13);
    form->setContentsMargins(0, 0, 0, 0);

    nameEdit_ = new QLineEdit(cfg_->displayName(), this);
    lanChk_ = new QCheckBox("启用局域网发现", this);
    lanChk_->setChecked(cfg_->lanEnabled());

    QJsonObject ai = cfg_->aiProviders().first().toObject();
    aiBase_ = new QLineEdit(ai["baseUrl"].toString(), this);
    aiKey_ = new QLineEdit(ai["apiKey"].toString(), this);
    aiKey_->setEchoMode(QLineEdit::Password);
    aiModel_ = new QLineEdit(ai["model"].toString(), this);

    QJsonObject bot = cfg_->bots().first().toObject();
    botType_ = new QComboBox(this);
    botType_->addItems({"echo", "webhook", "agent"});
    botType_->setCurrentText(bot["type"].toString());
    botTarget_ = new QLineEdit(bot["webhook"].toString().isEmpty()
                                   ? bot["provider"].toString()
                                   : bot["webhook"].toString(), this);
    botTarget_->setPlaceholderText("webhook 地址 或 agent 的 provider id");

    form->addRow("显示名称", nameEdit_);
    form->addRow("", lanChk_);
    form->addRow("AI Base URL", aiBase_);
    form->addRow("AI API Key", aiKey_);
    form->addRow("AI 模型", aiModel_);
    form->addRow("机器人类型", botType_);
    form->addRow("机器人目标", botTarget_);
    bv->addLayout(form);
    fl->addWidget(body);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, frame);
    box->setObjectName("dlgBtns");
    box->button(QDialogButtonBox::Ok)->setObjectName("dlgOk");
    box->button(QDialogButtonBox::Cancel)->setObjectName("dlgCancel");
    box->button(QDialogButtonBox::Ok)->setText("保存");
    box->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(box, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    fl->addWidget(box);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->addWidget(frame);

    frame->setStyleSheet(R"(
        QLabel{color:#5b5b5b;font-size:12px;}
        #dlgTitle{background:#FFFFFF;border-top-left-radius:16px;border-top-right-radius:16px;}
        #dlgClose{background:transparent;border:none;color:#999;font-size:14px;border-radius:6px;padding:4px 9px;}
        #dlgClose:hover{background:#F2F3F5;color:#333;}
        #dlgBody{background:#FFFFFF;}
        QLineEdit{background:#F2F3F5;border:1px solid #E6E7E9;border-radius:10px;padding:8px 12px;font-size:13px;color:#1f1f1f;}
        QLineEdit:focus{border:1px solid #12B7F5;}
        QComboBox{background:#F2F3F5;border:1px solid #E6E7E9;border-radius:10px;padding:8px 12px;font-size:13px;color:#1f1f1f;}
        QComboBox::drop-down{border:none;}
        QCheckBox{color:#1f1f1f;font-size:13px;spacing:6px;}
        QCheckBox::indicator{width:16px;height:16px;}
        #dlgBtns{background:#FFFFFF;border-bottom-left-radius:16px;border-bottom-right-radius:16px;
                 padding:8px 18px 18px 18px;}
        #dlgOk{background:#12B7F5;color:#fff;border:none;border-radius:10px;padding:9px 26px;font-size:13px;font-weight:600;}
        #dlgOk:hover{background:#0FA3DC;}
        #dlgCancel{background:#F2F3F5;color:#555;border:none;border-radius:10px;padding:9px 26px;font-size:13px;}
        #dlgCancel:hover{background:#E6E6E6;}
    )");
    setStyleSheet("QDialogButtonBox{border:none;}");
}

void SettingsDialog::save() {
    cfg_->set("server/name", nameEdit_->text().trimmed());
    cfg_->set("lan/enabled", lanChk_->isChecked());

    QJsonArray providers = cfg_->aiProviders();
    if (providers.isEmpty()) providers.append(QJsonObject());
    QJsonObject ai = providers.first().toObject();
    ai["baseUrl"] = aiBase_->text().trimmed();
    ai["apiKey"] = aiKey_->text().trimmed();
    ai["model"] = aiModel_->text().trimmed();
    providers[0] = ai;
    cfg_->set("aiProviders", providers);

    QJsonArray bots = cfg_->bots();
    if (bots.isEmpty()) bots.append(QJsonObject());
    QJsonObject bot = bots.first().toObject();
    QString type = botType_->currentText();
    bot["type"] = type;
    if (type == "webhook") bot["webhook"] = botTarget_->text().trimmed();
    else bot["provider"] = botTarget_->text().trimmed();
    bots[0] = bot;
    cfg_->set("bots", bots);

    accept();
}

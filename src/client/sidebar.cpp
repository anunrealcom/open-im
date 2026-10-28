#include "sidebar.h"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

// 单张会话卡片：圆形头像 + 名称 + 在线点 + 未读红点。选中用 checked 状态高亮。
class ConvCard : public QPushButton {
public:
    QString cid;
    QLabel* badge_ = nullptr;
    ConvCard(const ConvItem& it, QWidget* p = nullptr) : QPushButton(p), cid(it.id) {
        setObjectName("convCard");
        setCheckable(true);
        setFixedHeight(60);
        setCursor(Qt::PointingHandCursor);

        auto* h = new QHBoxLayout(this);
        h->setContentsMargins(12, 8, 12, 8);
        h->setSpacing(12);

        auto* avatar = new QLabel(it.avatar, this);
        QString bg;
        if (it.isBot) bg = "#E8E8EA";
        else if (it.isAi) bg = "#12B7F5";
        else bg = avatarColorFor(it.name);
        avatar->setStyleSheet(QString(
            "background:%1;color:%2;border-radius:20px;font-size:14px;font-weight:700;"
            "min-width:40px;max-width:40px;min-height:40px;max-height:40px;")
            .arg(bg).arg(it.isBot ? "#333333" : "#FFFFFF"));
        avatar->setAlignment(Qt::AlignCenter);
        h->addWidget(avatar);

        auto* v = new QVBoxLayout();
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(3);
        auto* name = new QLabel(it.name, this);
        name->setObjectName("convName");
        name->setFixedHeight(18);
        v->addWidget(name);
        h->addLayout(v, 1);

        if (!it.badge.isEmpty()) {
            auto* bl = new QLabel(it.badge, this);
            bl->setStyleSheet("color:#12B7F5;font-size:10px;font-weight:700;"
                              "background:#E8F4FB;padding:1px 6px;border-radius:6px;");
            h->addWidget(bl);
        }

        if (it.online) {
            auto* dot = new QLabel("●", this);
            dot->setStyleSheet("color:#34C759;font-size:9px;");
            h->addWidget(dot);
        }

        badge_ = new QLabel(this);
        badge_->setObjectName("unreadBadge");
        badge_->setStyleSheet("background:#FA5151;color:#fff;border-radius:10px;"
                              "font-size:11px;font-weight:700;min-width:18px;padding:1px 5px;");
        badge_->setAlignment(Qt::AlignCenter);
        h->addWidget(badge_);
        updateBadge(it.unread);
    }
    void updateBadge(int n) {
        if (n <= 0) { badge_->hide(); badge_->clear(); }
        else { badge_->setText(n > 99 ? "99+" : QString::number(n)); badge_->show(); }
    }
};

Sidebar::Sidebar(QWidget* parent) : QWidget(parent) {
    setObjectName("sidebar");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sa = new QScrollArea(this);
    sa->setObjectName("sidebarScroll");
    sa->setWidgetResizable(true);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setStyleSheet("QScrollArea{border:none;background:#FFFFFF;}");

    listWidget_ = new QWidget(sa);
    listLayout_ = new QVBoxLayout(listWidget_);
    listLayout_->setContentsMargins(8, 8, 8, 8);
    listLayout_->setSpacing(4);
    listLayout_->addStretch(1);
    sa->setWidget(listWidget_);

    root->addWidget(sa);

    applyTheme(false);
}

void Sidebar::applyTheme(bool dark) {
    QString nameColor = dark ? "#DADADA" : "#1f1f1f";
    QString hover = dark ? "#2E2E2E" : "#F2F3F5";
    QString checked = dark ? "#1F3A4D" : "#D6E9FF";
    setStyleSheet(QString(
        "#convName { color: %1; font-size: 13px; font-weight: 500; }"
        "#convCard { background: transparent; border: none; border-radius: 10px;"
        " text-align: left; padding: 0; }"
        "#convCard:hover { background: %2; }"
        "#convCard:checked { background: %3; }")
        .arg(nameColor).arg(hover).arg(checked));
}

void Sidebar::setItems(const QList<ConvItem>& items) {
    // 清空旧卡片（从底部 stretch 之上删除）
    while (listLayout_->count() > 1) {
        auto* item = listLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }
    cards_.clear();
    items_.clear();

    for (const auto& it : items) {
        items_.append(it);
        if (it.isHeader) {
            // 分组标题：不可点击的灰色小标题
            auto* hdr = new QLabel(it.name, listWidget_);
            hdr->setObjectName("convHeader");
            hdr->setStyleSheet("color:#8a8a8a;font-size:11px;font-weight:600;"
                              "padding:8px 14px 2px 14px;");
            listLayout_->insertWidget(listLayout_->count() - 1, hdr);
            continue;
        }
        auto* card = new ConvCard(it, listWidget_);
        connect(card, &QPushButton::clicked, this, [this, card, id = it.id]() {
            for (auto* c : cards_) c->setChecked(false);
            card->setChecked(true);
            emit selected(id);
        });
        listLayout_->insertWidget(listLayout_->count() - 1, card);
        cards_.append(card);
    }
}

void Sidebar::setCurrentById(const QString& id) {
    for (auto* c : cards_)
        if (c->cid == id) { c->setChecked(true); return; }
}

void Sidebar::setUnread(const QString& id, int n) {
    for (int i = 0; i < cards_.size(); ++i) {
        if (cards_[i]->cid == id) {
            cards_[i]->updateBadge(n);
            if (i < items_.size()) items_[i].unread = n;
            return;
        }
    }
}

void Sidebar::filter(const QString& text) {
    QString t = text.trimmed();
    for (int i = 0; i < items_.size() && i < cards_.size(); ++i) {
        bool show = t.isEmpty() || items_[i].name.contains(t, Qt::CaseInsensitive);
        cards_[i]->setVisible(show);
    }
}

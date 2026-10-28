#pragma once
#include <QWidget>
#include <QString>
#include <QList>
#include <QVBoxLayout>
#include <QChar>

class ConvCard;

struct ConvItem {
    QString id;
    QString name;
    QString avatar;     // 头像文字（emoji 或首字）
    bool online = false;
    bool isBot = false;
    bool isAi = false;
    bool isHeader = false;  // 分组标题（不可点击）
    int unread = 0;     // 未读消息计数
    QString badge;      // 右侧小标签（如 "LAN"）
};

// 根据名字哈希出一个稳定的头像背景色（同色人每次看到的颜色一致）
inline QString avatarColorFor(const QString& name) {
    if (name.isEmpty()) return "#5B8DEF";
    static const QStringList palette = {
        "#5B8DEF", "#12B7F5", "#2ECC71", "#F39C12",
        "#9B59B6", "#E74C3C", "#1ABC9C", "#E67E22",
        "#34495E", "#16A085", "#8E44AD", "#D35400"
    };
    uint h = 0;
    for (QChar c : name) h = h * 131u + c.unicode();
    return palette[h % (uint)palette.size()];
}

// 头像显示文字：取首字（中文/emoji 都行），空则用默认
inline QString avatarTextFor(const QString& name) {
    if (name.isEmpty()) return QString::fromUtf8("\xF0\x9F\x91\xA4"); // 👤
    return name.left(1);
}

// 左侧会话列表：QQ 风格卡片（圆形头像 + 名称 + 在线点 + 未读红点），点击发出 selected(id)。
class Sidebar : public QWidget {
    Q_OBJECT
public:
    explicit Sidebar(QWidget* parent = nullptr);
    void setItems(const QList<ConvItem>& items);
    void setCurrentById(const QString& id);
    void setUnread(const QString& id, int n);   // 动态更新某会话未读红点
    void filter(const QString& text);           // 按名称过滤
    void applyTheme(bool dark);                 // light/dark 主题

signals:
    void selected(const QString& id);

private:
    QWidget* listWidget_ = nullptr;
    QVBoxLayout* listLayout_ = nullptr;
    QList<ConvCard*> cards_;
    QList<ConvItem> items_;   // 保留以便过滤
};

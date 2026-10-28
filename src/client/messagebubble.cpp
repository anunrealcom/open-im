#include "messagebubble.h"
#include "sidebar.h"            // avatarColorFor / avatarTextFor
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QMenu>
#include <QFileInfo>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>
#include <QImageReader>
#include <QDialog>
#include <QMouseEvent>
#include <QFileDialog>
#include <QDateTime>

// 判断是否为可内联预览的图片类型
static bool isImageMime(const QString& mime, const QString& fileName) {
    if (mime.startsWith("image/")) return true;
    static const QStringList ext = {"png","jpg","jpeg","gif","bmp","webp","svg"};
    QString suf = QFileInfo(fileName).suffix().toLower();
    return ext.contains(suf);
}

MessageBubble::MessageBubble(const QJsonObject& msg, bool outgoing, QWidget* parent)
    : QWidget(parent) {
    QVariant vo = qApp->property("bubbleOut");
    QVariant vi = qApp->property("bubbleIn");
    QVariant vob = qApp->property("bubbleOutBorder");
    QVariant vib = qApp->property("bubbleInBorder");
    QVariant vt = qApp->property("bubbleText");
    QString bubbleColor = outgoing
        ? (vo.isValid() ? vo.toString() : "#95EC69")
        : (vi.isValid() ? vi.toString() : "#FFFFFF");
    QString borderColor = outgoing
        ? (vob.isValid() ? vob.toString() : "#8FD94E")
        : (vib.isValid() ? vib.toString() : "#EBEBEB");
    QString textColor = vt.isValid() ? vt.toString() : "#1f1f1f";

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(12, 6, 12, 6);
    root->setSpacing(0);

    if (outgoing) root->addStretch(1);

    // ---- 气泡 + 尖角 + 时间戳（无头像、无名字 label、气泡去边框）----
    auto* col = new QWidget(this);
    auto* colLay = new QVBoxLayout(col);
    colLay->setContentsMargins(0, 0, 0, 0);
    colLay->setSpacing(2);

    auto* row = new QWidget(col);
    auto* rh = new QHBoxLayout(row);
    rh->setContentsMargins(0, 0, 0, 0);
    rh->setSpacing(0);

    if (!outgoing) {
        auto* tri = new QLabel(row);
        tri->setFixedSize(8, 12);
        tri->setStyleSheet(QString(
            "background:transparent;border-style:solid;border-width:6px 8px 6px 0;"
            "border-color:transparent %1 transparent transparent;").arg(bubbleColor));
        rh->addWidget(tri);
    }

    box_ = new QWidget(row);
    box_->setMaximumWidth(460);
    auto* v = new QVBoxLayout(box_);
    v->setContentsMargins(12, 8, 12, 8);
    v->setSpacing(4);

    QString contentType = msg["contentType"].toString();
    fileId_ = msg["fileId"].toString();
    fileName_ = msg["fileName"].toString();
    isImage_ = isImageMime(msg["mime"].toString(), fileName_);

    if (contentType == "file" || contentType == "image") {
        if (isImage_) {
            imgLabel_ = new QLabel(box_);
            imgLabel_->setAlignment(Qt::AlignCenter);
            imgLabel_->setText("🖼️ 图片");
            imgLabel_->setStyleSheet("color:#9aa0a6;font-size:12px;padding:8px;");
            imgLabel_->setCursor(Qt::PointingHandCursor);
            imgLabel_->installEventFilter(this);
            v->addWidget(imgLabel_);
        } else {
            auto* btn = new QPushButton("📎 " + fileName_, box_);
            btn->setStyleSheet("text-align:left;padding:8px 12px;border:none;border-radius:8px;"
                               "background:rgba(0,0,0,0.05);color:#1f1f1f;font-size:13px;");
            btn->setCursor(Qt::PointingHandCursor);
            btn->setToolTip("点击用默认程序打开 · 右键更多");
            connect(btn, &QPushButton::clicked, this, &MessageBubble::openWithSystem);
            v->addWidget(btn);
        }
        progress_ = new QProgressBar(box_);
        progress_->setRange(0, 100);
        progress_->setValue(0);
        progress_->setTextVisible(true);
        progress_->setFixedHeight(14);
        progress_->setStyleSheet(
            "QProgressBar{border:none;border-radius:7px;background:#E6E6E6;font-size:10px;}"
            "QProgressBar::chunk{background:#12B7F5;border-radius:7px;}");
        v->addWidget(progress_);
        statusLabel_ = new QLabel("", box_);
        statusLabel_->setStyleSheet("color:#9aa0a6;font-size:11px;");
        v->addWidget(statusLabel_);
    } else {
        QString text = msg["text"].toString();
        auto* label = new QLabel(text.toHtmlEscaped(), box_);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setStyleSheet(QString("color:%1;font-size:13px;").arg(textColor));
        v->addWidget(label);
    }

    // 气泡去边框，只保留背景色 + 圆角
    box_->setStyleSheet(QString(
        "background:%1;border-radius:14px;").arg(bubbleColor));
    rh->addWidget(box_);

    if (outgoing) {
        auto* tri = new QLabel(row);
        tri->setFixedSize(8, 12);
        tri->setStyleSheet(QString(
            "background:transparent;border-style:solid;border-width:6px 0 6px 8px;"
            "border-color:transparent transparent transparent %1;").arg(bubbleColor));
        rh->addWidget(tri);
    }
    colLay->addWidget(row);

    qint64 ts = msg["ts"].toVariant().toLongLong();
    QString timeStr = ts > 0
        ? QDateTime::fromMSecsSinceEpoch(ts).toString("HH:mm")
        : QDateTime::currentDateTime().toString("HH:mm");
    auto* tsLabel = new QLabel(timeStr, col);
    tsLabel->setStyleSheet("color:#b0b0b0;font-size:10px;");
    auto* tsLay = new QHBoxLayout();
    tsLay->setContentsMargins(0, 0, 0, 0);
    tsLay->setSpacing(0);
    if (outgoing) {
        tsLay->addStretch(1);
        tsLay->addWidget(tsLabel);
    } else {
        tsLay->addWidget(tsLabel);
        tsLay->addStretch(1);
    }
    auto* tsWrap = new QWidget(col);
    tsWrap->setLayout(tsLay);
    colLay->addWidget(tsWrap);

    // 根布局组装：来消息 [col][stretch]；发消息 [stretch][col]
    if (outgoing) {
        root->addWidget(col);
    } else {
        root->addWidget(col);
        root->addStretch(1);
    }

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, &MessageBubble::showContextMenu);
}

bool MessageBubble::eventFilter(QObject* watched, QEvent* event) {
    if (watched == imgLabel_ && event->type() == QEvent::MouseButtonPress) {
        if (!filePath_.isEmpty()) openWithSystem();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void MessageBubble::setProgress(int pct) {
    if (!progress_) return;
    progress_->setVisible(true);
    progress_->setValue(qBound(0, pct, 100));
    if (statusLabel_) statusLabel_->setText(pct >= 100 ? "完成" : "传输中…");
}

void MessageBubble::setDone(bool ok, const QString& filePath) {
    if (!filePath.isEmpty()) filePath_ = filePath;
    if (progress_) progress_->setVisible(false);
    if (statusLabel_) {
        if (ok && !filePath_.isEmpty()) {
            statusLabel_->setText("✓ 已接收");
            statusLabel_->setStyleSheet("color:#2ECC71;font-size:11px;");
        } else if (ok) {
            statusLabel_->setText("✓ 已发送");
            statusLabel_->setStyleSheet("color:#2ECC71;font-size:11px;");
        } else {
            statusLabel_->setText("✗ 传输失败");
            statusLabel_->setStyleSheet("color:#FA5151;font-size:11px;");
        }
    }
    // 图片：加载本地预览
    if (ok && isImage_ && !filePath_.isEmpty() && imgLabel_) {
        QPixmap pm;
        if (pm.load(filePath_) && !pm.isNull()) {
            int maxW = 240;
            QPixmap scaled = pm.width() > maxW
                ? pm.scaledToWidth(maxW, Qt::SmoothTransformation) : pm;
            imgLabel_->setPixmap(scaled);
            imgLabel_->setText("");
            imgLabel_->setStyleSheet("");
        } else {
            imgLabel_->setText("🖼️ 图片 (无法预览)");
        }
    }
}

void MessageBubble::openWithSystem() {
    if (filePath_.isEmpty()) return;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath_))) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath_).absolutePath()));
    }
}

void MessageBubble::showContextMenu(const QPoint& pos) {
    QMenu menu(this);
    QAction* actOpen = menu.addAction("打开");
    QAction* actFolder = menu.addAction("打开所在文件夹");
    QAction* actSave = menu.addAction("另存为…");
    QAction* actCopy = menu.addAction("复制文件名");
    actOpen->setEnabled(!filePath_.isEmpty());
    actFolder->setEnabled(!filePath_.isEmpty());

    QAction* chosen = menu.exec(mapToGlobal(pos));
    if (!chosen) return;
    if (chosen == actOpen) {
        openWithSystem();
    } else if (chosen == actFolder) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath_).absolutePath()));
    } else if (chosen == actSave) {
        emit fileSaveAs(fileId_, fileName_);
    } else if (chosen == actCopy) {
        QApplication::clipboard()->setText(fileName_);
    }
}

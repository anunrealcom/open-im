#pragma once
#include <QDialog>
#include "core/config.h"

// 设置：服务端名称、局域网发现开关、首个 AI Provider、首个机器人。
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Config* config, QWidget* parent = nullptr);

private slots:
    void save();

private:
    Config* cfg_;
    class QLineEdit* nameEdit_;
    class QCheckBox* lanChk_;
    class QLineEdit* aiBase_;
    class QLineEdit* aiKey_;
    class QLineEdit* aiModel_;
    class QComboBox* botType_;
    class QLineEdit* botTarget_;
};

#pragma once
#include <QMessageBox>
#include <QPushButton>
#include <QLayout>

namespace ui {

// Keep the standard modal/keyboard behavior while sharing the main window palette.
inline QMessageBox::StandardButton confirm(QWidget* parent, const QString& title,
                                           const QString& text)
{
    QMessageBox dialog(parent);
    dialog.setOption(QMessageBox::Option::DontUseNativeDialog);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_StyledBackground);
    dialog.setObjectName(QStringLiteral("confirmationDialog"));
    dialog.setWindowTitle(title);
    dialog.setTextFormat(Qt::PlainText);
    dialog.setText(title);
    dialog.setInformativeText(text);
    dialog.setIcon(QMessageBox::NoIcon);
    dialog.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    dialog.setDefaultButton(QMessageBox::No);
    dialog.setEscapeButton(QMessageBox::No);
    auto* confirmButton = dialog.button(QMessageBox::Yes);
    confirmButton->setObjectName(QStringLiteral("confirmButton"));
    confirmButton->setText(QStringLiteral("确认"));
    auto* cancelButton = dialog.button(QMessageBox::No);
    cancelButton->setObjectName(QStringLiteral("cancelButton"));
    cancelButton->setText(QStringLiteral("取消"));
    dialog.setStyleSheet(QStringLiteral(R"(
        QMessageBox#confirmationDialog {
            background: #ffffff; border: 1px solid #e5eaf0; border-radius: 12px;
        }
        QWidget { font-family: "Microsoft YaHei"; font-size: 13px; color: #333333; }
        QLabel { background: transparent; border: none; }
        QLabel#qt_msgbox_label { font-size: 16px; font-weight: bold; padding-bottom: 6px; }
        QLabel#qt_msgbox_informativelabel { color: #667080; min-width: 320px; padding-bottom: 14px; }
        QDialogButtonBox { background: transparent; border: none; }
        QPushButton { min-width: 72px; padding: 8px 16px; border: 1px solid transparent; border-radius: 7px; }
        QPushButton#cancelButton { background: #f2f4f7; color: #333333; }
        QPushButton#cancelButton:hover { background: #e5eaf0; }
        QPushButton#cancelButton:pressed { background: #dce3eb; }
        QPushButton#confirmButton { background: #0099ff; color: #ffffff; }
        QPushButton#confirmButton:hover { background: #008de8; }
        QPushButton#confirmButton:pressed { background: #007bc9; }
        QPushButton:focus { border: 1px solid #0099ff; }
    )"));
    dialog.layout()->setContentsMargins(24, 24, 24, 20);
    dialog.layout()->setSpacing(12);
    dialog.exec();
    return dialog.clickedButton() == confirmButton ? QMessageBox::Yes : QMessageBox::No;
}

} // namespace ui

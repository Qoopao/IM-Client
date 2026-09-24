#ifndef LOGINFAILED_H
#define LOGINFAILED_H

#include <QDialog>
#include <QMouseEvent>

namespace Ui {
class LoginFailed;
}

class LoginFailed : public QDialog
{
    Q_OBJECT

public:
    explicit LoginFailed(QWidget *parent = nullptr);
    ~LoginFailed();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void on_shutdownButton_clicked();

    void on_confirmButton_clicked();

private:
    Ui::LoginFailed *ui;

    // 自定义菜单窗口拖拽
    QPoint m_dragStartPos;
    bool m_bDragging = false;
};

#endif // LOGINFAILED_H

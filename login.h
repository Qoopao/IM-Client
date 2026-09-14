#ifndef LOGIN_H
#define LOGIN_H

#include <QMainWindow>
#include <QMouseEvent>
#include <QMovie>
#include <QRegularExpressionValidator>
#include <QPoint>

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWindow;
}
QT_END_NAMESPACE

class LoginWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow() override;

    bool isCapsLockOn();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;


private slots:
    void on_shutdownButton_clicked();

    void on_minimizeButton_clicked();

    void on_registerButton_clicked();

private:
    Ui::LoginWindow *ui;

    // 自定义菜单窗口拖拽
    QPoint m_dragStartPos;
    bool m_bDragging = false;
};
#endif // LOGIN_H

#ifndef LOGIN_H
#define LOGIN_H

#include <QMainWindow>
#include <QMouseEvent>
#include <QMovie>
#include <QRegularExpressionValidator>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool isCapsLockOn();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;


private slots:
    void on_shutdownButton_clicked();

    void on_minimizeButton_clicked();

private:
    Ui::MainWindow *ui;

    // 自定义菜单窗口拖拽
    QPoint m_dragStartPos;
    bool m_bDragging = false;
};
#endif // LOGIN_H

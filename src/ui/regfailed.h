#ifndef REGFAILED_H
#define REGFAILED_H

#include <QDialog>
#include <QMouseEvent>

namespace Ui {
class RegFailed;
}

class RegFailed : public QDialog
{
    Q_OBJECT

public:
    explicit RegFailed(QWidget *parent = nullptr);
    ~RegFailed();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void on_shutdownButton_clicked();

    void on_confirmButton_clicked();

private:
    Ui::RegFailed *ui;
    QPoint m_dragStartPos;
    bool m_bDragging = false;
};

#endif // REGFAILED_H

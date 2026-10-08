#ifndef REG_H
#define REG_H

#include <QWidget>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QMouseEvent>
#include <QTimer>

#include "../controller/regController.h"
class QCloseEvent;

namespace Ui {
class Reg;
}

class Reg : public QWidget
{
    Q_OBJECT

public:
    explicit Reg(QWidget *parent = nullptr);
    ~Reg();

signals:
    void regOk(const QString& account);

protected:
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void on_shutdownButton_clicked();

    void on_registerButton_clicked();

private:
    void setRegistering(bool registering);
    void showRegistrationError(const QString& message);
    bool meetPhoneNumberStandard(const QString& phonenumber);
    bool meetUserNameStandard(const QString& username);
    bool meetPasswordStandard(const QString& password);

private:
    Ui::Reg *ui;
    controller::RegController m_controller;

    QPoint m_dragStartPos;
    bool m_bDragging = false;
};

#endif // REG_H

#ifndef REG_H
#define REG_H

#include <QWidget>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace Ui {
class Reg;
}

class Reg : public QWidget
{
    Q_OBJECT

public:
    explicit Reg(QWidget *parent = nullptr);
    ~Reg();

private slots:
    void on_shutdownButton_clicked();

    void on_registerButton_clicked();

private:
    bool meetPhoneNumberStandard(const QString& phonenumber);
    bool meetUserNameStandard(const QString& username);
    bool meetPasswordStandard(const QString& password);

private:
    Ui::Reg *ui;
};

#endif // REG_H

#include "reg.h"
#include "ui_reg.h"

Reg ::Reg(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Reg)
{
    ui->setupUi(this);

    // 设置菜单栏
    setWindowFlags(Qt::Window
                   | Qt::FramelessWindowHint
                   | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint);
    ui->shutdownButton->setText("\u00d7");


    // 表单
    ui->phoneNumber->setPlaceholderText("请输入电话号码");
    ui->phoneNumber->setAlignment(Qt::AlignCenter);
    ui->phoneNumber->setMaxLength(11);
    QRegularExpression phoneReg(R"(^1[3-9]\d{9}$)");
    QRegularExpressionValidator *phoneVal = new QRegularExpressionValidator(phoneReg,this);
    ui->phoneNumber->setValidator(phoneVal);
    ui->phoneStandard->setStyleSheet(R"(color: red;)");

    ui->userName->setPlaceholderText("请输入用户名");
    ui->userName->setAlignment(Qt::AlignCenter);
    ui->usernameStandard->setWordWrap(true);
    QRegularExpression userNameReg(R"(^[\p{Han}a-zA-Z][\p{Han}a-zA-Z0-9_]{2,9}$)");
    QRegularExpressionValidator* userNameVal = new QRegularExpressionValidator(userNameReg,this);
    ui->userName->setValidator(userNameVal);
    ui->usernameStandard->setStyleSheet(R"(color: red;)");

    ui->password->setPlaceholderText("请设置密码");
    ui->password->setAlignment(Qt::AlignCenter);
    QRegularExpression pwdReg(R"(^[A-Za-z0-9]{6,16}$)");            // 6到16位，数字或字母
    QRegularExpressionValidator* pwdVal = new QRegularExpressionValidator(pwdReg,this);
    ui->password->setValidator(pwdVal);
    ui->passwordStandard->setStyleSheet(R"(color: red;)");


    connect(ui->phoneNumber, &QLineEdit::textChanged,ui->phoneHolder,[this](const QString& text){
        if(meetPhoneNumberStandard(text)){
            ui->phoneHolder->setStyleSheet(R"(QWidget#phoneHolder {
                                                                    background-color: #ffffff;
                                                                    border: 2px solid green;
                                                                    border-top-left-radius: 10px;
                                                                    border-top-right-radius:10px;
                                                                    border-bottom-left-radius:10px;
                                                                    border-bottom-right-radius:10px;
                                                                    })");
            ui->phoneStandard->clear();
        }else{
            ui->phoneHolder->setStyleSheet(R"(QWidget#phoneHolder {
                                                                    background-color: #ffffff;
                                                                    border: 2px solid red;
                                                                    border-top-left-radius: 10px;
                                                                    border-top-right-radius:10px;
                                                                    border-bottom-left-radius:10px;
                                                                    border-bottom-right-radius:10px;
                                                                    })");
            ui->phoneStandard->setText("请正确填写手机号。");
        }
    });

    connect(ui->userName, &QLineEdit::textChanged,ui->usernameHolder,[this](const QString& text){
        if(meetUserNameStandard(text)){
            ui->usernameHolder->setStyleSheet(R"(QWidget#usernameHolder {
                                                                    background-color: #ffffff;
                                                                    border: 2px solid green;
                                                                    border-top-left-radius: 10px;
                                                                    border-top-right-radius:10px;
                                                                    border-bottom-left-radius:10px;
                                                                    border-bottom-right-radius:10px;
                                                                    })");
            ui->usernameStandard->clear();
        }else{
            ui->usernameHolder->setStyleSheet(R"(QWidget#usernameHolder {
                                                                    background-color: #ffffff;
                                                                    border: 2px solid red;
                                                                    border-top-left-radius: 10px;
                                                                    border-top-right-radius:10px;
                                                                    border-bottom-left-radius:10px;
                                                                    border-bottom-right-radius:10px;
                                                                    })");
            ui->usernameStandard->setText("用户名必须是长度在3~10之间，以字母或汉字开头的字符串，允许中文、字母、数字、下划线。");
        }
    });

    connect(ui->password, &QLineEdit::textChanged,ui->passwordHolder,[this](const QString& text){
        if(meetPasswordStandard(text)){
            ui->passwordHolder->setStyleSheet(R"(QWidget#passwordHolder {
                                                                    background-color: #ffffff;
                                                                    border: 2px solid green;
                                                                    border-top-left-radius: 10px;
                                                                    border-top-right-radius:10px;
                                                                    border-bottom-left-radius:10px;
                                                                    border-bottom-right-radius:10px;
                                                                    })");
            ui->passwordStandard->clear();
        }else{
            ui->passwordHolder->setStyleSheet(R"(QWidget#passwordHolder {
                                                                    background-color: #ffffff;
                                                                    border: 2px solid red;
                                                                    border-top-left-radius: 10px;
                                                                    border-top-right-radius:10px;
                                                                    border-bottom-left-radius:10px;
                                                                    border-bottom-right-radius:10px;
                                                                    })");
            ui->passwordStandard->setText("密码必须同时包含字母与数字，长度为6到16。");
        }
    });

}

Reg ::~Reg()
{
    delete ui;
}

void Reg::on_shutdownButton_clicked()
{
    this->close();
}


bool Reg::meetPhoneNumberStandard(const QString& phonenumber){
    auto val = qobject_cast<const QRegularExpressionValidator*>(ui->phoneNumber->validator());
    if(val){
        return val->regularExpression().match(phonenumber).hasMatch();
    }
    return false;
}
bool Reg::meetUserNameStandard(const QString& username){
    auto val = qobject_cast<const QRegularExpressionValidator*>(ui->userName->validator());
    if(val){
        return val->regularExpression().match(username).hasMatch();
    }
    return false;
}
bool Reg::meetPasswordStandard(const QString& password){
    auto val = qobject_cast<const QRegularExpressionValidator*>(ui->password->validator());
    if(val){
        return val->regularExpression().match(password).hasMatch();
    }
    return false;
}

void Reg::on_registerButton_clicked()
{
    if(meetPhoneNumberStandard(ui->phoneNumber->text()) && meetUserNameStandard(ui->userName->text()) && meetPasswordStandard(ui->password->text())){
        qDebug()<< "待开发";
    }
}


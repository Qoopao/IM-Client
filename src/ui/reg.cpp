#include "reg.h"
#include "ui_reg.h"
#include "regfailed.h"
#include "../dao/remoteregdao.h"
#include <QCloseEvent>

Reg ::Reg(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Reg)
{
    ui->setupUi(this);
    m_regDao = new dao::RemoteRegDao(this);
    this->setFocusPolicy(Qt::ClickFocus);

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
    ui->usernameStandard->setStyleSheet(R"(color: gray;)");

    ui->password->setPlaceholderText("请设置密码");
    ui->password->setAlignment(Qt::AlignCenter);
    QRegularExpression pwdReg(R"(^[A-Za-z0-9]{6,16}$)");            // 6到16位，数字或字母
    QRegularExpressionValidator* pwdVal = new QRegularExpressionValidator(pwdReg,this);
    ui->password->setValidator(pwdVal);
    ui->passwordStandard->setStyleSheet(R"(color: gray;)");

    ui->passwordStandard->setText("密码必须同时包含字母与数字，长度为6到16。");
    ui->usernameStandard->setText("用户名必须是长度在3~10之间，以字母或汉字开头的字符串，允许中文、字母、数字、下划线。");


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
        }else{
            ui->usernameHolder->setStyleSheet(R"(QWidget#usernameHolder {
                                                                    background-color: #ffffff;
                                                                    border: 2px solid red;
                                                                    border-top-left-radius: 10px;
                                                                    border-top-right-radius:10px;
                                                                    border-bottom-left-radius:10px;
                                                                    border-bottom-right-radius:10px;
                                                                    })");
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
        }else{
            ui->passwordHolder->setStyleSheet(R"(QWidget#passwordHolder {
                                                                    background-color: #ffffff;
                                                                    border: 2px solid red;
                                                                    border-top-left-radius: 10px;
                                                                    border-top-right-radius:10px;
                                                                    border-bottom-left-radius:10px;
                                                                    border-bottom-right-radius:10px;
                                                                    })");
        }
    });

}

Reg::~Reg()
{
    qDebug() << "[~Reg] enter";
    m_regDao->cancel();
    qDebug() << "[~Reg] cancel done";
    delete ui;
    qDebug() << "[~Reg] exit";
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
        return val->regularExpression().match(password).hasMatch() && password.contains(QRegularExpression("[A-Za-z]")) && password.contains(QRegularExpression("[0-9]"));
    }
    return false;
}

void Reg::on_registerButton_clicked()
{
    if (m_registering)
        return;

    const QString phone = ui->phoneNumber->text();
    const QString nickname = ui->userName->text();
    const QString password = ui->password->text();
    if (!meetPhoneNumberStandard(phone)) {
        ui->phoneNumber->setFocus();
        showRegistrationError(QStringLiteral("请正确填写手机号。"));
        return;
    }
    if (!meetUserNameStandard(nickname)) {
        ui->userName->setFocus();
        showRegistrationError(ui->usernameStandard->text());
        return;
    }
    if (!meetPasswordStandard(password)) {
        ui->password->setFocus();
        showRegistrationError(ui->passwordStandard->text());
        return;
    }

    setRegistering(true);
    m_regDao->sendReg(phone, nickname, password, [this](const model::RegResult& result) {
        qDebug() << "[R1] Reg callback enter, error=" << int(result.error);
        setRegistering(false);
        qDebug() << "[R2] Reg callback: setRegistering done";
        if (result.error == model::RegError::None) {
            qDebug() << "[R3] Reg callback: success, account=" << result.account;
            ui->password->clear();
            qDebug() << "[R4] Reg callback: password cleared, before emit";
            emit regOk(QString::number(result.account));
            qDebug() << "[R5] Reg callback: after emit";      // ← 关键
            return;
        }
        qDebug() << "[R6] Reg callback: failure path";
        showRegistrationError(result.message.isEmpty()
                                  ? QStringLiteral("注册失败，请稍后重试。") : result.message);
        qDebug() << "[R7] Reg callback: error shown";
    });
}

void Reg::setRegistering(bool registering)
{
    m_registering = registering;
    ui->registerButton->setEnabled(!registering);
    ui->registerButton->setText(registering ? QStringLiteral("注册中…") : QStringLiteral("立即注册"));
    ui->phoneNumber->setEnabled(!registering);
    ui->userName->setEnabled(!registering);
    ui->password->setEnabled(!registering);
}

void Reg::showRegistrationError(const QString& message)
{
    auto* dialog = new RegFailed(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setMessage(message);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->show();
}

void Reg::closeEvent(QCloseEvent* event)
{
    qDebug() << "[CE1] Reg::closeEvent enter";
    m_regDao->cancel();
    qDebug() << "[CE2] Reg::closeEvent: cancel done";
    setRegistering(false);
    qDebug() << "[CE3] Reg::closeEvent: setRegistering done";
    QWidget::closeEvent(event);
    qDebug() << "[CE4] Reg::closeEvent exit";
}

void Reg::mousePressEvent(QMouseEvent *event)
{
    if(ui->menuWidget->geometry().contains(event->pos()))
    {
        m_bDragging = true;
        m_dragStartPos = event->globalPosition().toPoint() - this->frameGeometry().topLeft();
    }
}

void Reg::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDragging)
        this->move(event->globalPosition().toPoint() - m_dragStartPos);
}

void Reg::mouseReleaseEvent(QMouseEvent *event)
{
    m_bDragging = false;
}

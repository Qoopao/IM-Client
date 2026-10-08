#include "reg.h"
#include "ui_reg.h"
#include "regfailed.h"
#include <QCloseEvent>

Reg ::Reg(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Reg)
{
    ui->setupUi(this);
    connect(&m_controller, &controller::RegController::stateChanged, this,
            [this](const model::RegModel& state) {
                setRegistering(state.status == model::RegModel::Status::Registering);
                if (state.status == model::RegModel::Status::Success) {
                    ui->password->clear();
                    emit regOk(QString::number(state.result.account));
                } else if (state.status == model::RegModel::Status::Failed) {
                    showRegistrationError(state.result.message);
                }
            });
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
    ui->phoneStandard->setStyleSheet(R"(color: red;)");

    ui->userName->setPlaceholderText("请输入用户名");
    ui->userName->setAlignment(Qt::AlignCenter);
    ui->usernameStandard->setWordWrap(true);
    ui->usernameStandard->setStyleSheet(R"(color: gray;)");

    ui->password->setPlaceholderText("请设置密码");
    ui->password->setAlignment(Qt::AlignCenter);
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
    m_controller.cancel();
    qDebug() << "[~Reg] cancel done";
    delete ui;
    qDebug() << "[~Reg] exit";
}

void Reg::on_shutdownButton_clicked()
{
    this->close();
}


bool Reg::meetPhoneNumberStandard(const QString& value)
{
    return m_controller.validPhone(value);
}

bool Reg::meetUserNameStandard(const QString& value)
{
    return m_controller.validNickname(value);
}

bool Reg::meetPasswordStandard(const QString& value)
{
    return m_controller.validPassword(value);
}

void Reg::on_registerButton_clicked()
{
    m_controller.registerAccount(ui->phoneNumber->text(), ui->userName->text(), ui->password->text());
}

void Reg::setRegistering(bool registering)
{
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
    m_controller.cancel();
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

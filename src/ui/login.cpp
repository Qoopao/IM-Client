#include "login.h"
#include "ui_login.h"
#include "avatarLabel.h"
#include <QPixmap>
#include <QStyle>
#include <QDebug>
#include <QMessageBox>  // [MVC-REFACTOR] 登录结果提示框
#include <windows.h>
#include "accountDropdown.h"
#include "reg.h"
#include "loginFailed.h"

LoginWindow::LoginWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LoginWindow)
    , m_controller(&m_model)
{
    ui->setupUi(this);

    connect(&m_model, &model::AuthModel::statusChanged, this,
            [this](model::AuthModel::Status status) {
                if (status == model::AuthModel::Status::LoggingIn) {
                    ui->loginButton->setText(QStringLiteral("登录中…"));
                    ui->loginButton->setEnabled(false);
                    return;
                }
                ui->loginButton->setText(QStringLiteral("登录"));
                ui->loginButton->setEnabled(true);
                if (status == model::AuthModel::Status::Failed) {
                    LoginFailed *lfw = new LoginFailed(this);
                    lfw->setReason(m_model.getErrorMessage());
                    lfw->exec();
                    lfw->setAttribute(Qt::WA_DeleteOnClose);
                } else if (status == model::AuthModel::Status::Success) {
                    QMessageBox::information(this, QStringLiteral("登录成功"), QStringLiteral("欢迎，%1").arg(m_model.getAccount()));
                }
            });

    this->setWindowIcon(QIcon(":/jpg/ape"));


    // 设置菜单栏
    setWindowFlags(Qt::Window
                   | Qt::FramelessWindowHint
                   | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint);
    ui->minimizeButton->setText("\u2212");
    ui->shutdownButton->setText("\u00d7");

    // 设置圆形头像Label
    CircularAvatarLabel* avatar = new CircularAvatarLabel(100,this);
    avatar->setAvatar(QPixmap(":/jpg/elep"));
    ui->avatarHolder->layout()->addWidget(avatar);
    ui->avatarHolder->layout()->setAlignment(Qt::AlignCenter);

    // 输入栏与输入栏的按钮，不用QPainter是因为响应速度问题
    ui->AclrButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    ui->AclrButton->setIconSize(QSize(14,14));
    ui->PclrButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    ui->PclrButton->setIconSize(QSize(14,14));
    ui->AclrButton->setVisible(false);
    ui->PclrButton->setVisible(false);
    ui->AclrButton->setFocusPolicy(Qt::NoFocus);
    ui->PclrButton->setFocusPolicy(Qt::NoFocus);
    ui->account->setAlignment(Qt::AlignCenter);
    ui->account->setPlaceholderText("输入账号");
    QRegularExpression accRe("^\\d*$");
    ui->account->setValidator(new QRegularExpressionValidator(accRe, this));
    ui->password->installEventFilter(this);
    ui->password->setAlignment(Qt::AlignCenter);
    ui->password->setPlaceholderText("输入密码");
    ui->password->setEchoMode(QLineEdit::Password);
    ui->centralwidget->setFocusPolicy (Qt::ClickFocus);

    auto updateClrBtnVis = [this](){
        bool Ashow = !ui->account->text().isEmpty() && ui->account->hasFocus();
        bool Pshow = !ui->password->text().isEmpty() && ui->password->hasFocus();
        ui->AclrButton->setVisible(Ashow);
        ui->PclrButton->setVisible(Pshow);
    };

    connect(ui->account, &QLineEdit::textChanged, this, updateClrBtnVis);
    connect(ui->password, &QLineEdit::textChanged, this, updateClrBtnVis);
    connect(ui->AclrButton, &QPushButton::clicked, this, [this](){
        ui->account->clear();
        ui->account->setFocus();
    });
    connect(ui->PclrButton, &QPushButton::clicked, this, [this](){
        ui->password->clear();
        ui->password->setFocus();
    });

    // 背景动画
    QLabel* labBg = new QLabel(this);
    labBg->setScaledContents(true);
    labBg->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    labBg->setStyleSheet("QLabel{border:none;background:transparent;}");
    labBg->lower();

    QMovie* movie = new QMovie(this);
    movie->setFileName(":/gif/firework");
    movie->setCacheMode(QMovie::CacheAll);
    movie->setSpeed(50);
    labBg->setMovie(movie);
    movie->start();
    labBg->setGeometry(this->rect());

    // ---- 历史账号下拉栏（内嵌式，窗口高度不变） ----
    ToggleButton* dropdownButton = new ToggleButton(this);
    ui->dropdownHolder->layout()->addWidget(dropdownButton);
    ui->dropdownHolder->layout()->setAlignment(dropdownButton,Qt::AlignCenter);

    auto* dropdownFrame = new DropdownPopup(this);
    ui->InfoLayout->insertWidget(2, dropdownFrame, 0, Qt::AlignHCenter);

    dropdownFrame->addItem("", "1000000001");
    dropdownFrame->addItem("", "251051");
    dropdownFrame->addItem("", "25162");
    dropdownFrame->addItem("", "251051");
    dropdownFrame->addItem("", "25162");
    dropdownFrame->addItem("", "251051");

    // 展开即hide 密码框、复选框，dropdownFrame 高度精确等于被隐藏区域的实际高度
    // 这样 InfoLayout + spacer_3 + OptLayout + spacer_4 总高不变，登录按钮及下方纹丝不动
    auto expandDropdown = [this, dropdownFrame, dropdownButton]() {
        if (dropdownFrame->isVisible()) return;

        // 改frame的高度
        int h = ui->passwordHolder->height();
        if (auto* cb = ui->OptLayout->itemAt(0)->widget()){
            h += 20;        // 微调一手
            h += cb->height();
        }

        setUpdatesEnabled(false);
        ui->passwordHolder->hide();
        ui->autoLogin->hide();
        ui->rememberPwd->hide();
        dropdownFrame->setFixedHeight(h);
        dropdownFrame->popup();
        dropdownButton->openedDo();
        ui->MainLayout->activate();
        setUpdatesEnabled(true);
    };

    auto collapseDropdown = [this, dropdownFrame, dropdownButton]() {
        if (!dropdownFrame->isVisible()) return;
        setUpdatesEnabled(false);
        dropdownFrame->closePopup();
        dropdownButton->closedDo();
        ui->passwordHolder->show();
        ui->autoLogin->show();
        ui->rememberPwd->show();
        setUpdatesEnabled(true);
    };

    connect(dropdownButton, &QPushButton::clicked, this,
            [expandDropdown, collapseDropdown, dropdownFrame]() {
                if (dropdownFrame->isVisible()){
                    collapseDropdown();
                }else{
                    expandDropdown();
                }
            });

    connect(dropdownFrame, &DropdownPopup::itemClicked, this,
            [collapseDropdown, this](const QString& account) {
                ui->account->setText(account);
                collapseDropdown();
            });

    // 注册账号相关
    ui->registerButton->setCursor(Qt::PointingHandCursor);


    // 两个复选框
    QPalette pal = ui->autoLogin->palette();
    pal.setColor(QPalette::WindowText, QColor(0x80, 0x80, 0x80));
    ui->autoLogin->setPalette(pal);
    ui->rememberPwd->setPalette(pal);


    connect(ui->autoLogin,&QCheckBox::checkStateChanged,ui->rememberPwd,[this](Qt::CheckState state){
        if(state == Qt::CheckState::Checked){
            ui->rememberPwd->setChecked(true);
        }
    });

}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::mousePressEvent(QMouseEvent *event)
{
    if(ui->menuWidget->geometry().contains(event->pos()))
    {
        m_bDragging = true;
        m_dragStartPos = event->globalPosition().toPoint() - this->frameGeometry().topLeft();
    }
}

void LoginWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDragging)
        this->move(event->globalPosition().toPoint() - m_dragStartPos);
}

void LoginWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_bDragging = false;
}

void LoginWindow::on_shutdownButton_clicked() { this->close(); }
void LoginWindow::on_minimizeButton_clicked() { this->showMinimized(); }

bool LoginWindow::isCapsLockOn()
{
#ifdef Q_OS_WIN
    return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
#else
    return false;
#endif
}

bool LoginWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(obj == ui->password)
    {
        if(event->type() == QEvent::FocusIn) {
            if(isCapsLockOn())
                ui->passwordHolder->setStyleSheet(R"(QWidget#passwordHolder{
                    background-color:#ffffff;border:2px solid green;
                    border-radius:10px;})");
        }
        else if(event->type() == QEvent::FocusOut) {
            ui->passwordHolder->setStyleSheet(R"(QWidget#passwordHolder{
                background-color:#ffffff;border:none;border-radius:10px;})");
        }
        else if(event->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(event);
            if(ke->key() == Qt::Key_CapsLock) {
                if(isCapsLockOn())
                    ui->passwordHolder->setStyleSheet(R"(QWidget#passwordHolder{
                        background-color:#ffffff;border:2px solid green;border-radius:10px;})");
                else
                    ui->passwordHolder->setStyleSheet(R"(QWidget#passwordHolder{
                        background-color:#ffffff;border:none;border-radius:10px;})");
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void LoginWindow::on_registerButton_clicked()
{
    Reg* r = new Reg(this);
    // 防止多次点击注册按钮
    ui->registerButton->setEnabled(false);
    // 接收注册成功信号
    connect(r, &Reg::regOk, this, [this](const QString& account){
        qDebug() << "[LW1] LoginWindow regOk slot enter";
        ui->account->setText(account);
        qDebug() << "[LW2] LoginWindow regOk slot exit";
    });
    // 防止跨线程野指针
    connect(r,&Reg::regOk,r,&Reg::close);
    r->setAttribute(Qt::WA_DeleteOnClose);

    connect(r, &Reg::destroyed, this, [this](){
        qDebug() << "[LW3] Reg destroyed";
        ui->registerButton->setEnabled(true);
        qDebug() << "[LW4] Reg destroyed handler exit";
    });
    r->show();
}


void LoginWindow::on_loginButton_clicked()
{

    m_controller.login(ui->account->text(), ui->password->text());

}


#include "loginFailed.h"
#include "ui_loginfailed.h"

LoginFailed::LoginFailed(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginFailed)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window
                   | Qt::FramelessWindowHint
                   | Qt::WindowSystemMenuHint
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint);
    ui->shutdownButton->setText("\u00d7");

    this->setStyleSheet(R"(background-color: white;)");
    ui->reasonLabel->setAlignment(Qt::AlignCenter);

    if(parent){
        this->adjustSize();
        QRect parentRect = parent->geometry();
        QRect dialogRect = this->frameGeometry();
        dialogRect.moveCenter(parentRect.center());
        this->move(dialogRect.topLeft());
    }
}

LoginFailed::~LoginFailed()
{
    delete ui;
}

void LoginFailed::setReason(QString reason){
    ui->reasonLabel->setText(ui->reasonLabel->text()+" "+ reason);
}

void LoginFailed::mousePressEvent(QMouseEvent *event)
{
    if(ui->menuWidget->geometry().contains(event->pos()))
    {
        m_bDragging = true;
        m_dragStartPos = event->globalPosition().toPoint() - this->frameGeometry().topLeft();
    }
}

void LoginFailed::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDragging)
        this->move(event->globalPosition().toPoint() - m_dragStartPos);
}

void LoginFailed::mouseReleaseEvent(QMouseEvent *event)
{
    m_bDragging = false;
}

void LoginFailed::on_shutdownButton_clicked()
{
    this->close();
}


void LoginFailed::on_confirmButton_clicked()
{
    this->close();
}


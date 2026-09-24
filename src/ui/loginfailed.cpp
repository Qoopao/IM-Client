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
}

LoginFailed::~LoginFailed()
{
    delete ui;
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


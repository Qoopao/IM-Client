#include "regfailed.h"
#include "ui_regfailed.h"

RegFailed::RegFailed(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegFailed)
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

RegFailed::~RegFailed()
{
    delete ui;
}

void RegFailed::mousePressEvent(QMouseEvent *event)
{
    if(ui->menuWidget->geometry().contains(event->pos()))
    {
        m_bDragging = true;
        m_dragStartPos = event->globalPosition().toPoint() - this->frameGeometry().topLeft();
    }
}

void RegFailed::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDragging)
        this->move(event->globalPosition().toPoint() - m_dragStartPos);
}

void RegFailed::mouseReleaseEvent(QMouseEvent *event)
{
    m_bDragging = false;
}

void RegFailed::on_shutdownButton_clicked()
{
    this->close();
}


void RegFailed::on_confirmButton_clicked()
{
    this->close();
}


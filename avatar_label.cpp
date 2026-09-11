#include "avatar_label.h"
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

CircularAvatarLabel::CircularAvatarLabel(int diameter, QWidget* parent) : QLabel(parent){

    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setScaledContents(true);
    setFixedSize(110,110);

    setFixedSize(diameter,diameter);
    setStyleSheet(QString("QLabel { background-color: #e0e0e0;border-radius: %1px; }").arg(diameter / 2));

}

void CircularAvatarLabel::setAvatar(const QPixmap& avatar){
    this->m_avatar = avatar;
    update();
}

void CircularAvatarLabel::paintEvent(QPaintEvent* event){
    QLabel::paintEvent(event);

    if(m_avatar.isNull()){
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addEllipse(rect());
    painter.setClipPath(path);

    QPixmap scaled = m_avatar.scaled(
        size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    int x = (width()  - scaled.width())  / 2;
    int y = (height() - scaled.height()) / 2;
    painter.drawPixmap(x, y, scaled);
}
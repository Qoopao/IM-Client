#ifndef AVATAR_LABEL_H
#define AVATAR_LABEL_H

#include <QLabel>
#include <QPixmap>

class CircularAvatarLabel: public QLabel{
    Q_OBJECT
public:
    explicit CircularAvatarLabel(int diameter=100, QWidget* parent=nullptr);
    void setAvatar(const QPixmap& avatar);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_avatar;
};

#endif // AVATAR_LABEL_H

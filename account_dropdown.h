#ifndef ACCOUNT_DROPDOWN_H
#define ACCOUNT_DROPDOWN_H

#include <QFrame>
#include <QPointer>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPropertyAnimation>
#include <QPushButton>

// 下拉的按钮
class ToggleButton : public QPushButton{
    Q_OBJECT
public:
    ToggleButton(QWidget* parent = nullptr);
    bool getIsopen(){return m_isopen;}
    void openedDo();
    void closedDo();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_isopen = false;
};

// 内嵌式下拉列表 frame
class DropdownPopup : public QFrame
{
    Q_OBJECT
public:
    explicit DropdownPopup(QWidget* parent = nullptr);

    void addItem(const QString& avatarPath, const QString& text);
    void clearItems();

public slots:
    void popup();
    void closePopup();
    void toggle();

signals:
    void itemClicked(const QString& text);

private:
    QVBoxLayout* m_rootLayout = nullptr;
    QFrame*      m_body = nullptr;
    QVBoxLayout* m_bodyLayout = nullptr;
    QScrollArea* m_itemScrollArea = nullptr;
    QWidget* m_itemWidget = nullptr;
    QVBoxLayout* m_itemLayout = nullptr;
};

//  列表项
class PopupItem : public QFrame
{
    Q_OBJECT
public:
    PopupItem(const QString& avatarPath, const QString& text, QWidget* parent = nullptr);
    QPixmap toCircularPixmap(const QPixmap& src, int size);

signals:
    void clicked(const QString& text);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* m_avatar = nullptr;
    QLabel* m_label  = nullptr;
    bool    m_hover  = false;
};

#endif // ACCOUNT_DROPDOWN_H

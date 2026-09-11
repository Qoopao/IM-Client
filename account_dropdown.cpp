#include "account_dropdown.h"

#include <QApplication>
#include <QDebug>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

// ==================== ToggleButton ====================
ToggleButton::ToggleButton(QWidget* parent) : QPushButton(parent){
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    setFixedSize(20,20);
    setStyleSheet(R"(QPushButton{
                            border:none;
                            padding: 0px;
                            background:transparent;
                            border-radius: 5px;
                            }

                            QPushButton:hover{
                            background:#1A000000;
                            })");
}

void ToggleButton::openedDo(){ m_isopen = true; update(); }
void ToggleButton::closedDo(){ m_isopen = false; update(); }

void ToggleButton::paintEvent(QPaintEvent* event){
    QPushButton::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRectF r = rect();
    QPointF center = r.center();
    int size = 6;
    QPolygon polyLine;
    if(!m_isopen) {
        polyLine << QPoint(center.x()-size, center.y()-size/2)
        << QPoint(center.x(),       center.y()+size/2)
        << QPoint(center.x()+size, center.y()-size/2);
    } else {
        polyLine << QPoint(center.x()-size, center.y()+size/2)
        << QPoint(center.x(),       center.y()-size/2)
        << QPoint(center.x()+size, center.y()+size/2);
    }
    painter.setPen(QPen(QColor(0x2E,0x2E,0x2E), 1));
    painter.drawPolyline(polyLine);
}


// ==================== DropdownPopup ====================

DropdownPopup::DropdownPopup(QWidget* parent) : QFrame(parent)
{
    setFixedWidth(250);
    setFixedHeight(0);
    setStyleSheet("QFrame{background:transparent;border:none;}");
    hide();

    // 先放一个布局，用来放一个空白的frame
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // 创建最底层的frame并把他放在先前创建的布局里面
    m_body = new QFrame(this);
    m_body->setObjectName("dropdownBody");
    m_body->setStyleSheet("QFrame#dropdownBody{background:#FFFFFF;border-radius:8px;}");
    m_rootLayout->addWidget(m_body);

    // 然后需要在这个frame里面加一个布局，用来存放滚动区域
    m_bodyLayout = new QVBoxLayout(m_body);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(0);

    // 往这个布局加入滚动区域
    m_itemScrollArea = new QScrollArea(m_body);
    m_itemScrollArea->viewport()->setStyleSheet("background:transparent;");
    m_bodyLayout->addWidget(m_itemScrollArea);

    // ---------------树到这里快结束了----------------------
    // 创建一个用于存放item的widget并将它交给ScrollArea
    // 要往itemWidget里面加东西就要放布局
    m_itemWidget = new QWidget(nullptr);        // 不属于任何一个容器
    m_itemLayout = new QVBoxLayout(m_itemWidget);
    m_itemLayout->setContentsMargins(4, 3, 4, 3);
    m_itemLayout->setSpacing(1);
    m_itemScrollArea->setWidgetResizable(true);
    m_itemScrollArea->setWidget(m_itemWidget);      // 交给ScrollArea

}

void DropdownPopup::addItem(const QString& avatarPath, const QString& text)
{
    auto* item = new PopupItem(avatarPath, text, m_itemWidget);
    connect(item, &PopupItem::clicked, this, &DropdownPopup::itemClicked);
    m_itemLayout->addWidget(item);
}

void DropdownPopup::clearItems()
{
    QLayoutItem* child;
    while ((child = m_itemLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
}

void DropdownPopup::popup()
{
    if (isVisible()) return;
    show();
}

void DropdownPopup::closePopup()
{
    if (!isVisible()) return;
    hide();
}

void DropdownPopup::toggle()
{
    if (isVisible()) closePopup();
    else popup();
}


// ==================== PopupItem ====================

QPixmap PopupItem::toCircularPixmap(const QPixmap& src, int size)
{
    QPixmap dst(size, size);
    dst.fill(Qt::transparent);
    QPainter p(&dst);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    p.setClipPath(path);
    p.drawPixmap(0, 0, src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    return dst;
}

PopupItem::PopupItem(const QString& avatarPath, const QString& text, QWidget* parent)
    : QFrame(parent)
{
    setFixedHeight(30);
    setCursor(Qt::PointingHandCursor);
    setStyleSheet("QFrame{background:transparent;border:none;}");

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(4, 1, 4, 1);
    lay->setSpacing(6);

    m_avatar = new QLabel(this);
    m_avatar->setFixedSize(24, 24);

    QPixmap pm(avatarPath);
    if (pm.isNull()) {
        pm = QPixmap(24, 24);
        pm.fill(Qt::transparent);
        QPainter pp(&pm);
        pp.setRenderHint(QPainter::Antialiasing);
        pp.setPen(Qt::NoPen);
        pp.setBrush(QColor(0x4E, 0x8D, 0xF5, 128));
        pp.drawEllipse(0, 0, 24, 24);
        pp.setPen(Qt::white);
        QFont f = pp.font();
        f.setPixelSize(12);
        f.setBold(true);
        pp.setFont(f);
        pp.drawText(pm.rect(), Qt::AlignCenter, text.left(1));
    }
    m_avatar->setPixmap(toCircularPixmap(pm, 24));

    m_label = new QLabel(text, this);
    m_label->setStyleSheet("color:#333333;font-size:12px;background:transparent;");

    lay->addWidget(m_avatar);
    lay->addWidget(m_label);
    lay->addStretch();
}

void PopupItem::enterEvent(QEnterEvent*) { m_hover = true; update(); }
void PopupItem::leaveEvent(QEvent*)    { m_hover = false; update(); }

void PopupItem::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->pos()))
        emit clicked(m_label->text());
    QFrame::mouseReleaseEvent(e);
}

void PopupItem::paintEvent(QPaintEvent*)
{
    // 写鼠标悬停特效
    if (!m_hover) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 12));
    p.drawRoundedRect(rect(), 5, 5);
}

#include "customclickableitem.h"
#include <QGraphicsSceneMouseEvent>

CustomClickableItem::CustomClickableItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QObject(nullptr), QGraphicsPixmapItem(pixmap, parent)
{
    // setAcceptHoverEvents(true); // 如果需要悬浮效果
}

void CustomClickableItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QGraphicsPixmapItem::mousePressEvent(event);
}

#ifndef CUSTOMCLICKABLEITEM_H
#define CUSTOMCLICKABLEITEM_H

#include <QObject>
#include <QGraphicsPixmapItem>

class CustomClickableItem : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    explicit CustomClickableItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    // void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override; // 可选：用于悬浮效果
    // void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;  // 可选：用于悬浮效果
};

#endif // CUSTOMCLICKABLEITEM_H

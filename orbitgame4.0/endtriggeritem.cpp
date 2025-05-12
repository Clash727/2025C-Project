#include "endtriggeritem.h"

EndTriggerItem::EndTriggerItem(qreal x, qreal y, qreal radius, QGraphicsItem *parent)
    : QGraphicsEllipseItem(0, 0, radius * 2, radius * 2, parent)
{
    // QGraphicsEllipseItem 的构造函数定义了相对于其自身 (0,0) 点的椭圆边界矩形
    // 我们将圆心放在 (0,0)，所以边界矩形的左上角是 (-radius, -radius)
    // 但为了使用 setPos(desired_center_x, desired_center_y) 来定位圆心，
    // 我们将边界矩形的左上角设为 (0,0)，然后通过 setPos 调整。
    // 或者，更常见的是，将椭圆的 rect 设置为以 (0,0) 为中心，然后用 setPos 设置中心点。
    // 这里我们采用第一种方式，setRect(0,0,diameter,diameter) 然后用 setPos(centerX - radius, centerY - radius)

    setPos(x - radius, y - radius); // 设置的是左上角的位置，使得圆心在 (x,y)
    setBrush(Qt::yellow);        // 设置为黄色
    setPen(Qt::NoPen);           // 无边框
    // Z值可以在 GameScene 中添加时设置，或者在这里设置一个默认值
    // setZValue(0.7);
}

// 如果选择将椭圆的 rect 设置为以 (0,0) 为中心：
// EndTriggerItem::EndTriggerItem(const QPointF &centerPos, qreal radius, QGraphicsItem *parent)
//     : QGraphicsEllipseItem(-radius, -radius, radius * 2, radius * 2, parent)
// {
//     setPos(centerPos);
//     setBrush(Qt::yellow);
//     setPen(Qt::NoPen);
// }

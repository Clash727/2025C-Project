#ifndef ENDTRIGGERITEM_H
#define ENDTRIGGERITEM_H

#include <QGraphicsEllipseItem>
#include <QBrush>
#include <QPen>

const qreal DEFAULT_END_POINT_RADIUS = 5.0; // 默认通关点半径

class EndTriggerItem : public QGraphicsEllipseItem
{
public:
    // 构造函数，允许指定位置、半径和父项
    explicit EndTriggerItem(qreal x, qreal y, qreal radius = DEFAULT_END_POINT_RADIUS, QGraphicsItem *parent = nullptr);
    // explicit EndTriggerItem(const QPointF &centerPos, qreal radius = DEFAULT_END_POINT_RADIUS, QGraphicsItem *parent = nullptr);


    // 可选：如果需要自定义类型以便于识别
    // enum { Type = UserType + 1 }; // UserType + x (x > 0)
    // int type() const override { return Type; }
};

#endif // ENDTRIGGERITEM_H

// obstacleitem.h
#ifndef OBSTACLEITEM_H
#define OBSTACLEITEM_H

#include <QObject>
#include <QGraphicsEllipseItem>
#include <QBrush>
#include <QPen>
#include <QPointF>
#include <QtMath>
#include <QRandomGenerator>

// 障碍物常量
const qreal DEFAULT_OBSTACLE_RADIUS = 7.0; // 障碍物可以比收集品稍大一点
const qreal OBSTACLE_ORBIT_PADDING = 3.0;  // 与收集品使用类似的内外圈偏移逻辑

class ObstacleItem : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT

public:
    explicit ObstacleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                          qreal radius = DEFAULT_OBSTACLE_RADIUS,
                          QGraphicsItem *parent = nullptr);
    ~ObstacleItem();

    void processHit(); // 处理被撞击
    bool isHit() const;

    int getAssociatedTrackIndex() const;
    qreal getAngleOnTrack() const;

    void updateVisualPosition(const QPointF& trackCenter, qreal trackRadius);
    void setVisibleState(bool visible);

    // 设置障碍物的轨道偏移（正为外圈，负为内圈）
    void setOrbitOffset(qreal offset);
    qreal getOrbitOffset() const;

signals:
    void hitSignal(ObstacleItem* item); // 被撞击时发出的信号

private:
    int m_associatedTrackIndex;
    qreal m_angleOnTrack;
    bool m_isHit; // 是否已被撞击
    qreal m_orbitOffset;

    QBrush m_brush;
    QPen m_pen;
    qreal m_radius;
};

#endif // OBSTACLEITEM_H

// obstacleitem.h
#ifndef OBSTACLEITEM_H
#define OBSTACLEITEM_H

#include <QObject>
#include <QGraphicsPixmapItem> // 修改: 使用 PixmapItem
#include <QPointF>
#include <QtMath>
#include <QRandomGenerator>
#include <QPixmap>             // 新增: 用于加载图片

// 障碍物常量
// const qreal DEFAULT_OBSTACLE_RADIUS = 7.0; // 旧的，基于椭圆半径
const qreal DEFAULT_OBSTACLE_TARGET_SIZE = 14.0; // 新的：期望障碍物贴图的显示直径或宽度 (通常比收集品大一点)
const qreal OBSTACLE_ORBIT_PADDING = 3.0;

class ObstacleItem : public QObject, public QGraphicsPixmapItem // 修改: 继承 QGraphicsPixmapItem
{
    Q_OBJECT

public:
    // 构造函数修改：不再需要 radius 来画椭圆
    explicit ObstacleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
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
    // QBrush m_brush; // 移除
    // QPen m_pen;     // 移除
    // qreal m_radius;  // 移除
};

#endif // OBSTACLEITEM_H

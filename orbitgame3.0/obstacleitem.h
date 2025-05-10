#ifndef OBSTACLEITEM_H
#define OBSTACLEITEM_H

#include <QObject>
#include <QGraphicsPixmapItem>
#include <QPointF>
#include <QtMath>
#include <QRandomGenerator> // 虽然构造函数不再随机设置，但保留以防未来需要
#include <QPixmap>

// 障碍物常量
const qreal DEFAULT_OBSTACLE_TARGET_SIZE = 25.0;
// OBSTACLE_ORBIT_PADDING 也许不再需要，或者可以设为0
const qreal OBSTACLE_ORBIT_PADDING = 0.0; // 或者根据您的视觉需求调整

class ObstacleItem : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT

public:
    explicit ObstacleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                          QGraphicsItem *parent = nullptr);
    ~ObstacleItem();

    void processHit();
    bool isHit() const;

    int getAssociatedTrackIndex() const;
    qreal getAngleOnTrack() const;

    void updateVisualPosition(const QPointF& trackCenter, qreal trackRadius);
    void setVisibleState(bool visible);

    void setOrbitOffset(qreal offset); // 用于从外部设置偏移
    qreal getOrbitOffset() const;

signals:
    void hitSignal(ObstacleItem* item);

private:
    int m_associatedTrackIndex;
    qreal m_angleOnTrack;
    bool m_isHit;
    qreal m_orbitOffset; // 将由 GameScene 根据 JSON 数据设置
};

#endif // OBSTACLEITEM_H

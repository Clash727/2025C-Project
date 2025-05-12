#ifndef OBSTACLEITEM_H
#define OBSTACLEITEM_H

#include <QObject>
#include <QGraphicsPixmapItem>
#include <QPointF>
#include <QtMath>
#include <QRandomGenerator>
#include <QPixmap>
#include <QSoundEffect> // <--- 添加 QSoundEffect 头文件

// 障碍物常量
const qreal DEFAULT_OBSTACLE_TARGET_SIZE = 25.0;
const qreal OBSTACLE_ORBIT_PADDING = 0.0;

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

    void setOrbitOffset(qreal offset);
    qreal getOrbitOffset() const;

signals:
    void hitSignal(ObstacleItem* item);

private:
    int m_associatedTrackIndex;
    qreal m_angleOnTrack;
    bool m_isHit;
    qreal m_orbitOffset;
    QSoundEffect m_hitSoundEffect; // <--- 添加 QSoundEffect 成员变量
};

#endif // OBSTACLEITEM_H

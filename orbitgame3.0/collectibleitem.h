#ifndef COLLECTIBLEITEM_H
#define COLLECTIBLEITEM_H

#include <QObject>
#include <QGraphicsPixmapItem>
#include <QPointF>
#include <QtMath>
#include <QRandomGenerator> // 虽然构造函数不再随机设置，但保留以防未来需要
#include <QPixmap>

// 收集品常量
const qreal DEFAULT_COLLECTIBLE_TARGET_SIZE = 25.0;
const int DEFAULT_SCORE_PER_COLLECTIBLE = 1;
// COLLECTIBLE_ORBIT_PADDING 也许不再需要，或者可以设为0，因为精确偏移由JSON控制
const qreal COLLECTIBLE_ORBIT_PADDING = 0.0;

class CollectibleItem : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT

public:
    explicit CollectibleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                             QGraphicsItem *parent = nullptr);
    ~CollectibleItem();

    void collect();
    bool isCollected() const;
    int getScoreValue() const;

    int getAssociatedTrackIndex() const;
    qreal getAngleOnTrack() const;

    void updateVisualPosition(const QPointF& trackCenter, qreal trackRadius);
    void setVisibleState(bool visible);

    void setOrbitOffset(qreal offset); // 用于从外部设置偏移
    qreal getOrbitOffset() const;

signals:
    void collectedSignal(CollectibleItem* item);

private:
    int m_associatedTrackIndex;
    qreal m_angleOnTrack;
    bool m_isCollected;
    int m_scoreValue;
    qreal m_orbitOffset; // 将由 GameScene 根据 JSON 数据设置
};

#endif // COLLECTIBLEITEM_H

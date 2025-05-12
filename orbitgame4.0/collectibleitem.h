#ifndef COLLECTIBLEITEM_H
#define COLLECTIBLEITEM_H

#include <QObject>
#include <QGraphicsPixmapItem>
#include <QPointF>
#include <QtMath>
#include <QRandomGenerator>
#include <QPixmap>
#include <QSoundEffect> // <--- 添加 QSoundEffect 头文件

// 收集品常量
const qreal DEFAULT_COLLECTIBLE_TARGET_SIZE = 25.0;
const int DEFAULT_SCORE_PER_COLLECTIBLE = 1;
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

    void setOrbitOffset(qreal offset);
    qreal getOrbitOffset() const;

signals:
    void collectedSignal(CollectibleItem* item);

private:
    int m_associatedTrackIndex;
    qreal m_angleOnTrack;
    bool m_isCollected;
    int m_scoreValue;
    qreal m_orbitOffset;
    QSoundEffect m_collectSoundEffect; // <--- 添加 QSoundEffect 成员变量
};

#endif // COLLECTIBLEITEM_H

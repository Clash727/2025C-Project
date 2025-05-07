// collectibleitem.h
#ifndef COLLECTIBLEITEM_H
#define COLLECTIBLEITEM_H

#include <QObject>
#include <QGraphicsEllipseItem>
#include <QBrush>
#include <QPen>
#include <QPointF>
#include <QtMath> // 用于 qCos, qSin
#include <QRandomGenerator> // 用于随机决定内外侧

// 收集品常量
const qreal DEFAULT_COLLECTIBLE_RADIUS = 6.0;
const int DEFAULT_SCORE_PER_COLLECTIBLE = 1;
// 收集品也使用与球类似的内外圈偏移逻辑
const qreal COLLECTIBLE_ORBIT_PADDING = 3.0; // 与 GameScene 中的 ORBIT_PADDING 类似，但可以独立设置

class CollectibleItem : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT

public:
    // 构造函数：关联的轨道索引，在轨道上的角度（弧度），半径，父QGraphicsItem
    explicit CollectibleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                             qreal radius = DEFAULT_COLLECTIBLE_RADIUS,
                             QGraphicsItem *parent = nullptr);
    ~CollectibleItem();

    void collect();
    bool isCollected() const;
    int getScoreValue() const;

    int getAssociatedTrackIndex() const;
    qreal getAngleOnTrack() const;

    // 更新收集品在场景中的视觉位置，现在需要考虑轨道偏移
    void updateVisualPosition(const QPointF& trackCenter, qreal trackRadius);
    void setVisibleState(bool visible);

    // 设置收集品的轨道偏移（正为外圈，负为内圈）
    void setOrbitOffset(qreal offset);
    qreal getOrbitOffset() const;


signals:
    void collectedSignal(CollectibleItem* item);

private:
    int m_associatedTrackIndex;
    qreal m_angleOnTrack;
    bool m_isCollected;
    int m_scoreValue;
    qreal m_orbitOffset; // 新增：收集品的轨道偏移量

    QBrush m_brush;
    QPen m_pen;
    qreal m_radius;
};

#endif // COLLECTIBLEITEM_H

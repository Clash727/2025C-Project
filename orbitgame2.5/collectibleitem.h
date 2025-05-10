// collectibleitem.h
#ifndef COLLECTIBLEITEM_H
#define COLLECTIBLEITEM_H

#include <QObject>
#include <QGraphicsPixmapItem> // 修改: 使用 PixmapItem
#include <QPointF>
#include <QtMath>
#include <QRandomGenerator>
#include <QPixmap>             // 新增: 用于加载图片

// 收集品常量
// const qreal DEFAULT_COLLECTIBLE_RADIUS = 6.0; // 旧的，基于椭圆半径
const qreal DEFAULT_COLLECTIBLE_TARGET_SIZE = 20.0; // 新的：期望收集品贴图的显示直径或宽度
const int DEFAULT_SCORE_PER_COLLECTIBLE = 1;
const qreal COLLECTIBLE_ORBIT_PADDING = 0.0;

class CollectibleItem : public QObject, public QGraphicsPixmapItem // 修改: 继承 QGraphicsPixmapItem
{
    Q_OBJECT

public:
    // 构造函数修改：不再需要 radius 来画椭圆
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
    // QBrush m_brush; // 移除
    // QPen m_pen;     // 移除
    // qreal m_radius;  // 移除 (或如果仍用于非绘图逻辑，则保留并重命名)
};

#endif // COLLECTIBLEITEM_H

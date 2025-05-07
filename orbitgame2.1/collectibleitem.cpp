// collectibleitem.cpp
#include "collectibleitem.h"
#include <QDebug> //确保已包含

CollectibleItem::CollectibleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                                 qreal radius, QGraphicsItem *parent)
    : QObject(nullptr),
    QGraphicsEllipseItem(-radius, -radius, 2 * radius, 2 * radius, parent),
    m_associatedTrackIndex(associatedTrackIndex), // 赋值点
    m_angleOnTrack(angleOnTrackRadians),
    m_isCollected(false),
    m_scoreValue(DEFAULT_SCORE_PER_COLLECTIBLE),
    m_orbitOffset(0), // 默认为0，即在轨道中心线上，将在 GameScene 中随机设置
    m_brush(Qt::yellow),
    m_pen(Qt::darkYellow, 1),
    m_radius(radius)
{
    // ***** DEBUGGING CODE START *****
    qDebug() << "[CollectibleItem Constructor] this:" << static_cast<void*>(this)
             << "associatedTrackIndex received:" << associatedTrackIndex
             << "angle (rad):" << angleOnTrackRadians;
    // ***** DEBUGGING CODE END *****

    setBrush(m_brush);
    setPen(m_pen);
    setZValue(0.5); // Z值，确保它在轨道之上，可能在球之下或同层
    setVisible(false);

    // 随机决定是在内圈还是外圈
    // 这个逻辑也可以放在 GameScene 创建 CollectibleItem 时
    if (QRandomGenerator::global()->bounded(2) == 0) { // 0 代表内圈
        m_orbitOffset = -(m_radius + COLLECTIBLE_ORBIT_PADDING);
    } else { // 1 代表外圈
        m_orbitOffset = (m_radius + COLLECTIBLE_ORBIT_PADDING);
    }
}

CollectibleItem::~CollectibleItem()
{
    // qDebug() << "轨道 " << m_associatedTrackIndex << " 角度 " << m_angleOnTrack << " 的收集品已删除。";
}

void CollectibleItem::collect()
{
    if (!m_isCollected) {
        m_isCollected = true;
        setVisible(false);
        emit collectedSignal(this);
    }
}

bool CollectibleItem::isCollected() const
{
    return m_isCollected;
}

int CollectibleItem::getScoreValue() const
{
    return m_scoreValue;
}

int CollectibleItem::getAssociatedTrackIndex() const
{
    return m_associatedTrackIndex;
}

qreal CollectibleItem::getAngleOnTrack() const
{
    return m_angleOnTrack;
}

void CollectibleItem::setOrbitOffset(qreal offset)
{
    m_orbitOffset = offset;
}

qreal CollectibleItem::getOrbitOffset() const
{
    return m_orbitOffset;
}


void CollectibleItem::updateVisualPosition(const QPointF& trackCenter, qreal trackRadius)
{
    // 收集品的实际运动半径 = 轨道半径 + 收集品自身的内外圈偏移
    qreal effectiveCollectibleRadiusOnTrack = trackRadius + m_orbitOffset;
    // 防止收集品的运动半径小于收集品自身半径（不太可能发生，因为偏移量通常包含半径）
    if (effectiveCollectibleRadiusOnTrack < m_radius && m_orbitOffset < 0) { // 内圈特殊处理
        effectiveCollectibleRadiusOnTrack = m_radius; // 保证至少能放下自己
    } else if (effectiveCollectibleRadiusOnTrack < 0 && m_orbitOffset > 0) { // 外圈但半径为负，不太可能
        effectiveCollectibleRadiusOnTrack = m_radius;
    }


    qreal x = trackCenter.x() + effectiveCollectibleRadiusOnTrack * qCos(m_angleOnTrack);
    qreal y = trackCenter.y() + effectiveCollectibleRadiusOnTrack * qSin(m_angleOnTrack);
    setPos(x, y);
}

void CollectibleItem::setVisibleState(bool visible)
{
    setVisible(visible && !m_isCollected);
}

// obstacleitem.cpp
#include "obstacleitem.h"
#include <QDebug>

ObstacleItem::ObstacleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                           qreal radius, QGraphicsItem *parent)
    : QObject(nullptr),
    QGraphicsEllipseItem(-radius, -radius, 2 * radius, 2 * radius, parent),
    m_associatedTrackIndex(associatedTrackIndex),
    m_angleOnTrack(angleOnTrackRadians),
    m_isHit(false),
    m_orbitOffset(0),
    m_brush(Qt::red), // 障碍物使用红色
    m_pen(Qt::darkRed, 1),
    m_radius(radius)
{
    qDebug() << "[ObstacleItem Constructor] this:" << static_cast<void*>(this)
    << "associatedTrackIndex received:" << associatedTrackIndex
    << "angle (rad):" << angleOnTrackRadians;

    setBrush(m_brush);
    setPen(m_pen);
    setZValue(0.6); // Z值，确保它在轨道之上，略高于收集品 (收集品是0.5)
    setVisible(false); // 初始不可见，由GameScene控制

    // 随机决定是在内圈还是外圈 (与CollectibleItem逻辑相同)
    if (QRandomGenerator::global()->bounded(2) == 0) { // 0 代表内圈
        m_orbitOffset = -(m_radius + OBSTACLE_ORBIT_PADDING);
    } else { // 1 代表外圈
        m_orbitOffset = (m_radius + OBSTACLE_ORBIT_PADDING);
    }
}

ObstacleItem::~ObstacleItem()
{
    // qDebug() << "轨道 " << m_associatedTrackIndex << " 角度 " << m_angleOnTrack << " 的障碍物已删除。";
}

void ObstacleItem::processHit()
{
    if (!m_isHit) {
        m_isHit = true;
        setVisible(false); // 被撞击后消失
        emit hitSignal(this);
    }
}

bool ObstacleItem::isHit() const
{
    return m_isHit;
}

int ObstacleItem::getAssociatedTrackIndex() const
{
    return m_associatedTrackIndex;
}

qreal ObstacleItem::getAngleOnTrack() const
{
    return m_angleOnTrack;
}

void ObstacleItem::setOrbitOffset(qreal offset)
{
    m_orbitOffset = offset;
}

qreal ObstacleItem::getOrbitOffset() const
{
    return m_orbitOffset;
}

void ObstacleItem::updateVisualPosition(const QPointF& trackCenter, qreal trackRadius)
{
    // 障碍物的实际运动半径 = 轨道半径 + 障碍物自身的内外圈偏移
    qreal effectiveObstacleRadiusOnTrack = trackRadius + m_orbitOffset;
    if (effectiveObstacleRadiusOnTrack < m_radius && m_orbitOffset < 0) { // 内圈特殊处理
        effectiveObstacleRadiusOnTrack = m_radius;
    } else if (effectiveObstacleRadiusOnTrack < 0 && m_orbitOffset > 0) { // 外圈但半径为负
        effectiveObstacleRadiusOnTrack = m_radius;
    }

    qreal x = trackCenter.x() + effectiveObstacleRadiusOnTrack * qCos(m_angleOnTrack);
    qreal y = trackCenter.y() + effectiveObstacleRadiusOnTrack * qSin(m_angleOnTrack);
    setPos(x, y);
}

void ObstacleItem::setVisibleState(bool visible)
{
    setVisible(visible && !m_isHit); // 只有未被撞击且被要求显示时才显示
}

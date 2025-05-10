
// 文件: obstacleitem.cpp
#include "obstacleitem.h"
#include <QDebug>
#include <QPixmap>

ObstacleItem::ObstacleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                           QGraphicsItem *parent)
    : QObject(nullptr),
    QGraphicsPixmapItem(parent),
    m_associatedTrackIndex(associatedTrackIndex),
    m_angleOnTrack(angleOnTrackRadians),
    m_isHit(false),
    m_orbitOffset(0) // 初始化为0，GameScene会通过setOrbitOffset设置正确的值
{
    qDebug() << "[ObstacleItem PIXMAP Constructor] this:" << static_cast<void*>(this)
    << "associatedTrackIndex received:" << associatedTrackIndex
    << "angle (rad):" << angleOnTrackRadians;

    QPixmap originalPixmap(":/images/obstacle.png");

    if (originalPixmap.isNull()) {
        qWarning() << "Failed to load obstacle image ':/images/obstacle.png'. Using fallback blue square.";
        QPixmap fallbackPixmap(static_cast<int>(DEFAULT_OBSTACLE_TARGET_SIZE), static_cast<int>(DEFAULT_OBSTACLE_TARGET_SIZE));
        fallbackPixmap.fill(Qt::blue);
        setPixmap(fallbackPixmap);
    } else {
        QPixmap scaledPixmap = originalPixmap.scaled(static_cast<int>(DEFAULT_OBSTACLE_TARGET_SIZE),
                                                     static_cast<int>(DEFAULT_OBSTACLE_TARGET_SIZE),
                                                     Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation);
        setPixmap(scaledPixmap);
    }

    if (!pixmap().isNull()) {
        setTransformOriginPoint(pixmap().width() / 2.0, pixmap().height() / 2.0);
    }

    setZValue(0.6);
    setVisible(false);

    // 移除随机设置 m_orbitOffset 的逻辑
    // qreal effectiveRadiusForOffsetCalculation = DEFAULT_OBSTACLE_TARGET_SIZE / 2.0;
    // if (QRandomGenerator::global()->bounded(2) == 0) {
    //     m_orbitOffset = -(effectiveRadiusForOffsetCalculation + OBSTACLE_ORBIT_PADDING);
    // } else {
    //     m_orbitOffset = (effectiveRadiusForOffsetCalculation + OBSTACLE_ORBIT_PADDING);
    // }
    // m_orbitOffset 现在将由 GameScene 在创建此对象后，
    // 根据从JSON读取的 radialOffset 值通过 setOrbitOffset() 方法设置。
}

ObstacleItem::~ObstacleItem()
{
    // qDebug() << "ObstacleItem PIXMAP at track " << m_associatedTrackIndex << " angle " << m_angleOnTrack << " deleted.";
}

void ObstacleItem::processHit()
{
    if (!m_isHit) {
        m_isHit = true;
        setVisible(false);
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
    // 类似 CollectibleItem，通常在 GameScene 中统一调用 updateVisualPosition
}

qreal ObstacleItem::getOrbitOffset() const
{
    return m_orbitOffset;
}

void ObstacleItem::updateVisualPosition(const QPointF& trackCenter, qreal trackRadius)
{
    qreal effectiveOrbitRadius = trackRadius + m_orbitOffset; // 使用 m_orbitOffset

    qreal x_center = trackCenter.x() + effectiveOrbitRadius * qCos(m_angleOnTrack);
    qreal y_center = trackCenter.y() + effectiveOrbitRadius * qSin(m_angleOnTrack);

    if (!pixmap().isNull()) {
        setPos(x_center - pixmap().width() / 2.0,
               y_center - pixmap().height() / 2.0);
    } else {
        setPos(x_center, y_center);
    }
}

void ObstacleItem::setVisibleState(bool visible)
{
    setVisible(visible && !m_isHit);
}

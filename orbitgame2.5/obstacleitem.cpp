// obstacleitem.cpp
#include "obstacleitem.h"
#include <QDebug>
#include <QPixmap> // 确保包含

ObstacleItem::ObstacleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                           QGraphicsItem *parent) // 修改
    : QObject(nullptr),
    QGraphicsPixmapItem(parent), // 修改
    m_associatedTrackIndex(associatedTrackIndex),
    m_angleOnTrack(angleOnTrackRadians),
    m_isHit(false),
    m_orbitOffset(0)
{
    qDebug() << "[ObstacleItem PIXMAP Constructor] this:" << static_cast<void*>(this)
    << "associatedTrackIndex received:" << associatedTrackIndex
    << "angle (rad):" << angleOnTrackRadians;

    // 1. 加载贴图 - 确保路径 ":/images/obstacle.png" 在您的 .qrc 文件中是正确的
    QPixmap originalPixmap(":/images/obstacle.png");

    if (originalPixmap.isNull()) {
        qWarning() << "Failed to load obstacle image ':/images/obstacle.png'. Using fallback blue square.";
        QPixmap fallbackPixmap(static_cast<int>(DEFAULT_OBSTACLE_TARGET_SIZE), static_cast<int>(DEFAULT_OBSTACLE_TARGET_SIZE));
        fallbackPixmap.fill(Qt::blue); // 使用不同颜色以便区分
        setPixmap(fallbackPixmap);
    } else {
        // 2. （可选）缩放贴图
        QPixmap scaledPixmap = originalPixmap.scaled(static_cast<int>(DEFAULT_OBSTACLE_TARGET_SIZE),
                                                     static_cast<int>(DEFAULT_OBSTACLE_TARGET_SIZE),
                                                     Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation);
        setPixmap(scaledPixmap);
    }

    // 3. 设置变换原点
    if (!pixmap().isNull()) {
        setTransformOriginPoint(pixmap().width() / 2.0, pixmap().height() / 2.0);
    }

    setZValue(0.6); // 比收集品高一点
    setVisible(false);

    // 内外圈偏移逻辑
    qreal effectiveRadiusForOffsetCalculation = DEFAULT_OBSTACLE_TARGET_SIZE / 2.0;
    if (QRandomGenerator::global()->bounded(2) == 0) {
        m_orbitOffset = -(effectiveRadiusForOffsetCalculation + OBSTACLE_ORBIT_PADDING);
    } else {
        m_orbitOffset = (effectiveRadiusForOffsetCalculation + OBSTACLE_ORBIT_PADDING);
    }
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
}

qreal ObstacleItem::getOrbitOffset() const
{
    return m_orbitOffset;
}

void ObstacleItem::updateVisualPosition(const QPointF& trackCenter, qreal trackRadius)
{
    qreal effectiveOrbitRadius = trackRadius + m_orbitOffset;

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
    setVisible(visible && !m_isHit); // 只有未被撞击且被要求显示时才显示
}

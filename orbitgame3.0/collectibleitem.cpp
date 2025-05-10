
// 文件: collectibleitem.cpp
#include "collectibleitem.h"
#include <QDebug>
#include <QPixmap>

CollectibleItem::CollectibleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                                 QGraphicsItem *parent)
    : QObject(nullptr),
    QGraphicsPixmapItem(parent),
    m_associatedTrackIndex(associatedTrackIndex),
    m_angleOnTrack(angleOnTrackRadians),
    m_isCollected(false),
    m_scoreValue(DEFAULT_SCORE_PER_COLLECTIBLE),
    m_orbitOffset(0) // 初始化为0，GameScene会通过setOrbitOffset设置正确的值
{
    qDebug() << "[CollectibleItem PIXMAP Constructor] this:" << static_cast<void*>(this)
    << "associatedTrackIndex received:" << associatedTrackIndex
    << "angle (rad):" << angleOnTrackRadians;

    QPixmap originalPixmap(":/images/collectible.png");

    if (originalPixmap.isNull()) {
        qWarning() << "Failed to load collectible image ':/images/collectible.png'. Using fallback red square.";
        QPixmap fallbackPixmap(static_cast<int>(DEFAULT_COLLECTIBLE_TARGET_SIZE), static_cast<int>(DEFAULT_COLLECTIBLE_TARGET_SIZE));
        fallbackPixmap.fill(Qt::red);
        setPixmap(fallbackPixmap);
    } else {
        QPixmap scaledPixmap = originalPixmap.scaled(static_cast<int>(DEFAULT_COLLECTIBLE_TARGET_SIZE),
                                                     static_cast<int>(DEFAULT_COLLECTIBLE_TARGET_SIZE),
                                                     Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation);
        setPixmap(scaledPixmap);
    }

    if (!pixmap().isNull()) {
        setTransformOriginPoint(pixmap().width() / 2.0, pixmap().height() / 2.0);
    }

    setZValue(0.5);
    setVisible(false);

    // 移除随机设置 m_orbitOffset 的逻辑
    // qreal effectiveRadiusForOffsetCalculation = DEFAULT_COLLECTIBLE_TARGET_SIZE / 2.0;
    // if (QRandomGenerator::global()->bounded(2) == 0) {
    //     m_orbitOffset = -(effectiveRadiusForOffsetCalculation + COLLECTIBLE_ORBIT_PADDING);
    // } else {
    //     m_orbitOffset = (effectiveRadiusForOffsetCalculation + COLLECTIBLE_ORBIT_PADDING);
    // }
    // m_orbitOffset 现在将由 GameScene 在创建此对象后，
    // 根据从JSON读取的 radialOffset 值通过 setOrbitOffset() 方法设置。
}

CollectibleItem::~CollectibleItem()
{
    // qDebug() << "CollectibleItem PIXMAP at track " << m_associatedTrackIndex << " angle " << m_angleOnTrack << " deleted.";
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
    // 当偏移量更新时，可能需要立即更新视觉位置，
    // 但这通常由 GameScene 的主更新循环或初始化逻辑处理。
    // 如果 GameScene 不会立即调用 updateVisualPosition，可以在这里调用，
    // 但需要确保 trackCenter 和 trackRadius 是可用的。
    // 通常，在 GameScene 中设置偏移后，再统一调用 updateVisualPosition 更清晰。
}

qreal CollectibleItem::getOrbitOffset() const
{
    return m_orbitOffset;
}

void CollectibleItem::updateVisualPosition(const QPointF& trackCenter, qreal trackRadius)
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

void CollectibleItem::setVisibleState(bool visible)
{
    setVisible(visible && !m_isCollected);
}

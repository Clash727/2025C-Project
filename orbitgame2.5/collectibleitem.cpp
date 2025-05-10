// collectibleitem.cpp
#include "collectibleitem.h"
#include <QDebug>
#include <QPixmap> // 确保包含

CollectibleItem::CollectibleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                                 QGraphicsItem *parent) // 移除了 radius 参数
    : QObject(nullptr),
    QGraphicsPixmapItem(parent), // 修改: 调用 QGraphicsPixmapItem 构造
    m_associatedTrackIndex(associatedTrackIndex),
    m_angleOnTrack(angleOnTrackRadians),
    m_isCollected(false),
    m_scoreValue(DEFAULT_SCORE_PER_COLLECTIBLE),
    m_orbitOffset(0)
{
    qDebug() << "[CollectibleItem PIXMAP Constructor] this:" << static_cast<void*>(this)
    << "associatedTrackIndex received:" << associatedTrackIndex
    << "angle (rad):" << angleOnTrackRadians;

    // 1. 加载贴图 - 确保路径 ":/images/collectible.png" 在您的 .qrc 文件中是正确的
    QPixmap originalPixmap(":/images/collectible.png");

    if (originalPixmap.isNull()) {
        qWarning() << "Failed to load collectible image ':/images/collectible.png'. Using fallback red square.";
        // 创建一个红色方块作为后备图像
        QPixmap fallbackPixmap(static_cast<int>(DEFAULT_COLLECTIBLE_TARGET_SIZE), static_cast<int>(DEFAULT_COLLECTIBLE_TARGET_SIZE));
        fallbackPixmap.fill(Qt::red);
        setPixmap(fallbackPixmap);
    } else {
        // 2. （可选）缩放贴图到期望大小
        QPixmap scaledPixmap = originalPixmap.scaled(static_cast<int>(DEFAULT_COLLECTIBLE_TARGET_SIZE),
                                                     static_cast<int>(DEFAULT_COLLECTIBLE_TARGET_SIZE),
                                                     Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation);
        setPixmap(scaledPixmap);
    }

    // 3. 设置变换原点为中心，这对于未来的旋转或缩放（如果需要）很重要
    if (!pixmap().isNull()) {
        setTransformOriginPoint(pixmap().width() / 2.0, pixmap().height() / 2.0);
    }

    setZValue(0.5); // Z值，确保它在轨道之上
    setVisible(false); // 初始不可见，由GameScene控制

    // 随机决定是在内圈还是外圈
    // 注意：这里的内外圈偏移计算依赖于一个“有效半径”的概念
    // 我们用 DEFAULT_COLLECTIBLE_TARGET_SIZE / 2.0 来近似这个半径
    qreal effectiveRadiusForOffsetCalculation = DEFAULT_COLLECTIBLE_TARGET_SIZE / 2.0;
    if (QRandomGenerator::global()->bounded(2) == 0) { // 0 代表内圈
        m_orbitOffset = -(effectiveRadiusForOffsetCalculation + COLLECTIBLE_ORBIT_PADDING);
    } else { // 1 代表外圈
        m_orbitOffset = (effectiveRadiusForOffsetCalculation + COLLECTIBLE_ORBIT_PADDING);
    }
}

CollectibleItem::~CollectibleItem()
{
    // qDebug() << "CollectibleItem PIXMAP at track " << m_associatedTrackIndex << " angle " << m_angleOnTrack << " deleted.";
}

void CollectibleItem::collect()
{
    if (!m_isCollected) {
        m_isCollected = true;
        setVisible(false); // 收集后不可见
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
    return m_associatedTrackIndex; // 这是之前报错“undefined reference”的函数，确保它在这里
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
    qreal effectiveOrbitRadius = trackRadius + m_orbitOffset;

    // 计算Pixmap的中心应该在的位置
    qreal x_center = trackCenter.x() + effectiveOrbitRadius * qCos(m_angleOnTrack);
    qreal y_center = trackCenter.y() + effectiveOrbitRadius * qSin(m_angleOnTrack);

    // QGraphicsPixmapItem 的 setPos 是设置其左上角
    // 所以需要调整以使其（视觉）中心在计算出的 (x_center, y_center)
    if (!pixmap().isNull()) {
        setPos(x_center - pixmap().width() / 2.0,
               y_center - pixmap().height() / 2.0);
    } else {
        // 如果没有图片，就把它放在计算出的中心点（不太可能发生）
        setPos(x_center, y_center);
    }
}

void CollectibleItem::setVisibleState(bool visible)
{
    setVisible(visible && !m_isCollected); // 只有未被收集且被要求显示时才显示
}

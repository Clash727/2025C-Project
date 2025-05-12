// 文件: collectibleitem.cpp
#include "collectibleitem.h"
#include <QDebug>
#include <QPixmap>
#include <QUrl> // <--- 添加 QUrl 头文件 (用于 QSoundEffect::setSource)

CollectibleItem::CollectibleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                                 QGraphicsItem *parent)
    : QObject(nullptr),
    QGraphicsPixmapItem(parent),
    m_associatedTrackIndex(associatedTrackIndex),
    m_angleOnTrack(angleOnTrackRadians),
    m_isCollected(false),
    m_scoreValue(DEFAULT_SCORE_PER_COLLECTIBLE),
    m_orbitOffset(0)
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

    // 初始化收集音效
    m_collectSoundEffect.setSource(QUrl("qrc:/sounds/collect.wav")); // <--- 设置音效文件路径 (对应 .qrc 中的 alias)
    m_collectSoundEffect.setVolume(0.75); // 可选: 设置音量 (0.0 到 1.0)
    // m_collectSoundEffect.setLoopCount(1); // 确保只播放一次 (QSoundEffect 默认播放一次)
    // 如果需要明确设置，可以取消注释
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

        // 播放收集音效
        if (m_collectSoundEffect.isLoaded()) { // 检查音效是否已成功加载
            m_collectSoundEffect.play();
        } else {
            qWarning() << "Collect sound (qrc:/sounds/collect.wav) not loaded or error: " << m_collectSoundEffect.status();
        }

        emit collectedSignal(this);
    }
}

// ... (其他 CollectibleItem 的方法保持不变) ...

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

void CollectibleItem::setVisibleState(bool visible)
{
    setVisible(visible && !m_isCollected);
}

// 文件: obstacleitem.cpp
#include "obstacleitem.h"
#include <QDebug>
#include <QPixmap>
#include <QUrl> // <--- 添加 QUrl 头文件

ObstacleItem::ObstacleItem(int associatedTrackIndex, qreal angleOnTrackRadians,
                           QGraphicsItem *parent)
    : QObject(nullptr),
    QGraphicsPixmapItem(parent),
    m_associatedTrackIndex(associatedTrackIndex),
    m_angleOnTrack(angleOnTrackRadians),
    m_isHit(false),
    m_orbitOffset(0)
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

    // 初始化受击音效
    m_hitSoundEffect.setSource(QUrl("qrc:/sounds/hit.wav")); // <--- 设置音效文件路径 (对应 .qrc 中的 alias)
    m_hitSoundEffect.setVolume(0.8); // 可选: 设置音量
    // m_hitSoundEffect.setLoopCount(1);
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

        // 播放受击音效
        if (m_hitSoundEffect.isLoaded()) { // 检查音效是否已成功加载
            m_hitSoundEffect.play();
        } else {
            qWarning() << "Hit sound (qrc:/sounds/hit.wav) not loaded or error: " << m_hitSoundEffect.status();
        }

        emit hitSignal(this);
    }
}

// ... (其他 ObstacleItem 的方法保持不变) ...

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
    setVisible(visible && !m_isHit);
}

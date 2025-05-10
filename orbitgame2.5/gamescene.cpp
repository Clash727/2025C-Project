#include "Gamescene.h"
#include "trackdata.h"
#include "collectibleitem.h" // 包含 collectibleitem.h 以获取 DEFAULT_COLLECTIBLE_TARGET_SIZE
#include "obstacleitem.h"
#include <QGraphicsView>
#include <QKeyEvent>
#include <QFont>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDebug>
#include <QtMath>
#include <QGraphicsPixmapItem>
#include <QMovie>

// --- 游戏常量 ---
// (已在 .h 文件中定义)
// 确保 DEFAULT_COLLECTIBLE_TARGET_SIZE 在 collectibleitem.h 中定义或可通过包含访问
// 例如: const qreal DEFAULT_COLLECTIBLE_TARGET_SIZE = 20.0; // 应该在 collectibleitem.h 中

const qreal ANGLE_TOP = 3 * M_PI / 2.0;
const qreal ANGLE_BOTTOM = M_PI / 2.0;

GameScene::GameScene(QObject *parent)
    : QGraphicsScene(parent),
    m_trackBrush(Qt::NoBrush),
    m_trackPen(Qt::gray, 2),
    m_targetBrush(Qt::gray),
    m_targetPen(Qt::NoPen),
    m_gameOver(true),
    m_currentTrackIndex(0),
    m_currentAngle(ANGLE_BOTTOM),
    m_orbitOffset(BALL_RADIUS + ORBIT_PADDING),
    m_canTakeDamage(true),
    m_linearSpeed(BASE_LINEAR_SPEED),
    m_rotationDirection(1),
    m_score(0),
    m_speedLevel(0),
    m_health(m_maxHealth),
    m_ball(nullptr),
    m_targetDot(nullptr),
    m_healthText(nullptr),
    m_judgmentText(nullptr),
    m_gameOverText(nullptr),
    m_scoreText(nullptr),
    m_timer(new QTimer(this)),
    m_judgmentTimer(new QTimer(this)),
    m_damageCooldownTimer(new QTimer(this)),
    m_explosionDurationTimer(nullptr),
    m_backgroundMusicPlayer(nullptr),
    m_audioOutput(nullptr),
    m_explosionMovie(nullptr),
    m_explosionItem(nullptr),
    m_collectEffectMovie(nullptr),      // 初始化新增成员
    m_collectEffectItem(nullptr),       // 初始化新增成员
    m_collectEffectDurationTimer(nullptr) // 初始化新增成员
{
    setSceneRect(-2000, -2000, 4000, 4000);

    // --- 背景初始化 (无限滚动) ---
    m_backgroundTilePixmap.load(":/images/background.png");
    if (m_backgroundTilePixmap.isNull()) {
        qWarning() << "Failed to load background tile image: :/images/background.png. Infinite background will not work.";
        setBackgroundBrush(Qt::darkGray);
    } else {
        m_backgroundTileSize = m_backgroundTilePixmap.size();
        if(m_backgroundTileSize.width() > 0 && m_backgroundTileSize.height() > 0) {
            for (int i = 0; i < BG_GRID_SIZE * BG_GRID_SIZE; ++i) {
                QGraphicsPixmapItem *tile = new QGraphicsPixmapItem(m_backgroundTilePixmap);
                tile->setZValue(-1.0);
                tile->setVisible(false);
                addItem(tile);
                m_backgroundTiles.append(tile);
            }
        } else {
            qWarning() << "Background tile image has zero width or height.";
            setBackgroundBrush(Qt::darkGray);
        }
    }

    connect(m_timer, &QTimer::timeout, this, &GameScene::updateGame);
    m_judgmentTimer->setSingleShot(true);
    connect(m_judgmentTimer, &QTimer::timeout, this, &GameScene::hideJudgmentText);
    m_damageCooldownTimer->setSingleShot(true);
    connect(m_damageCooldownTimer, &QTimer::timeout, this, &GameScene::enableDamageTaking);

    m_backgroundMusicPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_backgroundMusicPlayer->setAudioOutput(m_audioOutput);
    m_backgroundMusicPlayer->setSource(QUrl("qrc:/music/softmusic.mp3"));
    m_backgroundMusicPlayer->setLoops(QMediaPlayer::Infinite);
    m_audioOutput->setVolume(0.5);

    connect(m_backgroundMusicPlayer, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status){
                if (status == QMediaPlayer::LoadedMedia && m_backgroundMusicPlayer->playbackState() != QMediaPlayer::PlayingState) {
                    m_backgroundMusicPlayer->play();
                }
            });
    connect(m_backgroundMusicPlayer, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString &errorString){
                qDebug() << "[QMediaPlayer] ErrorOccurred:" << error << "ErrorString:" << errorString;
            });

    // --- 爆炸动画初始化 (用于障碍物/失误) ---
    m_explosionMovie = new QMovie(":/animations/explosion.gif", QByteArray(), this);
    if (!m_explosionMovie->isValid()) {
        qWarning() << "Failed to load explosion GIF: :/animations/explosion.gif";
    }
    m_explosionItem = new QGraphicsPixmapItem();
    m_explosionItem->setZValue(1.5); // 爆炸效果在最上层
    m_explosionItem->setVisible(false);
    addItem(m_explosionItem);

    connect(m_explosionMovie, &QMovie::frameChanged, this, &GameScene::updateExplosionFrame);
    connect(m_explosionMovie, &QMovie::finished, this, &GameScene::hideExplosionEffect);

    m_explosionDurationTimer = new QTimer(this);
    m_explosionDurationTimer->setSingleShot(true);
    connect(m_explosionDurationTimer, &QTimer::timeout, this, &GameScene::forceHideExplosion);
    // --- 结束爆炸动画初始化 ---

    // --- 收集品效果动画初始化 ---
    m_collectEffectMovie = new QMovie(":/animations/collect_effect.gif", QByteArray(), this); // 使用新的GIF
    if (!m_collectEffectMovie->isValid()) {
        qWarning() << "Failed to load collect effect GIF: :/animations/collect_effect.gif";
    }
    m_collectEffectItem = new QGraphicsPixmapItem();
    m_collectEffectItem->setZValue(1.4); // 比爆炸低一点，但高于其他元素
    m_collectEffectItem->setVisible(false);
    addItem(m_collectEffectItem);

    connect(m_collectEffectMovie, &QMovie::frameChanged, this, &GameScene::updateCollectEffectFrame);
    connect(m_collectEffectMovie, &QMovie::finished, this, &GameScene::hideCollectEffect);

    m_collectEffectDurationTimer = new QTimer(this);
    m_collectEffectDurationTimer->setSingleShot(true);
    connect(m_collectEffectDurationTimer, &QTimer::timeout, this, &GameScene::forceHideCollectEffect);
    // --- 结束收集品效果动画初始化 ---


    initializeGame();
}

GameScene::~GameScene()
{
    if (m_backgroundMusicPlayer) {
        m_backgroundMusicPlayer->stop();
    }

    qDeleteAll(m_backgroundTiles);
    m_backgroundTiles.clear();

    clearAllCollectibles();
    clearAllObstacles();
    qDeleteAll(m_trackItems);
    m_trackItems.clear();

    if (m_ball && m_ball->scene() == this) { removeItem(m_ball); delete m_ball; m_ball = nullptr; }
    if (m_targetDot && m_targetDot->scene() == this) { removeItem(m_targetDot); delete m_targetDot; m_targetDot = nullptr; }
    if (m_healthText && m_healthText->scene() == this) { removeItem(m_healthText); delete m_healthText; m_healthText = nullptr; }
    if (m_judgmentText && m_judgmentText->scene() == this) { removeItem(m_judgmentText); delete m_judgmentText; m_judgmentText = nullptr; }
    if (m_gameOverText && m_gameOverText->scene() == this) { removeItem(m_gameOverText); delete m_gameOverText; m_gameOverText = nullptr; }
    if (m_scoreText && m_scoreText->scene() == this) { removeItem(m_scoreText); delete m_scoreText; m_scoreText = nullptr; }

    // 清理爆炸效果资源
    if (m_explosionMovie) {
        m_explosionMovie->stop();
        // m_explosionMovie is a child of 'this', auto-deleted by Qt
    }
    if (m_explosionItem && m_explosionItem->scene() == this) {
        removeItem(m_explosionItem);
        delete m_explosionItem;
        m_explosionItem = nullptr;
    }
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) {
        m_explosionDurationTimer->stop();
        // m_explosionDurationTimer is a child of 'this', auto-deleted by Qt
    }

    // 清理收集品效果资源
    if (m_collectEffectMovie) {
        m_collectEffectMovie->stop();
        // m_collectEffectMovie is a child of 'this', auto-deleted by Qt
    }
    if (m_collectEffectItem && m_collectEffectItem->scene() == this) {
        removeItem(m_collectEffectItem);
        delete m_collectEffectItem;
        m_collectEffectItem = nullptr;
    }
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) {
        m_collectEffectDurationTimer->stop();
        // m_collectEffectDurationTimer is a child of 'this', auto-deleted by Qt
    }
}

void GameScene::clearAllCollectibles()
{
    for (CollectibleItem* collectible : m_collectibles) {
        if (collectible) {
            if (collectible->scene() == this) { removeItem(collectible); }
            delete collectible;
        }
    }
    m_collectibles.clear();
}

void GameScene::clearAllObstacles()
{
    for (ObstacleItem* obstacle : m_obstacles) {
        if (obstacle) {
            if (obstacle->scene() == this) { removeItem(obstacle); }
            delete obstacle;
        }
    }
    m_obstacles.clear();
}


bool GameScene::loadTracksAndCollectiblesFromFile(const QString& filename)
{
    m_tracks.clear();
    clearAllCollectibles();
    clearAllObstacles();

    QFile loadFile(filename);
    if (!loadFile.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QByteArray saveData = loadFile.readAll();
    loadFile.close();
    QJsonDocument loadDoc = QJsonDocument::fromJson(saveData);
    if (!loadDoc.isArray()) return false;

    QJsonArray trackArray = loadDoc.array();
    for (int i = 0; i < trackArray.size(); ++i) {
        if (!trackArray[i].isObject()){ continue; }
        QJsonObject trackObject = trackArray[i].toObject();
        qreal cx = trackObject["centerX"].toDouble();
        qreal cy = trackObject["centerY"].toDouble();
        qreal r = trackObject["radius"].toDouble();
        qreal tangentDeg = trackObject.value("tangentAngleDegrees").toDouble(0.0);

        if (r <= 0) { continue; }
        TrackData trackData(QPointF(cx, cy), r);
        trackData.tangentAngle = qDegreesToRadians(tangentDeg);
        m_tracks.append(trackData);

        if (trackObject.contains("collectibles") && trackObject["collectibles"].isArray()) {
            QJsonArray collectiblesArray = trackObject["collectibles"].toArray();
            for (const QJsonValue& collectibleVal : collectiblesArray) {
                if (collectibleVal.isObject()) {
                    QJsonObject cObj = collectibleVal.toObject();
                    if (cObj.contains("angleDegrees")) {
                        CollectibleItem *item = new CollectibleItem(i, qDegreesToRadians(cObj["angleDegrees"].toDouble()));
                        connect(item, &CollectibleItem::collectedSignal, this, &GameScene::handleCollectibleCollected);
                        m_collectibles.append(item);
                    }
                }
            }
        }
        if (trackObject.contains("obstacles") && trackObject["obstacles"].isArray()) {
            QJsonArray obstaclesArray = trackObject["obstacles"].toArray();
            for (const QJsonValue& obstacleVal : obstaclesArray) {
                if (obstacleVal.isObject()) {
                    QJsonObject oObj = obstacleVal.toObject();
                    if (oObj.contains("angleDegrees")) {
                        ObstacleItem *item = new ObstacleItem(i, qDegreesToRadians(oObj["angleDegrees"].toDouble()));
                        connect(item, &ObstacleItem::hitSignal, this, &GameScene::handleObstacleHit);
                        m_obstacles.append(item);
                    }
                }
            }
        }
    }
    return !m_tracks.isEmpty();
}

void GameScene::initializeGame()
{
    if (m_timer->isActive()) m_timer->stop();
    if (m_judgmentTimer->isActive()) m_judgmentTimer->stop();
    if (m_damageCooldownTimer->isActive()) m_damageCooldownTimer->stop();

    // 重置爆炸效果
    if (m_explosionMovie && m_explosionMovie->state() == QMovie::Running) {
        m_explosionMovie->stop();
    }
    if (m_explosionItem) {
        m_explosionItem->setVisible(false);
    }
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) {
        m_explosionDurationTimer->stop();
    }

    // 重置收集品效果
    if (m_collectEffectMovie && m_collectEffectMovie->state() == QMovie::Running) {
        m_collectEffectMovie->stop();
    }
    if (m_collectEffectItem) {
        m_collectEffectItem->setVisible(false);
    }
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) {
        m_collectEffectDurationTimer->stop();
    }

    if (m_ball) { removeItem(m_ball); delete m_ball; m_ball = nullptr; }
    if (m_targetDot) { removeItem(m_targetDot); delete m_targetDot; m_targetDot = nullptr; }
    if (m_healthText) { removeItem(m_healthText); delete m_healthText; m_healthText = nullptr; }
    if (m_judgmentText) { removeItem(m_judgmentText); delete m_judgmentText; m_judgmentText = nullptr; }
    if (m_gameOverText) { removeItem(m_gameOverText); delete m_gameOverText; m_gameOverText = nullptr; }
    if (m_scoreText) { removeItem(m_scoreText); delete m_scoreText; m_scoreText = nullptr; }

    clearAllCollectibles();
    clearAllObstacles();

    qDeleteAll(m_trackItems);
    m_trackItems.clear();

    m_gameOver = false;
    m_currentTrackIndex = 0;
    m_currentAngle = ANGLE_BOTTOM;
    m_orbitOffset = BALL_RADIUS + ORBIT_PADDING;
    m_canTakeDamage = true;
    m_health = m_maxHealth;
    m_score = 0;
    m_speedLevel = 0;
    m_linearSpeed = BASE_LINEAR_SPEED;
    m_rotationDirection = 1;

    if (!loadTracksAndCollectiblesFromFile(":/levels/level1.json")) {
        qCritical() << "Failed to load level data. Game cannot start.";
        m_gameOver = true;
        return;
    }

    QRectF totalTracksRect;
    if (!m_tracks.isEmpty()) {
        totalTracksRect = m_tracks.first().bounds;
        for (int i = 1; i < m_tracks.size(); ++i) {
            totalTracksRect = totalTracksRect.united(m_tracks[i].bounds);
        }
        qreal padding = 200.0;
        totalTracksRect.adjust(-padding, -padding, padding, padding);
        setSceneRect(totalTracksRect);
    }


    if (!m_tracks.isEmpty()) {
        for (const TrackData& trackData : m_tracks) {
            addTrackItem(trackData);
        }
    }

    if (!m_tracks.isEmpty()) {
        QPixmap originalSpaceshipPixmap(":/images/spaceship.png");
        if (originalSpaceshipPixmap.isNull()) {
            qWarning() << "Failed to load spaceship image. Using fallback.";
            QPixmap fallbackPixmap(20, 20); fallbackPixmap.fill(Qt::blue);
            m_ball = new QGraphicsPixmapItem(fallbackPixmap);
        } else {
            qreal targetDiameter = 2.0 * BALL_RADIUS;
            QPixmap scaledSpaceshipPixmap = originalSpaceshipPixmap.scaled(targetDiameter, targetDiameter, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_ball = new QGraphicsPixmapItem(scaledSpaceshipPixmap);
        }
        m_ball->setZValue(1.0);
        addItem(m_ball);
        m_ball->setTransformOriginPoint(m_ball->boundingRect().center());
    }

    if (!m_tracks.isEmpty()) {
        m_targetDot = new QGraphicsEllipseItem(-TARGET_DOT_RADIUS, -TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS);
        m_targetDot->setBrush(m_targetBrush); m_targetDot->setPen(m_targetPen); m_targetDot->setZValue(1.0); addItem(m_targetDot);
    }
    m_healthText = new QGraphicsTextItem(); m_healthText->setFont(QFont("Arial", 16, QFont::Bold)); m_healthText->setZValue(2.0); addItem(m_healthText);
    m_judgmentText = new QGraphicsTextItem(); m_judgmentText->setFont(QFont("Arial", 20, QFont::Bold)); m_judgmentText->setZValue(2.0); m_judgmentText->setVisible(false); addItem(m_judgmentText);
    m_scoreText = new QGraphicsTextItem(); m_scoreText->setFont(QFont("Arial", 16, QFont::Bold)); m_scoreText->setDefaultTextColor(Qt::white); m_scoreText->setZValue(2.0); addItem(m_scoreText);

    positionAndShowCollectibles();
    positionAndShowObstacles();

    if(m_ball) updateBallPosition();
    if(m_targetDot) updateTargetDotPosition();
    updateHealthDisplay();
    updateScoreDisplayAndSpeed();

    if (!views().isEmpty()) {
        updateInfiniteBackground();
    }


    if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->mediaStatus() == QMediaPlayer::LoadedMedia && m_backgroundMusicPlayer->playbackState() != QMediaPlayer::PlayingState) {
        m_backgroundMusicPlayer->play();
    } else if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->mediaStatus() != QMediaPlayer::LoadedMedia) {
        qWarning() << "Background music not loaded, trying to play source:" << m_backgroundMusicPlayer->source();
    }

    m_timer->start(16);
}

void GameScene::addTrackItem(const TrackData& trackData) {
    QGraphicsEllipseItem* trackItem = new QGraphicsEllipseItem(trackData.bounds);
    trackItem->setPen(m_trackPen);
    trackItem->setBrush(m_trackBrush);
    trackItem->setZValue(0);
    addItem(trackItem);
    m_trackItems.append(trackItem);
}

void GameScene::positionAndShowCollectibles()
{
    for (CollectibleItem* collectible : m_collectibles) {
        if (!collectible) continue;
        if (collectible->scene() == nullptr) addItem(collectible);
        int trackIdx = collectible->getAssociatedTrackIndex();
        if (trackIdx >= 0 && trackIdx < m_tracks.size()) {
            const TrackData& track = m_tracks[trackIdx];
            collectible->updateVisualPosition(track.center, track.radius);
            collectible->setZValue(0.5);
            collectible->setVisibleState(true);
        } else {
            collectible->setVisibleState(false);
        }
    }
}

void GameScene::positionAndShowObstacles()
{
    for (ObstacleItem* obstacle : m_obstacles) {
        if (!obstacle) continue;
        if (obstacle->scene() == nullptr) addItem(obstacle);
        int trackIdx = obstacle->getAssociatedTrackIndex();
        if (trackIdx >= 0 && trackIdx < m_tracks.size()) {
            const TrackData& track = m_tracks[trackIdx];
            obstacle->updateVisualPosition(track.center, track.radius);
            obstacle->setZValue(0.6);
            obstacle->setVisibleState(true);
        } else {
            obstacle->setVisibleState(false);
        }
    }
}

void GameScene::updateInfiniteBackground() {
    if (m_backgroundTilePixmap.isNull() || m_backgroundTileSize.width() <= 0 || m_backgroundTileSize.height() <= 0 || views().isEmpty()) {
        for(QGraphicsPixmapItem* tile : m_backgroundTiles) {
            if(tile) tile->setVisible(false);
        }
        return;
    }

    QGraphicsView* view = views().first();
    QRectF visibleRect = view->mapToScene(view->viewport()->geometry()).boundingRect();
    QPointF viewCenter = visibleRect.center();

    qreal tileW = m_backgroundTileSize.width();
    qreal tileH = m_backgroundTileSize.height();

    qreal centralAnchorX = floor(viewCenter.x() / tileW) * tileW;
    qreal centralAnchorY = floor(viewCenter.y() / tileH) * tileH;

    int offset = (BG_GRID_SIZE - 1) / 2;

    int tileIndex = 0;
    for (int row = 0; row < BG_GRID_SIZE; ++row) {
        for (int col = 0; col < BG_GRID_SIZE; ++col) {
            if (tileIndex < m_backgroundTiles.size()) {
                QGraphicsPixmapItem *tile = m_backgroundTiles.at(tileIndex);
                qreal tileX = centralAnchorX + (col - offset) * tileW;
                qreal tileY = centralAnchorY + (row - offset) * tileH;
                tile->setPos(tileX, tileY);
                tile->setVisible(true);
            }
            tileIndex++;
        }
    }
}


void GameScene::updateGame()
{
    if (m_gameOver) return;

    updateInfiniteBackground();

    if (m_tracks.isEmpty() || !m_ball) { if (!m_gameOver) endGame(); return; }
    if (m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) { endGame(); return; }


    const TrackData& currentTrack = m_tracks[m_currentTrackIndex];
    qreal effectiveBallRadiusOnTrack = currentTrack.radius + m_orbitOffset;
    if (effectiveBallRadiusOnTrack < BALL_RADIUS) effectiveBallRadiusOnTrack = BALL_RADIUS;

    qreal currentAngularVelocity = (effectiveBallRadiusOnTrack > 0.01) ? (m_linearSpeed / effectiveBallRadiusOnTrack) : 0;
    qreal timeDelta = m_timer->interval() / 1000.0;
    qreal angleDelta = currentAngularVelocity * timeDelta;

    m_currentAngle += angleDelta * m_rotationDirection;

    while (m_currentAngle >= 2.0 * M_PI) m_currentAngle -= 2.0 * M_PI;
    while (m_currentAngle < 0) m_currentAngle += 2.0 * M_PI;

    updateBallPosition();

    checkBallCollectibleCollisions();
    checkBallObstacleCollisions();
    if (m_canTakeDamage) checkCollisions();

    if (!views().isEmpty() && m_ball) {
        QGraphicsView* view = views().first();
        qreal yOffsetScene = 50;
        QPointF ballPixmapCenter = m_ball->sceneBoundingRect().center();
        QPointF targetCenterPoint = ballPixmapCenter - QPointF(0, yOffsetScene);

        view->centerOn(targetCenterPoint);

        QRectF viewRectForHUD = view->mapToScene(view->viewport()->geometry()).boundingRect();
        if (m_healthText) m_healthText->setPos(viewRectForHUD.topLeft() + QPointF(10, 10));
        if (m_scoreText) {
            QRectF scoreRect = m_scoreText->boundingRect();
            m_scoreText->setPos(viewRectForHUD.topRight() + QPointF(-scoreRect.width() - 10, 10));
        }
        if (m_gameOverText && m_gameOverText->isVisible()) {
            QRectF gameOverRect = m_gameOverText->boundingRect();
            m_gameOverText->setPos(viewRectForHUD.center() - QPointF(gameOverRect.width() / 2.0, gameOverRect.height() / 2.0));
        }
    }
}

void GameScene::updateBallPosition() {
    if (!m_ball || m_tracks.isEmpty() || m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) return;

    const TrackData& currentTrackData = m_tracks[m_currentTrackIndex];
    qreal effectiveRadius = currentTrackData.radius + m_orbitOffset;

    if (effectiveRadius < BALL_RADIUS && m_orbitOffset < 0) effectiveRadius = BALL_RADIUS;
    else if (effectiveRadius < 0 && m_orbitOffset > 0) effectiveRadius = BALL_RADIUS;

    qreal x_center = currentTrackData.center.x() + effectiveRadius * qCos(m_currentAngle);
    qreal y_center = currentTrackData.center.y() + effectiveRadius * qSin(m_currentAngle);

    if (m_ball->pixmap().isNull()) {
        m_ball->setPos(x_center, y_center);
    } else {
        m_ball->setPos(x_center - m_ball->boundingRect().width() / 2.0,
                       y_center - m_ball->boundingRect().height() / 2.0);
    }

    qreal tangentAngleRadians = m_currentAngle + m_rotationDirection * (M_PI / 2.0);
    qreal rotationAngleDegrees = qRadiansToDegrees(tangentAngleRadians);
    rotationAngleDegrees += 90.0;

    m_ball->setRotation(rotationAngleDegrees);
}

void GameScene::updateTargetDotPosition() {
    if (!m_targetDot || m_tracks.isEmpty() || m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) {
        if(m_targetDot) m_targetDot->setVisible(false);
        return;
    }
    const TrackData& currentTrackData = m_tracks[m_currentTrackIndex];
    QPointF judgmentPoint = currentTrackData.center + QPointF(currentTrackData.radius * qCos(ANGLE_TOP), currentTrackData.radius * qSin(ANGLE_TOP));
    m_targetDot->setPos(judgmentPoint);
    m_targetDot->setVisible(true);
}

void GameScene::keyPressEvent(QKeyEvent *event)
{
    if (m_gameOver) {
        if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
            initializeGame();
        }
        event->ignore();
        return;
    }

    if (m_tracks.isEmpty() || !m_ball) {
        QGraphicsScene::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_K && !event->isAutoRepeat()) {
        if (m_currentTrackIndex + 1 >= m_tracks.size()) {
            event->accept(); return;
        }

        const TrackData& currentTrackData = m_tracks[m_currentTrackIndex];
        qreal effectiveBallRadiusOnTrack = currentTrackData.radius + m_orbitOffset;
        if (effectiveBallRadiusOnTrack < BALL_RADIUS) effectiveBallRadiusOnTrack = BALL_RADIUS;

        qreal angleDiff = m_currentAngle - ANGLE_TOP;
        while (angleDiff <= -M_PI) angleDiff += 2.0 * M_PI;
        while (angleDiff > M_PI) angleDiff -= 2.0 * M_PI;
        qreal absAngleDiff = qAbs(angleDiff);

        qreal angularSpeed = (effectiveBallRadiusOnTrack > 0.01) ? (m_linearSpeed / effectiveBallRadiusOnTrack) : 0;
        qreal perfectAngleTolerance = angularSpeed * (PERFECT_MS / 1000.0);
        qreal goodAngleTolerance = angularSpeed * (GOOD_MS / 1000.0);

        QString judgmentTextStr;
        QColor textColor;
        bool success = false;
        int scoreGainedThisHit = 0;

        if (absAngleDiff <= perfectAngleTolerance) {
            judgmentTextStr = "PERFECT!"; textColor = Qt::yellow; success = true; scoreGainedThisHit = SCORE_PERFECT;
        } else if (absAngleDiff <= goodAngleTolerance) {
            judgmentTextStr = "GOOD!"; textColor = Qt::cyan; success = true; scoreGainedThisHit = SCORE_GOOD;
        } else {
            m_health--;
            updateHealthDisplay();
            triggerExplosionEffect(); // 失误时触发爆炸效果
            if (m_health <= 0) { endGame(); event->accept(); return; }
        }

        if (!judgmentTextStr.isEmpty() && m_judgmentText) {
            m_judgmentText->setPlainText(judgmentTextStr);
            m_judgmentText->setDefaultTextColor(textColor);
            m_judgmentText->setVisible(true);
            QPointF textPos;
            if (m_targetDot && m_targetDot->isVisible()) {
                QPointF targetDotCenter = m_targetDot->sceneBoundingRect().center();
                QRectF judgmentRect = m_judgmentText->boundingRect();
                textPos = targetDotCenter - QPointF(judgmentRect.width() / 2.0, judgmentRect.height() + TARGET_DOT_RADIUS + 5);
            } else if (!views().isEmpty()) {
                QRectF viewRect = views().first()->mapToScene(views().first()->viewport()->geometry()).boundingRect();
                QRectF judgmentRect = m_judgmentText->boundingRect();
                textPos = viewRect.center() - QPointF(judgmentRect.width() / 2.0, judgmentRect.height() /2.0 + 50);
            } else {
                textPos = QPointF(-m_judgmentText->boundingRect().width()/2, -50);
            }
            m_judgmentText->setPos(textPos);
            m_judgmentTimer->start(500);
        }

        if (success) {
            m_score += scoreGainedThisHit;
            updateScoreDisplayAndSpeed();
            switchTrack();
        }
        event->accept(); return;
    }
    else if (event->key() == Qt::Key_J && !event->isAutoRepeat()) {
        m_orbitOffset = -m_orbitOffset;
        updateBallPosition();
        event->accept(); return;
    }
    QGraphicsScene::keyPressEvent(event);
}

void GameScene::hideJudgmentText() { if (m_judgmentText) m_judgmentText->setVisible(false); }
void GameScene::enableDamageTaking() { m_canTakeDamage = true; }

void GameScene::checkCollisions() {
    if (m_orbitOffset <= 0 || !m_ball || m_tracks.isEmpty() || !m_canTakeDamage || m_currentTrackIndex < 0) return;

    QPointF ballVisualCenter = m_ball->sceneBoundingRect().center();

    bool collisionOccurred = false;
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (i == m_currentTrackIndex) continue;

        const TrackData& otherTrack = m_tracks[i];
        qreal distanceToOtherTrackCenter = QLineF(ballVisualCenter, otherTrack.center).length();

        if (distanceToOtherTrackCenter < otherTrack.radius + BALL_RADIUS) {
            collisionOccurred = true;
            break;
        }
    }

    if (collisionOccurred) {
        m_health--;
        updateHealthDisplay();
        triggerExplosionEffect(); // 与其他轨道碰撞时触发爆炸效果
        m_orbitOffset = -(BALL_RADIUS + ORBIT_PADDING);
        updateBallPosition();

        m_canTakeDamage = false;
        m_damageCooldownTimer->start(DAMAGE_COOLDOWN_MS);

        if (m_health <= 0) {
            endGame();
        }
    }
}

void GameScene::checkBallCollectibleCollisions() {
    if (!m_ball) return;
    QList<QGraphicsItem*> collidingItemsList = m_ball->collidingItems();
    for (QGraphicsItem* item : collidingItemsList) {
        CollectibleItem* collectible = dynamic_cast<CollectibleItem*>(item);
        if (collectible && !collectible->isCollected()) {
            collectible->collect(); // collect() 会发出 collectedSignal
                // handleCollectibleCollected 会处理得分和动画
        }
    }
}

void GameScene::checkBallObstacleCollisions() {
    if (!m_ball) return;
    QList<QGraphicsItem*> collidingItemsList = m_ball->collidingItems();
    for (QGraphicsItem* item : collidingItemsList) {
        ObstacleItem* obstacle = dynamic_cast<ObstacleItem*>(item);
        if (obstacle && !obstacle->isHit()) {
            obstacle->processHit(); // processHit() 会发出 hitSignal
                // handleObstacleHit 会处理伤害和动画
        }
    }
}

void GameScene::handleCollectibleCollected(CollectibleItem* item) {
    if (!item) return;

    triggerCollectEffect(); // 触发收集品特定的动画 (跟随飞船)

    m_score += item->getScoreValue();
    updateScoreDisplayAndSpeed();
    // CollectibleItem::collect() 内部会设置自身不可见
}

void GameScene::handleObstacleHit(ObstacleItem* item) {
    if (!item || !m_canTakeDamage) return;
    m_health--;
    updateHealthDisplay();
    triggerExplosionEffect(); // 碰到障碍物时触发通用的爆炸/受击效果
    m_canTakeDamage = false;
    m_damageCooldownTimer->start(DAMAGE_COOLDOWN_MS);
    if (m_health <= 0) {
        endGame();
    }
}

void GameScene::updateHealthDisplay() {
    if (m_healthText) {
        m_healthText->setPlainText(QString("生命: %1 / %2").arg(m_health).arg(m_maxHealth));
        if (m_health <= 0) m_healthText->setDefaultTextColor(Qt::darkRed);
        else if (m_health > m_maxHealth * 0.6) m_healthText->setDefaultTextColor(Qt::darkGreen);
        else if (m_health > m_maxHealth * 0.3) m_healthText->setDefaultTextColor(QColorConstants::Svg::orange);
        else m_healthText->setDefaultTextColor(Qt::red);
    }
}

void GameScene::updateScoreDisplayAndSpeed() {
    if (m_scoreText) m_scoreText->setPlainText(QString("分数: %1").arg(m_score));

    int targetLevel = m_score / SCORE_THRESHOLD_FOR_SPEEDUP;
    if (targetLevel > m_speedLevel) {
        m_speedLevel = targetLevel;
        m_linearSpeed = BASE_LINEAR_SPEED * qPow(SPEEDUP_FACTOR, m_speedLevel);
    }
}

void GameScene::switchTrack() {
    if (m_currentTrackIndex + 1 >= m_tracks.size()) return;

    m_currentTrackIndex++;
    m_rotationDirection = -m_rotationDirection;
    m_currentAngle = ANGLE_BOTTOM;
    m_orbitOffset = qAbs(m_orbitOffset);

    updateTargetDotPosition();
    updateBallPosition();
}
void GameScene::endGame() {
    if(m_gameOver) return;
    m_gameOver = true;

    if (m_timer->isActive()) m_timer->stop();
    if (m_judgmentTimer->isActive()) m_judgmentTimer->stop();
    if (m_damageCooldownTimer->isActive()) m_damageCooldownTimer->stop();

    // 停止爆炸效果
    if (m_explosionMovie && m_explosionMovie->state() == QMovie::Running) {
        m_explosionMovie->stop();
    }
    if (m_explosionItem) {
        m_explosionItem->setVisible(false);
    }
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) {
        m_explosionDurationTimer->stop();
    }

    // 停止收集品效果
    if (m_collectEffectMovie && m_collectEffectMovie->state() == QMovie::Running) {
        m_collectEffectMovie->stop();
    }
    if (m_collectEffectItem) {
        m_collectEffectItem->setVisible(false);
    }
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) {
        m_collectEffectDurationTimer->stop();
    }

    if (m_targetDot) m_targetDot->setVisible(false);
    if (m_ball) m_ball->setVisible(false);

    for (CollectibleItem* c : m_collectibles) if(c) c->setVisibleState(false);
    for (ObstacleItem* o : m_obstacles) if(o) o->setVisibleState(false);

    if (m_healthText) m_healthText->setVisible(false);
    if (m_judgmentText) m_judgmentText->setVisible(false);
    if (m_scoreText) m_scoreText->setVisible(false);

    for(QGraphicsPixmapItem* tile : m_backgroundTiles) {
        if(tile) tile->setVisible(false);
    }


    if (!m_gameOverText) {
        m_gameOverText = new QGraphicsTextItem();
        addItem(m_gameOverText);
    }
    m_gameOverText->setPlainText(QString("游戏结束\n最终分数: %1\n按 空格键 重新开始").arg(m_score));
    m_gameOverText->setFont(QFont("Arial", 24, QFont::Bold));
    m_gameOverText->setDefaultTextColor(Qt::red);
    m_gameOverText->setZValue(3);
    m_gameOverText->setVisible(true);

    if (!views().isEmpty()) {
        QGraphicsView* view = views().first();
        QRectF viewRect = view->mapToScene(view->viewport()->geometry()).boundingRect();
        m_gameOverText->setPos(viewRect.center() - QPointF(m_gameOverText->boundingRect().width() / 2.0, m_gameOverText->boundingRect().height() / 2.0));
    } else {
        m_gameOverText->setPos(-m_gameOverText->boundingRect().width()/2, -m_gameOverText->boundingRect().height()/2);
    }
}

// 爆炸/受击效果 (通用)
void GameScene::triggerExplosionEffect() {
    // 确保所有必要的指针都是有效的
    if (!m_explosionMovie || !m_explosionItem || !m_ball || !m_explosionDurationTimer) {
        qWarning() << "triggerExplosionEffect: One or more essential members are null.";
        return;
    }
    if (!m_explosionMovie->isValid()) {
        qWarning() << "Explosion GIF is not valid, cannot play.";
        return;
    }

    // 如果0.6秒计时器已在运行，则先停止
    if (m_explosionDurationTimer->isActive()) {
        m_explosionDurationTimer->stop();
    }
    // 如果GIF动画已在运行，则先停止
    if (m_explosionMovie->state() == QMovie::Running) {
        m_explosionMovie->stop();
    }
    m_explosionMovie->jumpToFrame(0); // 确保从GIF的第一帧开始播放

    // 获取飞船中心位置
    QPointF effectCenter = m_ball->sceneBoundingRect().center(); // 爆炸效果在飞船处
    QPixmap originalFrame = m_explosionMovie->currentPixmap();
    if (originalFrame.isNull()) {
        qWarning() << "Explosion GIF current frame is null for triggerExplosionEffect.";
        return;
    }

    // --- 缩放逻辑 ---
    qreal effectDisplayDiameter = 5.0 * BALL_RADIUS;
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // --- 结束缩放逻辑 ---

    // 将缩放后的帧设置给 QGraphicsPixmapItem
    m_explosionItem->setPixmap(scaledFrame);

    // 使用缩放后帧的尺寸来计算并设置爆炸效果的中心位置
    m_explosionItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));

    // 显示爆炸效果并开始播放GIF
    m_explosionItem->setVisible(true);
    m_explosionMovie->start();

    // 启动0.6秒的计时器，用于在固定时间后隐藏爆炸效果
    m_explosionDurationTimer->start(600); // 0.6 秒
}

void GameScene::updateExplosionFrame(int frameNumber) {
    Q_UNUSED(frameNumber);
    // 确保所有必要的对象都有效，并且动画正在播放
    if (!m_explosionMovie || !m_explosionItem || m_explosionMovie->state() != QMovie::Running) {
        return;
    }
    if (!m_ball){ // 如果飞船不存在了（比如游戏结束的瞬间），就不更新位置了
        return;
    }

    // 获取GIF的当前帧
    QPixmap originalFrame = m_explosionMovie->currentPixmap();
    if (originalFrame.isNull()) {
        qWarning() << "Explosion GIF update frame is null.";
        return;
    }

    // --- 缩放逻辑 ---
    qreal effectDisplayDiameter = 5.0 * BALL_RADIUS;
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // --- 结束缩放逻辑 ---

    // 设置缩放后的帧
    m_explosionItem->setPixmap(scaledFrame);
    // 爆炸效果跟随飞船 (如果飞船还可见)
    if (m_ball->isVisible()) {
        QPointF effectCenter = m_ball->sceneBoundingRect().center();
        m_explosionItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));
    }
}

void GameScene::hideExplosionEffect() {
    // 当GIF自然播放完成时调用 (QMovie::finished)
    if (m_explosionItem) {
        m_explosionItem->setVisible(false);
    }
    // 如果0.6秒计时器此时仍在运行（说明GIF本身短于0.6秒），则停止它
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) {
        m_explosionDurationTimer->stop();
    }
}

void GameScene::forceHideExplosion() {
    // 当0.6秒计时器超时时调用
    if (m_explosionMovie && m_explosionMovie->state() == QMovie::Running) {
        m_explosionMovie->stop();
    }
    if (m_explosionItem) {
        m_explosionItem->setVisible(false);
    }
}

// 收集品效果
void GameScene::triggerCollectEffect() { // 移除了 position 参数
    // 确保所有必要的指针都是有效的，包括 m_ball
    if (!m_collectEffectMovie || !m_collectEffectItem || !m_collectEffectDurationTimer || !m_ball) {
        qWarning() << "triggerCollectEffect: One or more essential members are null (collect effect movie/item/timer or ball).";
        return;
    }
    if (!m_collectEffectMovie->isValid()) {
        qWarning() << "Collect effect GIF is not valid, cannot play.";
        return;
    }

    // 如果0.6秒计时器已在运行，则先停止
    if (m_collectEffectDurationTimer->isActive()) {
        m_collectEffectDurationTimer->stop();
    }
    // 如果GIF动画已在运行，则先停止
    if (m_collectEffectMovie->state() == QMovie::Running) {
        m_collectEffectMovie->stop();
    }
    m_collectEffectMovie->jumpToFrame(0); // 确保从GIF的第一帧开始播放

    // 获取飞船中心位置用于播放动画
    QPointF effectCenter = m_ball->sceneBoundingRect().center();

    QPixmap originalFrame = m_collectEffectMovie->currentPixmap();
    if (originalFrame.isNull()) {
        qWarning() << "Collect effect GIF current frame is null for triggerCollectEffect.";
        return;
    }
    // 使用收集品大小常量来决定动画大小
    // 确保 DEFAULT_COLLECTIBLE_TARGET_SIZE 是可访问的 (例如，通过包含 collectibleitem.h)
    qreal effectDisplayDiameter = DEFAULT_COLLECTIBLE_TARGET_SIZE * DEFAULT_COLLECTIBLE_EFFECT_SIZE_MULTIPLIER;
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_collectEffectItem->setPixmap(scaledFrame);
    // 使用飞船中心位置设置动画位置
    m_collectEffectItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));
    m_collectEffectItem->setVisible(true);
    m_collectEffectMovie->start();
    m_collectEffectDurationTimer->start(600); // 0.6 秒
}

void GameScene::updateCollectEffectFrame(int frameNumber) {
    Q_UNUSED(frameNumber);
    // 确保所有必要的对象都有效，并且动画正在播放
    if (!m_collectEffectMovie || !m_collectEffectItem || m_collectEffectMovie->state() != QMovie::Running) {
        return;
    }
    // 确保飞船对象有效
    if (!m_ball) {
        return;
    }

    QPixmap originalFrame = m_collectEffectMovie->currentPixmap();
    if (originalFrame.isNull()) {
        qWarning() << "Collect effect GIF update frame is null.";
        return;
    }
    qreal effectDisplayDiameter = DEFAULT_COLLECTIBLE_TARGET_SIZE * DEFAULT_COLLECTIBLE_EFFECT_SIZE_MULTIPLIER;
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_collectEffectItem->setPixmap(scaledFrame);

    // 更新收集品动画的位置以跟随飞船 (如果飞船还可见)
    if (m_ball->isVisible()) {
        QPointF effectCenter = m_ball->sceneBoundingRect().center();
        m_collectEffectItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));
    }
}

void GameScene::hideCollectEffect() {
    // 当GIF自然播放完成时调用 (QMovie::finished)
    if (m_collectEffectItem) {
        m_collectEffectItem->setVisible(false);
    }
    // 如果0.6秒计时器此时仍在运行（说明GIF本身短于0.6秒），则停止它
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) {
        m_collectEffectDurationTimer->stop();
    }
}

void GameScene::forceHideCollectEffect() {
    // 当0.6秒计时器超时时调用
    if (m_collectEffectMovie && m_collectEffectMovie->state() == QMovie::Running) {
        m_collectEffectMovie->stop();
    }
    if (m_collectEffectItem) {
        m_collectEffectItem->setVisible(false);
    }
}

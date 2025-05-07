#include "Gamescene.h"
#include "trackdata.h"
#include "collectibleitem.h"
#include "obstacleitem.h"
#include <QGraphicsView>
#include <QKeyEvent>
#include <QFont>
#include <QRandomGenerator>
// #include <QSoundEffect> // 如果需要音效，取消注释
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDebug>
#include <QtMath>

// QMediaPlayer 和 QUrl 已经在 Gamescene.h 中包含

const qreal ANGLE_TOP = 3 * M_PI / 2.0;
const qreal ANGLE_BOTTOM = M_PI / 2.0;
// const qreal ANGLE_RIGHT = 0.0; // 未使用
// const qreal ANGLE_LEFT = M_PI; // 未使用

GameScene::GameScene(QObject *parent)
    : QGraphicsScene(parent),
    m_ballBrush(Qt::white),
    m_ballPen(Qt::black, 1),
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
    m_backgroundMusicPlayer(nullptr), // <<< 初始化新增成员 >>>
    m_audioOutput(nullptr)            // <<< 初始化新增成员 >>>
{
    setSceneRect(-500, -500, 1000, 1000);

    connect(m_timer, &QTimer::timeout, this, &GameScene::updateGame);
    m_judgmentTimer->setSingleShot(true);
    connect(m_judgmentTimer, &QTimer::timeout, this, &GameScene::hideJudgmentText);
    m_damageCooldownTimer->setSingleShot(true);
    connect(m_damageCooldownTimer, &QTimer::timeout, this, &GameScene::enableDamageTaking);

    // <<< 初始化背景音乐播放器 >>>
    m_backgroundMusicPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_backgroundMusicPlayer->setAudioOutput(m_audioOutput);
    // 假设音乐文件在qrc资源的 /sounds/background_music.mp3
    m_backgroundMusicPlayer->setSource(QUrl("qrc:/music/softmusic.mp3"));
    m_backgroundMusicPlayer->setLoops(QMediaPlayer::Infinite);
    m_audioOutput->setVolume(0.5); // 设置音量为50%

    // 可以在这里尝试播放，或者在 initializeGame 确保游戏真正开始时播放
    // m_backgroundMusicPlayer->play();
    // qDebug() << "[GameScene Constructor] Background music player setup. Status:" << m_backgroundMusicPlayer->mediaStatus()
    //          << "Error:" << m_backgroundMusicPlayer->errorString();
    // <<< 结束背景音乐播放器初始化 >>>


    initializeGame(); // 这会调用游戏的主要设置，包括可能启动音乐
}

GameScene::~GameScene()
{
    // QObject的父子关系会自动处理 m_timer, m_judgmentTimer, m_damageCooldownTimer,
    // m_backgroundMusicPlayer, m_audioOutput 的删除。

    // 手动停止音乐以防万一
    if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->isPlaying()) {
        m_backgroundMusicPlayer->stop();
    }

    qDeleteAll(m_collectibles);
    m_collectibles.clear();
    qDeleteAll(m_obstacles);
    m_obstacles.clear();
    qDeleteAll(m_trackItems);
    m_trackItems.clear();
    // clear() 会删除场景中的 QGraphicsItem，但不会删除 m_tracks 中的 TrackData 对象
    // 也不负责 QList<CollectibleItem*> m_collectibles 中的指针对象的删除（所以上面用qDeleteAll）
}

void GameScene::clearAllCollectibles()
{
    qDebug() << "[GameScene clearAllCollectibles] Before - Count:" << m_collectibles.size();
    for (CollectibleItem* collectible : m_collectibles) {
        if (collectible && collectible->scene() == this) {
            removeItem(collectible);
        }
    }
    qDeleteAll(m_collectibles);
    m_collectibles.clear();
    qDebug() << "[GameScene clearAllCollectibles] After - Count:" << m_collectibles.size();
}

void GameScene::clearAllObstacles()
{
    qDebug() << "[GameScene clearAllObstacles] Before - Count:" << m_obstacles.size();
    for (ObstacleItem* obstacle : m_obstacles) {
        if (obstacle && obstacle->scene() == this) {
            removeItem(obstacle);
        }
    }
    qDeleteAll(m_obstacles);
    m_obstacles.clear();
    qDebug() << "[GameScene clearAllObstacles] After - Count:" << m_obstacles.size();
}


bool GameScene::loadTracksAndCollectiblesFromFile(const QString& filename)
{
    m_tracks.clear();

    QFile loadFile(filename);
    if (!loadFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[GameScene loadLevelData] Failed to open file:" << filename;
        return false;
    }
    QByteArray saveData = loadFile.readAll();
    loadFile.close();
    QJsonParseError parseError;
    QJsonDocument loadDoc = QJsonDocument::fromJson(saveData, &parseError);

    if (loadDoc.isNull()) {
        qWarning() << "[GameScene loadLevelData] Failed to parse JSON:" << parseError.errorString() << "at offset" << parseError.offset;
        return false;
    }
    if (!loadDoc.isArray()) {
        qWarning() << "[GameScene loadLevelData] JSON root is not an array.";
        return false;
    }

    QJsonArray trackArray = loadDoc.array();
    qDebug() << "[GameScene loadLevelData] Loading" << trackArray.size() << "tracks from" << filename;

    for (int i = 0; i < trackArray.size(); ++i) {
        if (!trackArray[i].isObject()){
            qWarning() << "[GameScene loadLevelData] Track data at index" << i << "is not a JSON object.";
            m_tracks.clear();
            return false;
        }
        QJsonObject trackObject = trackArray[i].toObject();
        const QStringList requiredKeys = {"centerX", "centerY", "radius"};
        bool keysValid = true;
        for(const QString& key : requiredKeys) {
            if (!trackObject.contains(key) || !trackObject[key].isDouble()) {
                qWarning() << "[GameScene loadLevelData] Track object at index" << i << "missing or invalid key:" << key;
                keysValid = false; break;
            }
        }
        if (!keysValid) {
            m_tracks.clear();
            return false;
        }

        qreal cx = trackObject["centerX"].toDouble();
        qreal cy = trackObject["centerY"].toDouble();
        qreal r = trackObject["radius"].toDouble();
        qreal tangentDeg = trackObject.contains("tangentAngleDegrees") && trackObject["tangentAngleDegrees"].isDouble()
                               ? trackObject["tangentAngleDegrees"].toDouble() : 0.0;

        if (r <= 0) {
            qWarning() << "[GameScene loadLevelData] Track object at index" << i << "has invalid radius:" << r;
            m_tracks.clear();
            return false;
        }
        TrackData trackData(QPointF(cx, cy), r);
        trackData.tangentAngle = qDegreesToRadians(tangentDeg);
        m_tracks.append(trackData);
        qDebug() << "[GameScene loadLevelData] Loaded track" << i << "Radius:" << r;

        if (trackObject.contains("collectibles") && trackObject["collectibles"].isArray()) {
            QJsonArray collectiblesArray = trackObject["collectibles"].toArray();
            qDebug() << "[GameScene loadLevelData] Track" << i << "has" << collectiblesArray.size() << "collectibles.";
            for (const QJsonValue& collectibleVal : collectiblesArray) {
                if (collectibleVal.isObject()) {
                    QJsonObject collectibleObject = collectibleVal.toObject();
                    if (collectibleObject.contains("angleDegrees") && collectibleObject["angleDegrees"].isDouble()) {
                        qreal angleDeg = collectibleObject["angleDegrees"].toDouble();
                        qreal angleRad = qDegreesToRadians(angleDeg);
                        CollectibleItem *newCollectible = new CollectibleItem(i, angleRad);
                        connect(newCollectible, &CollectibleItem::collectedSignal, this, &GameScene::handleCollectibleCollected);
                        m_collectibles.append(newCollectible);
                    } else {
                        qWarning() << "[GameScene loadLevelData] Collectible for track" << i << "missing 'angleDegrees' or invalid type.";
                    }
                } else {
                    qWarning() << "[GameScene loadLevelData] Collectible entry for track" << i << "is not a JSON object.";
                }
            }
        }

        if (trackObject.contains("obstacles") && trackObject["obstacles"].isArray()) {
            QJsonArray obstaclesArray = trackObject["obstacles"].toArray();
            qDebug() << "[GameScene loadLevelData] Track" << i << "has" << obstaclesArray.size() << "obstacles.";
            for (const QJsonValue& obstacleVal : obstaclesArray) {
                if (obstacleVal.isObject()) {
                    QJsonObject obstacleObject = obstacleVal.toObject();
                    if (obstacleObject.contains("angleDegrees") && obstacleObject["angleDegrees"].isDouble()) {
                        qreal angleDeg = obstacleObject["angleDegrees"].toDouble();
                        qreal angleRad = qDegreesToRadians(angleDeg);
                        ObstacleItem *newObstacle = new ObstacleItem(i, angleRad);
                        connect(newObstacle, &ObstacleItem::hitSignal, this, &GameScene::handleObstacleHit);
                        m_obstacles.append(newObstacle);
                    } else {
                        qWarning() << "[GameScene loadLevelData] Obstacle for track" << i << "missing 'angleDegrees' or invalid type.";
                    }
                } else {
                    qWarning() << "[GameScene loadLevelData] Obstacle entry for track" << i << "is not a JSON object.";
                }
            }
        }
    }

    if (m_tracks.isEmpty() && trackArray.size() > 0) {
        qWarning() << "[GameScene loadLevelData] JSON contained track data, but no tracks were successfully loaded.";
        return false;
    } else if (m_tracks.isEmpty()) {
        qWarning() << "[GameScene loadLevelData] No tracks loaded from" << filename;
    }
    qDebug() << "[GameScene loadLevelData] Successfully loaded" << m_tracks.size() << "tracks,"
             << m_collectibles.size() << "collectibles, and" << m_obstacles.size() << "obstacles.";
    return true;
}

void GameScene::initializeGame()
{
    qDebug() << "[GameScene initializeGame] Called. Current m_gameOver state:" << m_gameOver;

    if (m_timer->isActive()) m_timer->stop();
    if (m_judgmentTimer->isActive()) m_judgmentTimer->stop();
    if (m_damageCooldownTimer->isActive()) m_damageCooldownTimer->stop();
    qDebug() << "[GameScene initializeGame] Timers stopped.";

    // <<< 停止可能正在播放的音乐 (如果从游戏结束状态重新开始) >>>
    if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->isPlaying()) {
        m_backgroundMusicPlayer->stop();
    }
    // <<< 结束停止音乐 >>>

    clearAllCollectibles();
    clearAllObstacles();
    qDebug() << "[GameScene initializeGame] Collectibles and Obstacles cleared.";

    qDeleteAll(m_trackItems);
    m_trackItems.clear();
    qDebug() << "[GameScene initializeGame] m_trackItems cleared.";

    clear(); // 这个会移除所有 scene items, 包括之前添加的 m_ball, m_targetDot 等
    qDebug() << "[GameScene initializeGame] Scene cleared.";

    m_ball = nullptr; // 因为 clear() 会删除它们，所以指针置空
    m_targetDot = nullptr;
    m_healthText = nullptr;
    m_judgmentText = nullptr;
    m_gameOverText = nullptr;
    m_scoreText = nullptr;
    qDebug() << "[GameScene initializeGame] Member pointers reset to nullptr.";

    m_currentTrackIndex = 0;
    m_currentAngle = ANGLE_BOTTOM;
    m_orbitOffset = BALL_RADIUS + ORBIT_PADDING;
    m_canTakeDamage = true;
    m_health = m_maxHealth;
    m_score = 0;
    m_speedLevel = 0;
    m_linearSpeed = BASE_LINEAR_SPEED;
    m_rotationDirection = 1;
    qDebug() << "[GameScene initializeGame] Game state variables reset.";

    QString levelFile = ":/levels/level1.json"; // 确保这个资源路径是正确的
    if (!loadTracksAndCollectiblesFromFile(levelFile)) {
        qCritical() << "[GameScene initializeGame] Failed to load level data from" << levelFile << ". Cannot start game.";
        QGraphicsTextItem* errorText = new QGraphicsTextItem(QString("错误: 无法加载关卡 '%1'").arg(levelFile));
        errorText->setDefaultTextColor(Qt::red);
        errorText->setFont(QFont("Arial", 20));
        addItem(errorText); //addItem 会管理 errorText 的生命周期（如果它被添加到场景中）
        errorText->setPos(-errorText->boundingRect().width()/2, -100);
        m_gameOver = true; // 设置游戏结束状态
        return;
    }
    qDebug() << "[GameScene initializeGame] Level data loaded. Tracks:" << m_tracks.size()
             << "Collectibles:" << m_collectibles.size() << "Obstacles:" << m_obstacles.size();


    QRectF totalBounds;
    if (!m_tracks.isEmpty()) {
        totalBounds = m_tracks[0].bounds;
        for (int i = 0; i < m_tracks.size(); ++i) {
            addTrackItem(m_tracks[i]);
            if (i > 0) totalBounds = totalBounds.united(m_tracks[i].bounds);
        }
        if (totalBounds.isValid() && totalBounds.width() > 0 && totalBounds.height() > 0) {
            qreal adjustX = totalBounds.width() / 8.0;
            qreal adjustY = totalBounds.height() / 8.0;
            setSceneRect(totalBounds.adjusted(-adjustX, -adjustY, adjustX, adjustY));
        } else {
            qWarning() << "[GameScene initializeGame] totalBounds invalid, using default scene rect.";
            setSceneRect(-500, -500, 1000, 1000);
        }
    } else {
        qWarning() << "[GameScene initializeGame] No tracks loaded, using default scene rect.";
        setSceneRect(-500, -500, 1000, 1000); // 默认场景大小
    }
    qDebug() << "[GameScene initializeGame] Track items created and scene rect set.";

    if (!m_tracks.isEmpty()) {
        m_ball = new QGraphicsEllipseItem(-BALL_RADIUS, -BALL_RADIUS, 2 * BALL_RADIUS, 2 * BALL_RADIUS);
        m_ball->setBrush(m_ballBrush);
        m_ball->setPen(m_ballPen);
        m_ball->setZValue(1);
        addItem(m_ball);
        qDebug() << "[GameScene initializeGame] Ball created.";
    } else {
        qWarning() << "[GameScene initializeGame] No tracks, ball not created.";
    }

    if (!m_tracks.isEmpty()) {
        m_targetDot = new QGraphicsEllipseItem(-TARGET_DOT_RADIUS, -TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS);
        m_targetDot->setBrush(m_targetBrush);
        m_targetDot->setPen(m_targetPen);
        m_targetDot->setZValue(1);
        addItem(m_targetDot);
        qDebug() << "[GameScene initializeGame] Target dot created.";
    } else {
        qWarning() << "[GameScene initializeGame] No tracks, target dot not created.";
    }

    m_healthText = new QGraphicsTextItem();
    m_healthText->setFont(QFont("Arial", 16, QFont::Bold));
    m_healthText->setZValue(2);
    addItem(m_healthText);

    m_judgmentText = new QGraphicsTextItem();
    m_judgmentText->setFont(QFont("Arial", 20, QFont::Bold));
    m_judgmentText->setZValue(2);
    m_judgmentText->setVisible(false);
    addItem(m_judgmentText);

    m_scoreText = new QGraphicsTextItem();
    m_scoreText->setFont(QFont("Arial", 16, QFont::Bold));
    m_scoreText->setDefaultTextColor(Qt::white);
    m_scoreText->setZValue(2);
    addItem(m_scoreText);
    qDebug() << "[GameScene initializeGame] HUD text items created.";

    positionAndShowCollectibles();
    positionAndShowObstacles();
    qDebug() << "[GameScene initializeGame] Collectibles and Obstacles positioned and shown.";

    updateBallPosition();
    updateTargetDotPosition();
    updateHealthDisplay();
    updateScoreDisplayAndSpeed();
    qDebug() << "[GameScene initializeGame] Game elements positions updated.";

    m_gameOver = false; // 现在游戏正式开始
    qDebug() << "[GameScene initializeGame] Finished. m_gameOver is now" << m_gameOver;

    // <<< 启动背景音乐 >>>
    if (m_backgroundMusicPlayer) {
        if (m_backgroundMusicPlayer->mediaStatus() == QMediaPlayer::NoMedia ||
            m_backgroundMusicPlayer->mediaStatus() == QMediaPlayer::LoadedMedia ||
            m_backgroundMusicPlayer->mediaStatus() == QMediaPlayer::StoppedState) {
            m_backgroundMusicPlayer->play();
        } else if (m_backgroundMusicPlayer->mediaStatus() == QMediaPlayer::PausedState) {
            m_backgroundMusicPlayer->play();
        }
        qDebug() << "[GameScene initializeGame] Attempted to play background music. Status:" << m_backgroundMusicPlayer->mediaStatus()
                 << "Error:" << m_backgroundMusicPlayer->errorString();
    }
    // <<< 结束启动背景音乐 >>>

    if (!m_timer->isActive()) {
        m_timer->start(16); // 约 60 FPS
        qDebug() << "[GameScene initializeGame] Main timer started.";
    } else {
        qWarning() << "[GameScene initializeGame] Timer was already active, restarting.";
        m_timer->stop();
        m_timer->start(16);
    }
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
    qDebug() << "[GameScene positionAndShowCollectibles] Called. Processing" << m_collectibles.size() << "collectibles.";
    for (CollectibleItem* collectible : m_collectibles) {
        if (!collectible) {
            qWarning() << "[GameScene positionAndShowCollectibles] Null collectible found. Skipping.";
            continue;
        }
        if (collectible->scene() == nullptr) { // 确保添加到场景中
            addItem(collectible);
        }
        int trackIdx = collectible->getAssociatedTrackIndex();
        if (trackIdx >= 0 && trackIdx < m_tracks.size()) {
            const TrackData& track = m_tracks[trackIdx];
            collectible->updateVisualPosition(track.center, track.radius);
            collectible->setVisibleState(true); // 确保可见
        } else {
            qWarning() << "[GameScene positionAndShowCollectibles] Collectible (ptr:" << static_cast<void*>(collectible)
            << ") has INVALID associatedTrackIndex:" << trackIdx
            << "(Track count:" << m_tracks.size() << "). Hiding it.";
            collectible->setVisibleState(false);
        }
    }
    qDebug() << "[GameScene positionAndShowCollectibles] Finished.";
}

void GameScene::positionAndShowObstacles()
{
    qDebug() << "[GameScene positionAndShowObstacles] Called. Processing" << m_obstacles.size() << "obstacles.";
    for (ObstacleItem* obstacle : m_obstacles) {
        if (!obstacle) {
            qWarning() << "[GameScene positionAndShowObstacles] Null obstacle found. Skipping.";
            continue;
        }
        if (obstacle->scene() == nullptr) { // 确保添加到场景中
            addItem(obstacle);
        }
        int trackIdx = obstacle->getAssociatedTrackIndex();
        if (trackIdx >= 0 && trackIdx < m_tracks.size()) {
            const TrackData& track = m_tracks[trackIdx];
            obstacle->updateVisualPosition(track.center, track.radius);
            obstacle->setVisibleState(true); // 确保可见
        } else {
            qWarning() << "[GameScene positionAndShowObstacles] Obstacle (ptr:" << static_cast<void*>(obstacle)
            << ") has INVALID associatedTrackIndex:" << trackIdx
            << "(Track count:" << m_tracks.size() << "). Hiding it.";
            obstacle->setVisibleState(false);
        }
    }
    qDebug() << "[GameScene positionAndShowObstacles] Finished.";
}


void GameScene::updateGame()
{
    if (m_gameOver) return;
    if (m_tracks.isEmpty() || !m_ball) {
        if (!m_gameOver) {
            qWarning() << "[GameScene updateGame] No tracks or ball. Ending game.";
            endGame();
        }
        return;
    }
    if (m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) {
        qWarning() << "[GameScene updateGame] Current track index" << m_currentTrackIndex << "out of bounds. Ending game.";
        endGame();
        return;
    }

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

    if (m_canTakeDamage) {
        checkCollisions();
    }

    if (!views().isEmpty() && m_ball) {
        QGraphicsView* view = views().first();
        qreal yOffsetScene = 50;
        QPointF targetCenterPoint = QPointF(m_ball->pos().x(), m_ball->pos().y() - yOffsetScene);
        view->centerOn(targetCenterPoint);

        QRectF viewRect = view->mapToScene(view->viewport()->geometry()).boundingRect();

        if (m_healthText) {
            m_healthText->setPos(viewRect.topLeft() + QPointF(10, 10));
        }
        if (m_scoreText) {
            QRectF scoreRect = m_scoreText->boundingRect();
            m_scoreText->setPos(viewRect.topRight() + QPointF(-scoreRect.width() - 10, 10));
        }
        if (m_gameOverText && m_gameOverText->isVisible()) {
            QRectF gameOverRect = m_gameOverText->boundingRect();
            QPointF viewCenter = viewRect.center();
            m_gameOverText->setPos(viewCenter - QPointF(gameOverRect.width() / 2.0, gameOverRect.height() / 2.0));
        }
    }
}

void GameScene::updateBallPosition() {
    if (!m_ball || m_tracks.isEmpty() || m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) return;

    const TrackData& currentTrackData = m_tracks[m_currentTrackIndex];
    qreal effectiveRadius = currentTrackData.radius + m_orbitOffset;
    if (effectiveRadius < BALL_RADIUS && m_orbitOffset < 0) effectiveRadius = BALL_RADIUS;
    else if (effectiveRadius < 0 && m_orbitOffset > 0) effectiveRadius = BALL_RADIUS;


    qreal x = currentTrackData.center.x() + effectiveRadius * qCos(m_currentAngle);
    qreal y = currentTrackData.center.y() + effectiveRadius * qSin(m_currentAngle);
    m_ball->setPos(x, y); // setPos 设置的是图形项的 (0,0) 点在场景中的位置，对于中心在(0,0)的圆，这恰好是中心
}

void GameScene::updateTargetDotPosition() {
    if (!m_targetDot || m_tracks.isEmpty() || m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) {
        if(m_targetDot) m_targetDot->setVisible(false);
        return;
    }

    const TrackData& currentTrackData = m_tracks[m_currentTrackIndex];
    qreal judgmentAngle = ANGLE_TOP; // 通常在轨道的顶部

    QPointF judgmentPoint = currentTrackData.center + QPointF(currentTrackData.radius * qCos(judgmentAngle), currentTrackData.radius * qSin(judgmentAngle));
    m_targetDot->setPos(judgmentPoint); // 同理，setPos 设置的是目标点的中心
    m_targetDot->setVisible(true);
}

void GameScene::keyPressEvent(QKeyEvent *event)
{
    if (m_gameOver) {
        if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
            qDebug() << "[GameScene keyPressEvent] GameOver: Space pressed, calling initializeGame().";
            initializeGame(); // 这会处理音乐的重新开始
            event->accept();
            return;
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
            qDebug() << "[GameScene keyPressEvent] Already on the last track, cannot switch.";
            event->accept();
            return;
        }

        const TrackData& currentTrackData = m_tracks[m_currentTrackIndex];
        qreal targetAngle = ANGLE_TOP;
        qreal effectiveBallRadiusOnTrack = currentTrackData.radius + m_orbitOffset;
        if (effectiveBallRadiusOnTrack < BALL_RADIUS) effectiveBallRadiusOnTrack = BALL_RADIUS;

        qreal angleDiff = m_currentAngle - targetAngle;
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
        bool showJudgment = false;

        if (absAngleDiff <= perfectAngleTolerance) {
            judgmentTextStr = "PERFECT!"; textColor = Qt::yellow; success = true;
            scoreGainedThisHit = SCORE_PERFECT; showJudgment = true;
        } else if (absAngleDiff <= goodAngleTolerance) {
            judgmentTextStr = "GOOD!"; textColor = Qt::cyan; success = true;
            scoreGainedThisHit = SCORE_GOOD; showJudgment = true;
        } else {
            m_health--; updateHealthDisplay();
            qDebug() << "[GameScene keyPressEvent] Switch missed! Health:" << m_health << "Angle diff (deg):" << qRadiansToDegrees(absAngleDiff);
            if (m_health <= 0) { endGame(); event->accept(); return; }
        }

        if (showJudgment && m_judgmentText) {
            m_judgmentText->setPlainText(judgmentTextStr);
            m_judgmentText->setDefaultTextColor(textColor);
            m_judgmentText->setVisible(true);
            QPointF textPos;
            if (m_targetDot && m_targetDot->isVisible()) {
                QPointF targetDotCenter = m_targetDot->pos();
                QRectF judgmentRect = m_judgmentText->boundingRect();
                textPos = QPointF(targetDotCenter.x() - judgmentRect.width() / 2.0,
                                  targetDotCenter.y() - judgmentRect.height() - TARGET_DOT_RADIUS - 5);
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
            m_score += scoreGainedThisHit; updateScoreDisplayAndSpeed(); switchTrack();
        }
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_J && !event->isAutoRepeat()) {
        m_orbitOffset = -m_orbitOffset;
        updateBallPosition();
        qDebug() << "[GameScene keyPressEvent] Orbit switched to offset:" << m_orbitOffset;
        event->accept();
        return;
    }

    QGraphicsScene::keyPressEvent(event);
}


void GameScene::hideJudgmentText() {
    if (m_judgmentText) {
        m_judgmentText->setVisible(false);
    }
}

void GameScene::enableDamageTaking() {
    m_canTakeDamage = true;
}

void GameScene::checkCollisions() {
    if (m_orbitOffset <= 0 || !m_ball || m_tracks.isEmpty() || !m_canTakeDamage || m_currentTrackIndex < 0) return;

    QPointF ballCenter = m_ball->pos();
    bool collisionOccurred = false;

    for (int i = 0; i < m_tracks.size(); ++i) {
        if (i == m_currentTrackIndex) continue;
        const TrackData& otherTrack = m_tracks[i];
        qreal distanceToOtherTrackCenter = QLineF(ballCenter, otherTrack.center).length();
        if (distanceToOtherTrackCenter < otherTrack.radius + BALL_RADIUS) { // 球体碰撞检测
            collisionOccurred = true;
            qDebug() << "[GameScene checkCollisions] Collision with track" << i;
            break;
        }
    }

    if (collisionOccurred) {
        m_health--; updateHealthDisplay();
        m_orbitOffset = -(BALL_RADIUS + ORBIT_PADDING); updateBallPosition();
        m_canTakeDamage = false; m_damageCooldownTimer->start(DAMAGE_COOLDOWN_MS);
        qDebug() << "[GameScene checkCollisions] Track Collision! Health:" << m_health << ". Cooldown started.";
        if (m_health <= 0) { endGame(); }
    }
}

void GameScene::checkBallCollectibleCollisions()
{
    if (!m_ball) return;
    QList<QGraphicsItem*> collidingItemsList = m_ball->collidingItems();
    for (QGraphicsItem* item : collidingItemsList) {
        CollectibleItem* collectible = dynamic_cast<CollectibleItem*>(item);
        if (collectible && !collectible->isCollected()) {
            collectible->collect();
        }
    }
}

void GameScene::checkBallObstacleCollisions()
{
    if (!m_ball) return;
    QList<QGraphicsItem*> collidingItemsList = m_ball->collidingItems();
    for (QGraphicsItem* item : collidingItemsList) {
        ObstacleItem* obstacle = dynamic_cast<ObstacleItem*>(item);
        if (obstacle && !obstacle->isHit()) {
            obstacle->processHit();
        }
    }
}

void GameScene::handleCollectibleCollected(CollectibleItem* item)
{
    if (!item) return;
    m_score += item->getScoreValue();
    updateScoreDisplayAndSpeed();
    qDebug() << "[GameScene handleCollectibleCollected] Collected item from track" << item->getAssociatedTrackIndex() << "! Score:" << m_score;
    // 可以在这里添加音效等
}

void GameScene::handleObstacleHit(ObstacleItem* item)
{
    if (!item) return;

    m_health--;
    updateHealthDisplay();
    qDebug() << "[GameScene handleObstacleHit] Hit obstacle on track" << item->getAssociatedTrackIndex() << "! Health:" << m_health;

    // 示例音效 (需要 QSoundEffect 和 .wav 文件在资源中)
    // QSoundEffect* hitSound = new QSoundEffect(this);
    // hitSound->setSource(QUrl("qrc:/sounds/obstacle_hit.wav"));
    // hitSound->setVolume(0.5);
    // hitSound->play();
    // connect(hitSound, &QSoundEffect::playingChanged, [hitSound](){ if(!hitSound->isPlaying()) hitSound->deleteLater(); });

    if (m_health <= 0) {
        endGame();
    }
}


void GameScene::updateHealthDisplay() {
    if (m_healthText) {
        m_healthText->setPlainText(QString("生命: %1 / %2").arg(m_health).arg(m_maxHealth));
        if (m_health <= 0) {
            m_healthText->setDefaultTextColor(Qt::darkRed);
        } else if (m_health > m_maxHealth * 0.6) {
            m_healthText->setDefaultTextColor(Qt::darkGreen);
        } else if (m_health > m_maxHealth * 0.3) {
            m_healthText->setDefaultTextColor(Qt::darkYellow);
        } else {
            m_healthText->setDefaultTextColor(Qt::red);
        }
    }
}

void GameScene::updateScoreDisplayAndSpeed() {
    if (m_scoreText) {
        m_scoreText->setPlainText(QString("分数: %1").arg(m_score));
    }

    int targetLevel = m_score / SCORE_THRESHOLD_FOR_SPEEDUP;
    if (targetLevel > m_speedLevel) {
        m_speedLevel = targetLevel;
        m_linearSpeed = BASE_LINEAR_SPEED * qPow(SPEEDUP_FACTOR, m_speedLevel);
        qDebug() << "[GameScene updateScoreDisplayAndSpeed] Level up! New speed level:" << m_speedLevel << "Linear speed:" << m_linearSpeed;
    }
}

void GameScene::switchTrack() {
    if (m_currentTrackIndex + 1 >= m_tracks.size()) {
        qDebug() << "[GameScene switchTrack] Already on the last track. Victory or no switch.";
        // 可以在这里处理游戏胜利
        return;
    }
    qDebug() << "[GameScene switchTrack] Switching track: from" << m_currentTrackIndex << "to" << m_currentTrackIndex + 1;
    m_currentTrackIndex++;
    m_rotationDirection = -m_rotationDirection;
    m_currentAngle = ANGLE_BOTTOM; // 新轨道从底部开始
    m_orbitOffset = qAbs(m_orbitOffset); // 确保是外圈开始

    updateTargetDotPosition();
    updateBallPosition();
}

void GameScene::endGame() {
    if(m_gameOver && m_gameOverText && m_gameOverText->isVisible() && !m_timer->isActive()) {
        qDebug() << "[GameScene endGame] Already in game over state and displayed. Aborting.";
        return;
    }

    qDebug() << "[GameScene endGame] Called. Current m_gameOver (before set):" << m_gameOver;
    m_gameOver = true;

    if (m_timer->isActive()) m_timer->stop();
    if (m_judgmentTimer->isActive()) m_judgmentTimer->stop();
    if (m_damageCooldownTimer->isActive()) m_damageCooldownTimer->stop();
    qDebug() << "[GameScene endGame] All timers stopped.";

    // <<< 停止背景音乐 >>>
    if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->isPlaying()) {
        m_backgroundMusicPlayer->stop();
        qDebug() << "[GameScene endGame] Background music stopped.";
    }
    // <<< 结束停止背景音乐 >>>

    if (m_targetDot) m_targetDot->setVisible(false);
    if (m_ball) m_ball->setVisible(false);
    qDebug() << "[GameScene endGame] Ball and target dot hidden.";

    for (CollectibleItem* collectible : m_collectibles) {
        if (collectible) collectible->setVisibleState(false);
    }
    for (ObstacleItem* obstacle : m_obstacles) {
        if (obstacle) obstacle->setVisibleState(false);
    }
    qDebug() << "[GameScene endGame] All collectibles and obstacles hidden.";

    if (m_healthText) m_healthText->setVisible(false);
    if (m_judgmentText) m_judgmentText->setVisible(false);
    if (m_scoreText) m_scoreText->setVisible(false);
    qDebug() << "[GameScene endGame] HUD elements hidden.";

    qDebug() << "[GameScene endGame] Final score:" << m_score;

    if (!m_gameOverText) { // 如果指针为空 (例如在 initializeGame 中被 clear()删除了)
        m_gameOverText = new QGraphicsTextItem();
        addItem(m_gameOverText);
        qDebug() << "[GameScene endGame] Created new GameOverText and added to scene.";
    } else { // 如果指针存在，确保它在场景中
        qDebug() << "[GameScene endGame] Reusing existing GameOverText.";
        if(m_gameOverText->scene() == nullptr) { // 如果不在场景中，则添加回去
            addItem(m_gameOverText);
            qWarning() << "[GameScene endGame] GameOverText existed but was not in scene. Re-added.";
        }
    }

    m_gameOverText->setPlainText(QString("游戏结束\n最终分数: %1\n按 空格键 重新开始").arg(m_score));
    m_gameOverText->setFont(QFont("Arial", 24, QFont::Bold));
    m_gameOverText->setDefaultTextColor(Qt::red);
    m_gameOverText->setZValue(3); // 确保在最顶层
    m_gameOverText->setVisible(true);
    qDebug() << "[GameScene endGame] GameOverText content set and visible.";

    if (!views().isEmpty()) {
        QGraphicsView* view = views().first();
        QRectF viewRect = view->mapToScene(view->viewport()->geometry()).boundingRect();
        QRectF gameOverRect = m_gameOverText->boundingRect();
        QPointF viewCenter = viewRect.center();
        m_gameOverText->setPos(viewCenter - QPointF(gameOverRect.width() / 2.0, gameOverRect.height() / 2.0));
        qDebug() << "[GameScene endGame] GameOverText centered in view.";
    } else {
        // Fallback if no view is available (should not happen in normal operation)
        m_gameOverText->setPos(-m_gameOverText->boundingRect().width()/2, -m_gameOverText->boundingRect().height()/2);
        qWarning() << "[GameScene endGame] No view available to center GameOverText. Centered at scene origin.";
    }
    qDebug() << "[GameScene endGame] Execution finished.";
}

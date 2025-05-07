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
#include <QGraphicsPixmapItem> // 确保包含
#include <QPixmap>           // 确保包含


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
    m_backgroundMusicPlayer(nullptr),
    m_audioOutput(nullptr),
    m_backgroundItem(nullptr) // 初始化背景项指针
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
    m_backgroundMusicPlayer->setSource(QUrl("qrc:/music/softmusic.mp3")); // 确保路径正确
    m_backgroundMusicPlayer->setLoops(QMediaPlayer::Infinite);
    m_audioOutput->setVolume(0.5);

    connect(m_backgroundMusicPlayer, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status){
                qDebug() << "[QMediaPlayer] MediaStatusChanged:" << status;
                if (status == QMediaPlayer::LoadedMedia) {
                    // 当媒体加载完成且当前未在播放时，开始播放
                    if (m_backgroundMusicPlayer->playbackState() != QMediaPlayer::PlayingState) {
                        qDebug() << "[QMediaPlayer] Media is LoadedMedia and not playing, calling play().";
                        m_backgroundMusicPlayer->play();
                    }
                }
            });

    connect(m_backgroundMusicPlayer, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString &errorString){
                qDebug() << "[QMediaPlayer] ErrorOccurred:" << error << "ErrorString:" << errorString;
            });

    connect(m_backgroundMusicPlayer, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState newState){
                qDebug() << "[QMediaPlayer] PlaybackStateChanged:" << newState;
            });
    // <<< 结束背景音乐播放器初始化 >>>


    initializeGame(); // 调用游戏初始化
}

GameScene::~GameScene()
{
    // 停止音乐
    if (m_backgroundMusicPlayer) {
        m_backgroundMusicPlayer->stop();
    }

    // 清理其他资源（Qt的父子关系会自动处理大部分图形项和计时器）
    qDeleteAll(m_collectibles);
    m_collectibles.clear();
    qDeleteAll(m_obstacles);
    m_obstacles.clear();
    qDeleteAll(m_trackItems); // m_trackItems 存储的是 QGraphicsEllipseItem 指针
    m_trackItems.clear();
    // 注意：m_backgroundItem 如果被添加到场景中，其生命周期由场景管理，
    // 或者如果它是 GameScene 的子对象，则由 GameScene 管理。
    // 如果手动 new 且未设置父对象或未添加到场景，则需要 delete。
    // 但由于 addItem() 会接管所有权，通常不需要手动 delete。
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

    // 不再在此处停止音乐
    // if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->isPlaying()) { ... }

    clearAllCollectibles();
    clearAllObstacles();
    qDebug() << "[GameScene initializeGame] Collectibles and Obstacles cleared.";

    qDeleteAll(m_trackItems); // 清理视觉轨道项列表
    m_trackItems.clear();
    qDebug() << "[GameScene initializeGame] m_trackItems cleared.";

    // 先不调用 clear()，以便保留背景项（如果已创建）
    // 如果背景项也需要每次重建，则保留 clear()，并在下面总是创建背景
    // clear();
    // qDebug() << "[GameScene initializeGame] Scene cleared.";

    // --- 添加或检查背景 ---
    if (!m_backgroundItem) { // 检查指针是否为空
        QPixmap bgPixmap(":/images/background.png"); // 从资源加载
        if (bgPixmap.isNull()) {
            qWarning() << "Failed to load background image from resource ':/images/background.png'";
            setBackgroundBrush(Qt::darkGray); // 设置一个深灰色备用背景
        } else {
            m_backgroundItem = new QGraphicsPixmapItem(bgPixmap);
            // 设置位置，尝试覆盖可能的场景范围，具体数值需要调整
            QRectF rect = sceneRect(); // 获取当前场景矩形
            //  // 尝试让背景覆盖场景矩形，假设图片左上角与场景左上角对齐
            //  m_backgroundItem->setPos(rect.topLeft());
            // 或者，如果背景图很大，尝试居中放置
            m_backgroundItem->setPos(rect.center().x() - bgPixmap.width() / 2.0, rect.center().y() - bgPixmap.height() / 2.0);
            m_backgroundItem->setZValue(-1.0); // 确保在最底层
            addItem(m_backgroundItem);         // 添加到场景
            qDebug() << "[GameScene initializeGame] Background item added.";
        }
    } else {
        // 如果保留了 clear()，则这里需要重新添加 m_backgroundItem
        // if (m_backgroundItem->scene() == nullptr) {
        //     addItem(m_backgroundItem);
        // }
        qDebug() << "[GameScene initializeGame] Background item already exists.";
        // 可能需要根据新的 sceneRect 调整背景位置
        // QRectF rect = sceneRect();
        // m_backgroundItem->setPos(rect.center().x() - m_backgroundItem->pixmap().width() / 2.0, rect.center().y() - m_backgroundItem->pixmap().height() / 2.0);
    }
    // 如果上面没有调用 clear()，需要手动移除旧的游戏元素 (除了背景)
    QList<QGraphicsItem*> itemsToRemove;
    for (QGraphicsItem* item : items()) {
        if (item != m_backgroundItem) { // 不要移除背景
            itemsToRemove.append(item);
        }
    }
    for (QGraphicsItem* item : itemsToRemove) {
        removeItem(item);
        delete item; // 确保释放内存
    }
    qDebug() << "[GameScene initializeGame] Removed old game elements (excluding background).";


    m_ball = nullptr;
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

    QString levelFile = ":/levels/level1.json";
    if (!loadTracksAndCollectiblesFromFile(levelFile)) {
        qCritical() << "[GameScene initializeGame] Failed to load level data from" << levelFile << ". Cannot start game.";
        // ... (错误处理代码) ...
        return;
    }
    qDebug() << "[GameScene initializeGame] Level data loaded. Tracks:" << m_tracks.size()
             << "Collectibles:" << m_collectibles.size() << "Obstacles:" << m_obstacles.size();

    // --- 创建轨道视觉效果 ---
    QRectF totalBounds; // totalBounds 需要重新计算或在此处定义
    if (!m_tracks.isEmpty()) {
        totalBounds = m_tracks[0].bounds;
        for (int i = 0; i < m_tracks.size(); ++i) {
            // 检查 m_trackItems 是否已被清理，如果需要重新添加视觉轨道
            // 这里假设每次都重新创建轨道视觉项
            addTrackItem(m_tracks[i]);
            if (i > 0) totalBounds = totalBounds.united(m_tracks[i].bounds);
        }
        // ... (设置 sceneRect 的逻辑) ...
    } else {
        // ... (处理无轨道的情况) ...
    }
    qDebug() << "[GameScene initializeGame] Track items created and scene rect set.";

    // --- 创建其他游戏元素 ---
    if (!m_tracks.isEmpty()) {
        m_ball = new QGraphicsEllipseItem(-BALL_RADIUS, -BALL_RADIUS, 2 * BALL_RADIUS, 2 * BALL_RADIUS);
        m_ball->setBrush(m_ballBrush);
        m_ball->setPen(m_ballPen);
        m_ball->setZValue(1); // Z值应高于背景
        addItem(m_ball);
        qDebug() << "[GameScene initializeGame] Ball created.";

        m_targetDot = new QGraphicsEllipseItem(-TARGET_DOT_RADIUS, -TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS);
        m_targetDot->setBrush(m_targetBrush);
        m_targetDot->setPen(m_targetPen);
        m_targetDot->setZValue(1); // Z值应高于背景
        addItem(m_targetDot);
        qDebug() << "[GameScene initializeGame] Target dot created.";
    } else {
        qWarning() << "[GameScene initializeGame] No tracks, ball and target dot not created.";
    }

    // --- 创建 HUD ---
    m_healthText = new QGraphicsTextItem();
    m_healthText->setFont(QFont("Arial", 16, QFont::Bold));
    m_healthText->setZValue(2); // Z值高于背景和游戏元素
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

    // --- 放置和显示动态元素 ---
    positionAndShowCollectibles();
    positionAndShowObstacles();
    qDebug() << "[GameScene initializeGame] Collectibles and Obstacles positioned and shown.";

    // --- 更新初始位置和显示 ---
    updateBallPosition();
    updateTargetDotPosition();
    updateHealthDisplay();
    updateScoreDisplayAndSpeed();
    qDebug() << "[GameScene initializeGame] Game elements positions updated.";

    m_gameOver = false;
    qDebug() << "[GameScene initializeGame] Finished. m_gameOver is now" << m_gameOver;

    // --- 音乐播放由信号处理，此处不再调用 play ---
    // 添加日志确认当前音乐状态
    if (m_backgroundMusicPlayer) {
        qDebug() << "[GameScene initializeGame] Music player status on game init exit:" << m_backgroundMusicPlayer->mediaStatus()
        << "Playback state:" << m_backgroundMusicPlayer->playbackState();
    }

    // --- 启动游戏循环计时器 ---
    if (!m_timer->isActive()) {
        m_timer->start(16);
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
    trackItem->setZValue(0); // Z 值低于球 (1) 和 HUD (2)，但高于背景 (-1)
    addItem(trackItem);
    m_trackItems.append(trackItem); // 追踪视觉项以便清理
}

void GameScene::positionAndShowCollectibles()
{
    qDebug() << "[GameScene positionAndShowCollectibles] Called. Processing" << m_collectibles.size() << "collectibles.";
    for (CollectibleItem* collectible : m_collectibles) {
        if (!collectible) {
            qWarning() << "[GameScene positionAndShowCollectibles] Null collectible found. Skipping.";
            continue;
        }
        if (collectible->scene() == nullptr) {
            addItem(collectible);
        }
        int trackIdx = collectible->getAssociatedTrackIndex();
        if (trackIdx >= 0 && trackIdx < m_tracks.size()) {
            const TrackData& track = m_tracks[trackIdx];
            collectible->updateVisualPosition(track.center, track.radius);
            collectible->setZValue(0.5); // 确保 Z 值高于背景
            collectible->setVisibleState(true);
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
        if (obstacle->scene() == nullptr) {
            addItem(obstacle);
        }
        int trackIdx = obstacle->getAssociatedTrackIndex();
        if (trackIdx >= 0 && trackIdx < m_tracks.size()) {
            const TrackData& track = m_tracks[trackIdx];
            obstacle->updateVisualPosition(track.center, track.radius);
            obstacle->setZValue(0.6); // 确保 Z 值高于背景
            obstacle->setVisibleState(true);
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

    // --- 更新球的位置 ---
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

    // --- 碰撞检测 ---
    checkBallCollectibleCollisions();
    checkBallObstacleCollisions();

    if (m_canTakeDamage) {
        checkCollisions();
    }

    // --- 视图和 HUD 更新 ---
    if (!views().isEmpty() && m_ball) {
        QGraphicsView* view = views().first();
        qreal yOffsetScene = 50; // 视图焦点稍微向上偏移，让玩家能看到前方更多内容
        QPointF targetCenterPoint = m_ball->pos() + QPointF(0, -yOffsetScene); // ball->pos() 已经是中心点
        view->centerOn(targetCenterPoint);

        // 更新 HUD 位置相对于视图
        QRectF viewRect = view->mapToScene(view->viewport()->geometry()).boundingRect();

        if (m_healthText) {
            m_healthText->setPos(viewRect.topLeft() + QPointF(10, 10));
        }
        if (m_scoreText) {
            QRectF scoreRect = m_scoreText->boundingRect();
            m_scoreText->setPos(viewRect.topRight() + QPointF(-scoreRect.width() - 10, 10));
        }
        // Game Over 文本的位置在 endGame 中设置，但这里确保它在视图滚动时保持居中（如果可见）
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
    m_ball->setPos(x, y); // setPos 设置图形项局部坐标(0,0)点在父坐标系（场景）中的位置
        // 对于以(0,0)为中心创建的 QGraphicsEllipseItem, 这恰好是其中心点
}

void GameScene::updateTargetDotPosition() {
    if (!m_targetDot || m_tracks.isEmpty() || m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) {
        if(m_targetDot) m_targetDot->setVisible(false);
        return;
    }

    const TrackData& currentTrackData = m_tracks[m_currentTrackIndex];
    qreal judgmentAngle = ANGLE_TOP;

    QPointF judgmentPoint = currentTrackData.center + QPointF(currentTrackData.radius * qCos(judgmentAngle), currentTrackData.radius * qSin(judgmentAngle));
    m_targetDot->setPos(judgmentPoint); // 同理，设置中心点
    m_targetDot->setVisible(true);
}

// ... (keyPressEvent, hideJudgmentText, enableDamageTaking, checkCollisions,
//      checkBallCollectibleCollisions, checkBallObstacleCollisions,
//      handleCollectibleCollected, handleObstacleHit,
//      updateHealthDisplay, updateScoreDisplayAndSpeed, switchTrack 基本保持不变) ...

// --- keyPressEvent, hideJudgmentText, enableDamageTaking 等函数保持不变 ---
// --- ... 省略这些函数的代码，假设它们没有改动 ... ---
void GameScene::keyPressEvent(QKeyEvent *event)
{
    if (m_gameOver) {
        if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
            qDebug() << "[GameScene keyPressEvent] GameOver: Space pressed, calling initializeGame().";
            initializeGame();
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
                QPointF targetDotCenter = m_targetDot->pos(); // .pos() is center
                QRectF judgmentRect = m_judgmentText->boundingRect();
                textPos = QPointF(targetDotCenter.x() - judgmentRect.width() / 2.0,
                                  targetDotCenter.y() - judgmentRect.height() - TARGET_DOT_RADIUS - 5);
            } else if (!views().isEmpty()) {
                QRectF viewRect = views().first()->mapToScene(views().first()->viewport()->geometry()).boundingRect();
                QRectF judgmentRect = m_judgmentText->boundingRect();
                textPos = viewRect.center() - QPointF(judgmentRect.width() / 2.0, judgmentRect.height() /2.0 + 50); // Position relative to view center
            } else {
                textPos = QPointF(-m_judgmentText->boundingRect().width()/2, -50); // Fallback
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

    QPointF ballCenter = m_ball->pos(); // Get ball center
    bool collisionOccurred = false;

    for (int i = 0; i < m_tracks.size(); ++i) {
        if (i == m_currentTrackIndex) continue; // Skip collision with current track
        const TrackData& otherTrack = m_tracks[i];
        qreal distanceToOtherTrackCenter = QLineF(ballCenter, otherTrack.center).length();
        // Simple circle-circle collision check
        if (distanceToOtherTrackCenter < otherTrack.radius + BALL_RADIUS) {
            collisionOccurred = true;
            qDebug() << "[GameScene checkCollisions] Collision with track" << i;
            break;
        }
    }

    if (collisionOccurred) {
        m_health--; updateHealthDisplay();
        // Force ball to inner orbit after collision
        m_orbitOffset = -(BALL_RADIUS + ORBIT_PADDING);
        updateBallPosition(); // Update position immediately
        m_canTakeDamage = false; // Start cooldown
        m_damageCooldownTimer->start(DAMAGE_COOLDOWN_MS);
        qDebug() << "[GameScene checkCollisions] Track Collision! Health:" << m_health << ". Cooldown started.";
        if (m_health <= 0) {
            endGame();
        }
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
}

void GameScene::handleObstacleHit(ObstacleItem* item)
{
    if (!item) return;

    m_health--;
    updateHealthDisplay();
    qDebug() << "[GameScene handleObstacleHit] Hit obstacle on track" << item->getAssociatedTrackIndex() << "! Health:" << m_health;

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
        return;
    }
    qDebug() << "[GameScene switchTrack] Switching track: from" << m_currentTrackIndex << "to" << m_currentTrackIndex + 1;
    m_currentTrackIndex++;
    m_rotationDirection = -m_rotationDirection;
    m_currentAngle = ANGLE_BOTTOM;
    m_orbitOffset = qAbs(m_orbitOffset);

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

    // MODIFICATION: 不再在此处停止音乐
    // if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->isPlaying()) { ... }

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

    if (!m_gameOverText) {
        m_gameOverText = new QGraphicsTextItem();
        addItem(m_gameOverText);
        qDebug() << "[GameScene endGame] Created new GameOverText and added to scene.";
    } else {
        qDebug() << "[GameScene endGame] Reusing existing GameOverText.";
        if(m_gameOverText->scene() == nullptr) {
            addItem(m_gameOverText);
            qWarning() << "[GameScene endGame] GameOverText existed but was not in scene. Re-added.";
        }
        // 确保 Game Over 文本可见
        m_gameOverText->setVisible(true);
    }

    m_gameOverText->setPlainText(QString("游戏结束\n最终分数: %1\n按 空格键 重新开始").arg(m_score));
    m_gameOverText->setFont(QFont("Arial", 24, QFont::Bold));
    m_gameOverText->setDefaultTextColor(Qt::red);
    m_gameOverText->setZValue(3);
    m_gameOverText->setVisible(true); // 再次确保可见
    qDebug() << "[GameScene endGame] GameOverText content set and visible.";

    // 定位 Game Over 文本到视图中心
    if (!views().isEmpty()) {
        QGraphicsView* view = views().first();
        QRectF viewRect = view->mapToScene(view->viewport()->geometry()).boundingRect();
        QRectF gameOverRect = m_gameOverText->boundingRect();
        QPointF viewCenter = viewRect.center();
        m_gameOverText->setPos(viewCenter - QPointF(gameOverRect.width() / 2.0, gameOverRect.height() / 2.0));
        qDebug() << "[GameScene endGame] GameOverText centered in view.";
    } else {
        m_gameOverText->setPos(-m_gameOverText->boundingRect().width()/2, -m_gameOverText->boundingRect().height()/2);
        qWarning() << "[GameScene endGame] No view available to center GameOverText. Centered at scene origin.";
    }
    qDebug() << "[GameScene endGame] Execution finished.";
}

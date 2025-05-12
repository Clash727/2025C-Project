// gamescene.cpp
#include "gamescene.h"
#include "trackdata.h"
#include "collectibleitem.h"
#include "obstacleitem.h"
#include "gameoverdisplay.h"
#include "endtriggeritem.h" // 包含 EndTriggerItem
#include <QGraphicsView>
#include <QKeyEvent>
#include <QFont>
#include <QFontDatabase>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDebug> // 确保 QDebug 被包含
#include <QtMath>
#include <QGraphicsPixmapItem>
#include <QMovie>
#include <QGraphicsEllipseItem>
#include <QtGlobal> // For QT_VERSION_CHECK

// --- 游戏常量 ---
const qreal ANGLE_TOP = 3 * M_PI / 2.0;
const qreal ANGLE_BOTTOM = M_PI / 2.0;
// END_POINT_RADIUS 在 endtriggeritem.h 中定义为 DEFAULT_END_POINT_RADIUS
// 其他常量如 BALL_RADIUS, ORBIT_PADDING 等应在 gamescene.h 中定义
// 假设这些常量已在 gamescene.h 或其他地方定义

// 轨道碰撞有效半径调整因子。值小于1.0会减少轨道间碰撞的敏感度，1.0为原始行为。
// 值越小，飞船需要更深入地“侵入”其他轨道的范围才会判定为碰撞。
const qreal TRACK_COLLISION_EFFECTIVE_RADIUS_FACTOR = 0.8; // 您可以调整此值 (例如 0.8, 0.9, 0.7等)


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
    m_maxHealth(10z), // 假设最大生命值为3
    m_health(m_maxHealth),
    m_ball(nullptr),
    m_targetDot(nullptr),
    m_sunItem(nullptr),
    m_mercuryItem(nullptr),
    m_venusItem(nullptr),
    m_earthItem(nullptr),
    m_marsItem(nullptr),
    m_jupiterItem(nullptr),
    m_saturnItem(nullptr),
    m_uranusItem(nullptr),
    m_neptuneItem(nullptr),
    m_healthText(nullptr),
    m_judgmentText(nullptr),
    m_endTriggerPoint(nullptr),
    m_scoreText(nullptr),
    m_gameOverDisplay(nullptr),
    m_timer(new QTimer(this)),
    m_judgmentTimer(new QTimer(this)),
    m_damageCooldownTimer(new QTimer(this)),
    m_explosionMovie(nullptr),
    m_collectEffectMovie(nullptr),
    m_explosionDurationTimer(nullptr),
    m_collectEffectDurationTimer(nullptr),
    m_backgroundMusicPlayer(nullptr),
    m_audioOutput(nullptr),
    m_explosionItem(nullptr),
    m_collectEffectItem(nullptr),
    m_englishFontFamily("Arial"),
    m_chineseFontFamily("SimSun")
{
    setSceneRect(-2000, -2000, 4000, 4000);

    // --- 背景初始化 ---
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

    // --- 背景音乐设置 ---
    m_backgroundMusicPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_backgroundMusicPlayer->setAudioOutput(m_audioOutput);
    m_backgroundMusicPlayer->setSource(QUrl("qrc:/music/softmusic.mp3"));
    m_backgroundMusicPlayer->setLoops(QMediaPlayer::Infinite);
    m_audioOutput->setVolume(0.5); // Qt6: setVolume takes float 0.0-1.0
    connect(m_backgroundMusicPlayer, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status){
                if (status == QMediaPlayer::LoadedMedia &&
                    m_backgroundMusicPlayer->playbackState() != QMediaPlayer::PlayingState &&
                    !m_gameOver && m_timer && m_timer->isActive()) {
                    m_backgroundMusicPlayer->play();
                }
            });
    connect(m_backgroundMusicPlayer, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString &errorString){
                qDebug() << "[QMediaPlayer] ErrorOccurred:" << error << "ErrorString:" << errorString;
            });


    // --- 动画 QMovie 和相关 Timer 初始化 ---
    m_explosionMovie = new QMovie(":/animations/explosion.gif", QByteArray(), this);
    if (!m_explosionMovie->isValid()) {
        qWarning() << "Failed to load explosion GIF: :/animations/explosion.gif";
    }
    connect(m_explosionMovie, &QMovie::frameChanged, this, &GameScene::updateExplosionFrame);
    connect(m_explosionMovie, &QMovie::finished, this, &GameScene::hideExplosionEffect); // QMovie::finished is correct
    m_explosionDurationTimer = new QTimer(this);
    m_explosionDurationTimer->setSingleShot(true);
    connect(m_explosionDurationTimer, &QTimer::timeout, this, &GameScene::forceHideExplosion);

    m_collectEffectMovie = new QMovie(":/animations/collect_effect.gif", QByteArray(), this);
    if (!m_collectEffectMovie->isValid()) {
        qWarning() << "Failed to load collect effect GIF: :/animations/collect_effect.gif";
    }
    connect(m_collectEffectMovie, &QMovie::frameChanged, this, &GameScene::updateCollectEffectFrame);
    connect(m_collectEffectMovie, &QMovie::finished, this, &GameScene::hideCollectEffect); // QMovie::finished is correct
    m_collectEffectDurationTimer = new QTimer(this);
    m_collectEffectDurationTimer->setSingleShot(true);
    connect(m_collectEffectDurationTimer, &QTimer::timeout, this, &GameScene::forceHideCollectEffect);

    initializeGame();
}

GameScene::~GameScene()
{
    if (m_backgroundMusicPlayer) {
        m_backgroundMusicPlayer->stop();
    }
    qDeleteAll(m_backgroundTiles);
    m_backgroundTiles.clear();
    clearAllGameItems();
    // Timers are children of GameScene, Qt will handle their deletion.
    // QMovie objects are also children.
}

void GameScene::clearAllCollectibles()
{
    for (CollectibleItem* collectible : m_collectibles) {
        if (collectible) {
            if (collectible->scene() == this) {
                removeItem(collectible);
            }
            delete collectible;
        }
    }
    m_collectibles.clear();
}

void GameScene::clearAllObstacles()
{
    for (ObstacleItem* obstacle : m_obstacles) {
        if (obstacle) {
            if (obstacle->scene() == this) {
                removeItem(obstacle);
            }
            delete obstacle;
        }
    }
    m_obstacles.clear();
}

void GameScene::clearAllGameItems() {
    clearAllCollectibles();
    clearAllObstacles();

    qDeleteAll(m_trackItems); // Deletes all QGraphicsEllipseItem* in the list
    m_trackItems.clear();

    if (m_ball && m_ball->scene() == this) { removeItem(m_ball); delete m_ball; m_ball = nullptr; }
    if (m_targetDot && m_targetDot->scene() == this) { removeItem(m_targetDot); delete m_targetDot; m_targetDot = nullptr; }

    if (m_endTriggerPoint && m_endTriggerPoint->scene() == this) {
        removeItem(m_endTriggerPoint);
        delete m_endTriggerPoint;
        m_endTriggerPoint = nullptr;
    }

    // Planet items
    if (m_sunItem && m_sunItem->scene() == this) { removeItem(m_sunItem); delete m_sunItem; m_sunItem = nullptr; }
    if (m_mercuryItem && m_mercuryItem->scene() == this) { removeItem(m_mercuryItem); delete m_mercuryItem; m_mercuryItem = nullptr; }
    if (m_venusItem && m_venusItem->scene() == this) { removeItem(m_venusItem); delete m_venusItem; m_venusItem = nullptr; }
    if (m_earthItem && m_earthItem->scene() == this) { removeItem(m_earthItem); delete m_earthItem; m_earthItem = nullptr; }
    if (m_marsItem && m_marsItem->scene() == this) { removeItem(m_marsItem); delete m_marsItem; m_marsItem = nullptr; }
    if (m_jupiterItem && m_jupiterItem->scene() == this) { removeItem(m_jupiterItem); delete m_jupiterItem; m_jupiterItem = nullptr; }
    if (m_saturnItem && m_saturnItem->scene() == this) { removeItem(m_saturnItem); delete m_saturnItem; m_saturnItem = nullptr; }
    if (m_uranusItem && m_uranusItem->scene() == this) { removeItem(m_uranusItem); delete m_uranusItem; m_uranusItem = nullptr; }
    if (m_neptuneItem && m_neptuneItem->scene() == this) { removeItem(m_neptuneItem); delete m_neptuneItem; m_neptuneItem = nullptr; }

    // Text items
    if (m_healthText && m_healthText->scene() == this) { removeItem(m_healthText); delete m_healthText; m_healthText = nullptr; }
    if (m_judgmentText && m_judgmentText->scene() == this) { removeItem(m_judgmentText); delete m_judgmentText; m_judgmentText = nullptr; }
    if (m_scoreText && m_scoreText->scene() == this) { removeItem(m_scoreText); delete m_scoreText; m_scoreText = nullptr; }

    // Effect items
    if (m_explosionItem && m_explosionItem->scene() == this) { removeItem(m_explosionItem); delete m_explosionItem; m_explosionItem = nullptr; }
    if (m_collectEffectItem && m_collectEffectItem->scene() == this) { removeItem(m_collectEffectItem); delete m_collectEffectItem; m_collectEffectItem = nullptr; }
}


void GameScene::initializeGame()
{
    // Stop all timers and animations
    if (m_timer->isActive()) m_timer->stop();
    if (m_judgmentTimer->isActive()) m_judgmentTimer->stop();
    if (m_damageCooldownTimer->isActive()) m_damageCooldownTimer->stop();
    if (m_explosionMovie && m_explosionMovie->state() == QMovie::Running) m_explosionMovie->stop();
    if (m_collectEffectMovie && m_collectEffectMovie->state() == QMovie::Running) m_collectEffectMovie->stop();
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) m_explosionDurationTimer->stop();
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) m_collectEffectDurationTimer->stop();

    clearAllGameItems(); // Clear existing items

    // Reset game state variables
    m_gameOver = false;
    m_currentTrackIndex = 0;
    m_currentAngle = ANGLE_BOTTOM; // Start at the bottom of the first track
    m_orbitOffset = (BALL_RADIUS + ORBIT_PADDING); // Start on the outer orbit
    m_canTakeDamage = true;
    m_health = m_maxHealth;
    m_score = 0;
    m_speedLevel = 0;
    m_linearSpeed = BASE_LINEAR_SPEED;
    m_rotationDirection = 1; // Initial rotation direction (e.g., counter-clockwise)

    // Font loading
    int englishFontId = QFontDatabase::addApplicationFont(":/fonts/MyEnglishFont.ttf");
    if (englishFontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(englishFontId);
        if (!fontFamilies.isEmpty()) { m_englishFontFamily = fontFamilies.at(0); qDebug() << "Custom English font loaded:" << m_englishFontFamily; }
        else { qWarning() << "Failed to retrieve font family name for English font at :/fonts/MyEnglishFont.ttf. Using default:" << m_englishFontFamily; }
    } else { qWarning() << "Failed to load custom English font from :/fonts/MyEnglishFont.ttf. Using default:" << m_englishFontFamily; }

    int chineseFontId = QFontDatabase::addApplicationFont(":/fonts/MyChineseFont.ttf");
    if (chineseFontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(chineseFontId);
        if (!fontFamilies.isEmpty()) { m_chineseFontFamily = fontFamilies.at(0); qDebug() << "Custom Chinese font loaded:" << m_chineseFontFamily; }
        else { qWarning() << "Failed to retrieve font family name for Chinese font at :/fonts/MyChineseFont.ttf. Using default:" << m_chineseFontFamily; }
    } else { qWarning() << "Failed to load custom Chinese font from :/fonts/MyChineseFont.ttf. Using default:" << m_chineseFontFamily; }


    // Game Over Display
    if (m_gameOverDisplay) {
        m_gameOverDisplay->hideScreen();
    } else {
        m_gameOverDisplay = new GameOverDisplay(m_chineseFontFamily, m_englishFontFamily); // Ensure it's created
        addItem(m_gameOverDisplay); // Add to scene
        connect(m_gameOverDisplay, &GameOverDisplay::restartGameRequested, this, &GameScene::handleGameOverRestart);
        connect(m_gameOverDisplay, &GameOverDisplay::returnToMainMenuRequested, this, &GameScene::handleGameOverReturnToMain);
    }


    // Load level data
    if (!loadLevelData(":/levels/level1.json")) {
        qCritical() << "Failed to load level data. Game cannot start.";
        m_gameOver = true; // Set game over if level loading fails
        QPointF centerPos = sceneRect().center();
        if (!views().isEmpty()) centerPos = views().first()->mapToScene(views().first()->viewport()->geometry()).boundingRect().center();

        if(m_gameOverDisplay) { // Check if it was created
            m_gameOverDisplay->showScreen(-1, centerPos); // Show game over screen with error indication
        } else { // Fallback if GameOverDisplay is still null for some reason
            QGraphicsTextItem* errorText = new QGraphicsTextItem(
                QString("<p align='center'><span style=\"font-family: '%1'; font-size: 18pt; font-weight: bold; color: red;\">错误: 无法加载关卡数据!</span><br/>"
                        "<span style=\"font-family: '%1'; font-size: 18pt; font-weight: bold; color: red;\">请检查文件 </span><span style=\"font-family: '%2'; font-size: 18pt; font-weight: bold; color: red;\">:/levels/level1.json</span></p>")
                    .arg(m_chineseFontFamily).arg(m_englishFontFamily));
            addItem(errorText);
            errorText->setPos(centerPos - QPointF(errorText->boundingRect().width()/2, errorText->boundingRect().height()/2));
            errorText->setZValue(5.0); // Ensure it's on top
        }
        return; // Stop initialization
    }

    // --- 创建通关触发点 ---
    if (!m_levelData.segments.empty()) {
        // 使用 EndTriggerItem 类创建实例
        // 假设 DEFAULT_END_POINT_RADIUS 在 endtriggeritem.h 中定义
        m_endTriggerPoint = new EndTriggerItem(0, -16913, DEFAULT_END_POINT_RADIUS); // 示例坐标和半径
        m_endTriggerPoint->setZValue(0.7); // 确保它在轨道之上，但在飞船之下或同层，以便碰撞
        addItem(m_endTriggerPoint);
        m_endTriggerPoint->setVisible(true); // Make sure it's visible
        qDebug() << "EndTriggerItem created at scene pos (center): (0, -16913) with radius" << DEFAULT_END_POINT_RADIUS;
    } else {
        qDebug() << "Skipping end trigger point creation as no level data was loaded.";
    }


    // --- 创建和定位太阳及行星 ---
    if (!m_levelData.segments.empty()) {
        QPixmap originalSunPixmap(":/images/sun.png");
        if (originalSunPixmap.isNull()) { qWarning() << "Failed to load sun image: :/images/sun.png"; }
        else {
            const int sunTargetWidth = 450; const int sunTargetHeight = 450;
            QPixmap scaledSunPixmap = originalSunPixmap.scaled(sunTargetWidth, sunTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_sunItem = new QGraphicsPixmapItem(scaledSunPixmap);
            const TrackSegmentData& firstTrack = m_levelData.segments[0];
            qreal sunX = firstTrack.centerX - m_sunItem->boundingRect().width() / 2.0;
            qreal sunY = firstTrack.centerY - m_sunItem->boundingRect().height() / 2.0;
            m_sunItem->setPos(sunX, sunY);
            m_sunItem->setZValue(-0.5); addItem(m_sunItem); m_sunItem->setVisible(true);
        }
    }
    const qreal planetBaseX = 0; const qreal planetZValue = -0.5;
    QPixmap originalMercuryPixmap(":/images/mercury.png");
    if (!originalMercuryPixmap.isNull()) {
        const int mercuryTargetWidth = 120; const int mercuryTargetHeight = 120;
        QPixmap scaledMercuryPixmap = originalMercuryPixmap.scaled(mercuryTargetWidth, mercuryTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_mercuryItem = new QGraphicsPixmapItem(scaledMercuryPixmap);
        m_mercuryItem->setPos(planetBaseX - scaledMercuryPixmap.width() / 2.0, -1330 - scaledMercuryPixmap.height() / 2.0);
        m_mercuryItem->setZValue(planetZValue); addItem(m_mercuryItem); m_mercuryItem->setVisible(true);
    } else { qWarning() << "Failed to load mercury image: :/images/mercury.png"; }

    QPixmap originalVenusPixmap(":/images/venus.png");
    if (!originalVenusPixmap.isNull()) {
        const int venusTargetWidth = 150; const int venusTargetHeight = 150;
        QPixmap scaledVenusPixmap = originalVenusPixmap.scaled(venusTargetWidth, venusTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_venusItem = new QGraphicsPixmapItem(scaledVenusPixmap);
        m_venusItem->setPos(planetBaseX - scaledVenusPixmap.width() / 2.0, -2428 - scaledVenusPixmap.height() / 2.0);
        m_venusItem->setZValue(planetZValue); addItem(m_venusItem); m_venusItem->setVisible(true);
    } else { qWarning() << "Failed to load venus image: :/images/venus.png"; }

    QPixmap originalEarthPixmap(":/images/earth.png");
    if (!originalEarthPixmap.isNull()) {
        const int earthTargetWidth = 170; const int earthTargetHeight = 170;
        QPixmap scaledEarthPixmap = originalEarthPixmap.scaled(earthTargetWidth, earthTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_earthItem = new QGraphicsPixmapItem(scaledEarthPixmap);
        m_earthItem->setPos(planetBaseX - scaledEarthPixmap.width() / 2.0, -3610 - scaledEarthPixmap.height() / 2.0);
        m_earthItem->setZValue(planetZValue); addItem(m_earthItem); m_earthItem->setVisible(true);
    } else { qWarning() << "Failed to load earth image: :/images/earth.png"; }

    QPixmap originalMarsPixmap(":/images/mars.png");
    if (!originalMarsPixmap.isNull()) {
        const int marsTargetWidth = 130; const int marsTargetHeight = 130;
        QPixmap scaledMarsPixmap = originalMarsPixmap.scaled(marsTargetWidth, marsTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_marsItem = new QGraphicsPixmapItem(scaledMarsPixmap);
        m_marsItem->setPos(planetBaseX - scaledMarsPixmap.width() / 2.0, -5592 - scaledMarsPixmap.height() / 2.0);
        m_marsItem->setZValue(planetZValue); addItem(m_marsItem); m_marsItem->setVisible(true);
    } else { qWarning() << "Failed to load mars image: :/images/mars.png"; }

    QPixmap originalJupiterPixmap(":/images/jupiter.png");
    if (!originalJupiterPixmap.isNull()) {
        const int jupiterTargetWidth = 370; const int jupiterTargetHeight = 370;
        QPixmap scaledJupiterPixmap = originalJupiterPixmap.scaled(jupiterTargetWidth, jupiterTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_jupiterItem = new QGraphicsPixmapItem(scaledJupiterPixmap);
        m_jupiterItem->setPos(planetBaseX - scaledJupiterPixmap.width() / 2.0,-8148 - scaledJupiterPixmap.height() / 2.0);
        m_jupiterItem->setZValue(planetZValue); addItem(m_jupiterItem); m_jupiterItem->setVisible(true);
    } else { qWarning() << "Failed to load jupiter image: :/images/jupiter.png"; }

    QPixmap originalSaturnPixmap(":/images/saturn.png");
    if (!originalSaturnPixmap.isNull()) {
        const int saturnTargetWidth = 330; const int saturnTargetHeight = 240; // Adjusted for Saturn's shape
        QPixmap scaledSaturnPixmap = originalSaturnPixmap.scaled(saturnTargetWidth, saturnTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_saturnItem = new QGraphicsPixmapItem(scaledSaturnPixmap);
        m_saturnItem->setPos(planetBaseX - scaledSaturnPixmap.width() / 2.0, -10908- scaledSaturnPixmap.height() / 2.0);
        m_saturnItem->setZValue(planetZValue); addItem(m_saturnItem); m_saturnItem->setVisible(true);
    } else { qWarning() << "Failed to load saturn image: :/images/saturn.png"; }

    QPixmap originalUranusPixmap(":/images/uranus.png");
    if (!originalUranusPixmap.isNull()) {
        const int uranusTargetWidth = 275; const int uranusTargetHeight = 275;
        QPixmap scaledUranusPixmap = originalUranusPixmap.scaled(uranusTargetWidth, uranusTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_uranusItem = new QGraphicsPixmapItem(scaledUranusPixmap);
        m_uranusItem->setPos(planetBaseX - scaledUranusPixmap.width() / 2.0, -13718- scaledUranusPixmap.height() / 2.0);
        m_uranusItem->setZValue(planetZValue); addItem(m_uranusItem); m_uranusItem->setVisible(true);
    } else { qWarning() << "Failed to load uranus image: :/images/uranus.png"; }

    QPixmap originalNeptunePixmap(":/images/neptune.png");
    if (!originalNeptunePixmap.isNull()) {
        const int neptuneTargetWidth = 250; const int neptuneTargetHeight = 250;
        QPixmap scaledNeptunePixmap = originalNeptunePixmap.scaled(neptuneTargetWidth, neptuneTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_neptuneItem = new QGraphicsPixmapItem(scaledNeptunePixmap);
        m_neptuneItem->setPos(planetBaseX - scaledNeptunePixmap.width() / 2.0, -16688 - scaledNeptunePixmap.height() / 2.0); // Near end trigger
        m_neptuneItem->setZValue(planetZValue); addItem(m_neptuneItem); m_neptuneItem->setVisible(true);
    } else { qWarning() << "Failed to load neptune image: :/images/neptune.png"; }


    // Adjust sceneRect to encompass all tracks and major elements
    if (!m_levelData.segments.empty()) {
        const TrackSegmentData& firstSegment = m_levelData.segments.front();
        QRectF totalTracksRect(firstSegment.centerX - firstSegment.radius, firstSegment.centerY - firstSegment.radius, 2 * firstSegment.radius, 2 * firstSegment.radius);
        for (size_t i = 1; i < m_levelData.segments.size(); ++i) {
            const TrackSegmentData& segment = m_levelData.segments[i];
            QRectF segmentRect(segment.centerX - segment.radius, segment.centerY - segment.radius, 2 * segment.radius, 2 * segment.radius);
            totalTracksRect = totalTracksRect.united(segmentRect);
        }
        // Include planets in scene rect calculation if they exist
        qreal padding = 200.0; // Padding around the content
        if (m_neptuneItem) { // Furthest planet
            totalTracksRect = totalTracksRect.united(m_neptuneItem->sceneBoundingRect().adjusted(-padding, -padding, padding, padding));
        } else if (m_sunItem) { // Or sun if no planets
            totalTracksRect = totalTracksRect.united(m_sunItem->sceneBoundingRect().adjusted(-padding, -padding, padding, padding));
        }
        totalTracksRect.adjust(-padding, -padding, padding, padding); // General padding
        setSceneRect(totalTracksRect);
    } else {
        setSceneRect(-400, -300, 800, 600); // Default if no level data
    }


    // Create game items (ball, target dot, etc.)
    if (!m_levelData.segments.empty()) { // Only create ball if there are tracks
        QPixmap originalSpaceshipPixmap(":/images/spaceship.png");
        if (originalSpaceshipPixmap.isNull()) {
            qWarning() << "Failed to load spaceship image. Using fallback blue circle.";
            // Fallback: create a simple blue circle if image fails
            QPixmap fallbackPixmap(static_cast<int>(2*BALL_RADIUS), static_cast<int>(2*BALL_RADIUS));
            fallbackPixmap.fill(Qt::transparent); // Transparent background
            QPainter painter(&fallbackPixmap);
            painter.setBrush(Qt::blue);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(0, 0, static_cast<int>(2*BALL_RADIUS), static_cast<int>(2*BALL_RADIUS));
            painter.end();
            m_ball = new QGraphicsPixmapItem(fallbackPixmap);
        } else {
            qreal targetDiameter = 2.0 * BALL_RADIUS;
            QPixmap scaledSpaceship = originalSpaceshipPixmap.scaled(static_cast<int>(targetDiameter), static_cast<int>(targetDiameter), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_ball = new QGraphicsPixmapItem(scaledSpaceship);
        }
        m_ball->setZValue(1.0); // Ensure ball is above tracks
        addItem(m_ball);
        m_ball->setTransformOriginPoint(m_ball->boundingRect().center()); // For rotation
    }

    if (!m_levelData.segments.empty()) { // Only create target dot if there are tracks
        m_targetDot = new QGraphicsEllipseItem(-TARGET_DOT_RADIUS, -TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS);
        m_targetDot->setBrush(m_targetBrush);
        m_targetDot->setPen(m_targetPen);
        m_targetDot->setZValue(1.0); // Same Z as ball or slightly below
        addItem(m_targetDot);
    }

    // Effect items (invisible initially)
    m_explosionItem = new QGraphicsPixmapItem();
    m_explosionItem->setZValue(1.5); // Above ball
    m_explosionItem->setVisible(false);
    addItem(m_explosionItem);

    m_collectEffectItem = new QGraphicsPixmapItem();
    m_collectEffectItem->setZValue(1.4); // Above ball, below explosion
    m_collectEffectItem->setVisible(false);
    addItem(m_collectEffectItem);


    // UI Text Items
    m_healthText = new QGraphicsTextItem();
    m_healthText->setFont(QFont(m_englishFontFamily, 18)); // Font size might need adjustment
    m_healthText->setZValue(2.0); // On top of everything
    addItem(m_healthText);

    m_judgmentText = new QGraphicsTextItem();
    m_judgmentText->setFont(QFont(m_englishFontFamily, 22)); // Larger for judgment
    m_judgmentText->setZValue(2.0);
    m_judgmentText->setVisible(false); // Initially hidden
    addItem(m_judgmentText);

    m_scoreText = new QGraphicsTextItem();
    m_scoreText->setFont(QFont(m_englishFontFamily, 18));
    m_scoreText->setDefaultTextColor(Qt::white); // Score color
    m_scoreText->setZValue(2.0);
    addItem(m_scoreText);

    // Position collectibles and obstacles based on loaded level data
    positionAndShowCollectibles();
    positionAndShowObstacles();


    // Initial updates
    if(m_ball) updateBallPosition(); // Position ball on the first track
    if(m_targetDot) updateTargetDotPosition(); // Position target dot
    updateHealthDisplay();
    updateScoreDisplayAndSpeed();

    // Ensure view is focused and background is updated
    if (!views().isEmpty()) {
        updateInfiniteBackground(); // Initial background draw
        views().first()->setFocus(); // For keyboard input
    }

    // Start background music if valid and not already playing
    if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->source().isValid() && !m_gameOver) {
        if (m_backgroundMusicPlayer->mediaStatus() == QMediaPlayer::LoadedMedia &&
            m_backgroundMusicPlayer->playbackState() != QMediaPlayer::PlayingState) {
            // Timer not checked here, music should play if game is initialized and not over
            m_backgroundMusicPlayer->play();
            qDebug() << "Music play attempt in initializeGame (after setup).";
        }
    }

    m_timer->start(16); // Approx 60 FPS
    qDebug() << "Game initialized and timer started.";
}


void GameScene::updateHealthDisplay() {
    if (m_healthText) {
        QString healthColorName = "darkGreen"; // Default color for good health
        if (m_health <= 0) healthColorName = "darkRed"; // Critical health or game over
        else if (m_health <= m_maxHealth * 0.3) healthColorName = "red"; // Low health
        else if (m_health <= m_maxHealth * 0.6) healthColorName = "orange"; // Medium health

        // Using HTML for rich text formatting
        QString htmlText = QString("<span style=\"font-family: '%1'; font-size: 50pt; font-weight: bold; color: %3;\">生命: </span>"
                                   "<span style=\"font-family: '%2'; font-size: 60pt; font-weight: bold; color: %3;\">%4 / %5</span>")
                               .arg(m_chineseFontFamily).arg(m_chineseFontFamily) // Font for "生命" and numbers
                               .arg(healthColorName) // Color based on health
                               .arg(m_health).arg(m_maxHealth);
        m_healthText->setHtml(htmlText);
    }
}

void GameScene::updateScoreDisplayAndSpeed() {
    if (m_scoreText) {
        QString htmlText = QString("<span style=\"font-family: '%1'; font-size: 50pt; font-weight: bold; color: white;\">分数: </span>"
                                   "<span style=\"font-family: '%2'; font-size: 60pt; font-weight: bold; color: white;\">%3</span>")
                               .arg(m_chineseFontFamily).arg(m_chineseFontFamily) // Font for "分数" and numbers
                               .arg(m_score);
        m_scoreText->setHtml(htmlText);
    }

    // Speed up logic
    int targetLevel = m_score / SCORE_THRESHOLD_FOR_SPEEDUP;
    if (targetLevel > m_speedLevel) {
        m_speedLevel = targetLevel;
        m_linearSpeed = BASE_LINEAR_SPEED * qPow(SPEEDUP_FACTOR, m_speedLevel);
        qDebug() << "Speed increased! Level:" << m_speedLevel << "Speed:" << m_linearSpeed;
    }
}

void GameScene::keyPressEvent(QKeyEvent *event)
{
    if (m_gameOver) {
        // If game over, pass event to GameOverDisplay or default handling
        // GameOverDisplay should handle its own key presses (like Enter for restart)
        QGraphicsScene::keyPressEvent(event);
        return;
    }

    if (m_levelData.segments.empty() || !m_ball) {
        // No tracks or no ball, nothing to control
        QGraphicsScene::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_K && !event->isAutoRepeat()) {
        // Check if it's possible to switch to the next track
        if (static_cast<size_t>(m_currentTrackIndex + 1) >= m_levelData.segments.size()) {
            qDebug() << "[KeyPress K] Already on the last track or no next track. Cannot switch.";
            event->accept(); // Consume the event
            return;
        }

        const TrackSegmentData& currentSegment = m_levelData.segments[m_currentTrackIndex];
        qreal effectiveBallRadiusOnTrack = currentSegment.radius + m_orbitOffset;
        if (effectiveBallRadiusOnTrack < BALL_RADIUS) effectiveBallRadiusOnTrack = BALL_RADIUS; // Prevent division by zero or too small radius

        // Calculate angle difference from the target (top of the circle)
        qreal angleDiff = m_currentAngle - ANGLE_TOP;
        // Normalize angle difference to be within -PI to PI
        while (angleDiff <= -M_PI) angleDiff += 2.0 * M_PI;
        while (angleDiff > M_PI) angleDiff -= 2.0 * M_PI;

        qreal absAngleDiff = qAbs(angleDiff);

        // Calculate angular speed for tolerance calculation
        qreal angularSpeed = (effectiveBallRadiusOnTrack > 0.01) ? (m_linearSpeed / effectiveBallRadiusOnTrack) : 0;

        // Tolerances (convert from milliseconds to radians based on angular speed)
        qreal perfectAngleTolerance = angularSpeed * (PERFECT_MS / 1000.0);
        qreal goodAngleTolerance = angularSpeed * (GOOD_MS / 1000.0);

        QString judgmentTextStrKey;
        QString judgmentTextColorName;
        bool success = false;
        int scoreGainedThisHit = 0;

        if (absAngleDiff <= perfectAngleTolerance) {
            qDebug() << "[KeyPress K] JUDGMENT: PERFECT! Current health:" << m_health << "Can take damage:" << m_canTakeDamage;
            judgmentTextStrKey = "PERFECT!";
            judgmentTextColorName = "yellow"; // Or some QColor
            success = true;
            scoreGainedThisHit = SCORE_PERFECT;
        } else if (absAngleDiff <= goodAngleTolerance) {
            qDebug() << "[KeyPress K] JUDGMENT: GOOD! Current health:" << m_health << "Can take damage:" << m_canTakeDamage;
            judgmentTextStrKey = "GOOD!";
            judgmentTextColorName = "cyan"; // Or some QColor
            success = true;
            scoreGainedThisHit = SCORE_GOOD;
        } else { // Miss
            qDebug() << "[KeyPress K] JUDGMENT: MISS! Current health BEFORE deduction:" << m_health << "Can take damage:" << m_canTakeDamage;
            m_health--;
            qDebug() << "[KeyPress K] JUDGMENT: MISS! Current health AFTER deduction:" << m_health;
            updateHealthDisplay();
            triggerExplosionEffect(); // Show explosion on miss
            if (m_health <= 0) {
                endGame();
                event->accept();
                return;
            }
        }

        // Display judgment text
        if (!judgmentTextStrKey.isEmpty() && m_judgmentText) {
            QString htmlText = QString("<span style=\"font-family: '%1'; font-size: 50pt; font-weight: bold; color: %2;\">%3</span>")
            .arg(m_englishFontFamily).arg(judgmentTextColorName).arg(judgmentTextStrKey);
            m_judgmentText->setHtml(htmlText);
            m_judgmentText->setVisible(true);

            // Position judgment text (e.g., above target dot or center of view)
            QPointF textPos;
            if (m_targetDot && m_targetDot->isVisible()) {
                QPointF targetDotCenter = m_targetDot->sceneBoundingRect().center();
                QRectF judgmentRect = m_judgmentText->boundingRect();
                textPos = targetDotCenter - QPointF(judgmentRect.width() / 2.0, judgmentRect.height() + TARGET_DOT_RADIUS + 15);
            } else if (!views().isEmpty()) {
                QRectF viewRect = views().first()->mapToScene(views().first()->viewport()->geometry()).boundingRect();
                QRectF judgmentRect = m_judgmentText->boundingRect();
                textPos = viewRect.center() - QPointF(judgmentRect.width() / 2.0, judgmentRect.height() / 2.0 + 60); // Offset from center
            } else {
                // Fallback position if no view or target dot
                textPos = QPointF(-m_judgmentText->boundingRect().width() / 2.0, -60);
            }
            m_judgmentText->setPos(textPos);
            m_judgmentTimer->start(500); // Hide after 0.5 seconds
        }

        if (success) {
            qDebug() << "[KeyPress K] SUCCESS: About to switch track. Health:" << m_health << "Can take damage:" << m_canTakeDamage;
            m_score += scoreGainedThisHit;
            updateScoreDisplayAndSpeed();
            switchTrack(); // Switch to the next track
            qDebug() << "[KeyPress K] SUCCESS: Switched track. Health:" << m_health << "Can take damage:" << m_canTakeDamage;

            // ***** MODIFIED LOGIC: Post-switch invulnerability *****
            if (m_damageCooldownTimer) { // Ensure timer is valid
                m_canTakeDamage = false; // Player cannot take damage immediately after switch
                m_damageCooldownTimer->start(DAMAGE_COOLDOWN_MS); // Use existing cooldown duration
                qDebug() << "[KeyPress K] SUCCESS: Post-switch invulnerability activated for" << DAMAGE_COOLDOWN_MS << "ms. m_canTakeDamage:" << m_canTakeDamage;
            }
            // ***** MODIFIED LOGIC END *****
        }
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_J && !event->isAutoRepeat()) {
        // Switch orbit (inner/outer)
        m_orbitOffset = -m_orbitOffset; // Toggle sign
        qDebug() << "[KeyPress J] Orbit switched. New m_orbitOffset:" << m_orbitOffset;
        updateBallPosition(); // Update ball position based on new orbit
        event->accept();
        return;
    }

    QGraphicsScene::keyPressEvent(event); // Pass to base class if not handled
}

void GameScene::endGame() {
    if(m_gameOver) return; // Already ended
    m_gameOver = true;
    qDebug() << "Game Over. Final Score:" << m_score << "Reason: Health depleted or level completed without video trigger.";

    // Stop game timer and other relevant timers/animations
    if (m_timer->isActive()) m_timer->stop();
    if (m_judgmentTimer->isActive()) m_judgmentTimer->stop();
    if (m_damageCooldownTimer->isActive()) m_damageCooldownTimer->stop();

    if (m_explosionMovie && m_explosionMovie->state() == QMovie::Running) m_explosionMovie->stop();
    if (m_explosionItem) m_explosionItem->setVisible(false);
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) m_explosionDurationTimer->stop();

    if (m_collectEffectMovie && m_collectEffectMovie->state() == QMovie::Running) m_collectEffectMovie->stop();
    if (m_collectEffectItem) m_collectEffectItem->setVisible(false);
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) m_collectEffectDurationTimer->stop();

    // Optionally stop music, or let it play if GameOverDisplay has its own music/silence
    // if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->playbackState() == QMediaPlayer::PlayingState) {
    //     m_backgroundMusicPlayer->stop();
    // }

    // Hide game elements
    if (m_targetDot) m_targetDot->setVisible(false);
    if (m_ball) m_ball->setVisible(false);
    if (m_endTriggerPoint) m_endTriggerPoint->setVisible(false); // Hide end trigger too

    for (CollectibleItem* c : m_collectibles) if(c) c->setVisibleState(false);
    for (ObstacleItem* o : m_obstacles) if(o) o->setVisibleState(false);

    // Hide HUD elements
    if (m_healthText) m_healthText->setVisible(false);
    if (m_judgmentText) m_judgmentText->setVisible(false);
    if (m_scoreText) m_scoreText->setVisible(false);

    // Show Game Over screen
    if (m_gameOverDisplay) {
        QPointF centerPosOfView;
        if (!views().isEmpty()) {
            QGraphicsView* view = views().first();
            // Map the viewport's center to scene coordinates for positioning the display
            QRectF viewRect = view->mapToScene(view->viewport()->geometry()).boundingRect();
            centerPosOfView = viewRect.center();
        } else {
            centerPosOfView = sceneRect().center(); // Fallback if no view
        }
        m_gameOverDisplay->showScreen(m_score, centerPosOfView);
    } else {
        qWarning() << "m_gameOverDisplay is null in endGame! Cannot show game over screen.";
    }
}

bool GameScene::loadLevelData(const QString& filename)
{
    m_levelData.segments.clear(); // Clear previous data
    qDeleteAll(m_trackItems);     // Delete old QGraphicsPathItem objects
    m_trackItems.clear();         // Clear the list

    // Use TrackData class to load from JSON
    if (!m_levelData.loadLevelFromFile(filename)) {
        qCritical() << "GameScene: Failed to load level data using TrackData class from file:" << filename;
        return false;
    }

    if (m_levelData.segments.empty()) {
        qWarning() << "GameScene: Level data loaded successfully, but no track segments found in" << filename << ". Game might be unplayable.";
        // Don't return false, allow game to start with empty level if needed (e.g. for testing)
    }

    // Create visual track items and game objects (collectibles, obstacles)
    for (size_t i = 0; i < m_levelData.segments.size(); ++i) {
        const TrackSegmentData& segmentData = m_levelData.segments[i];
        addTrackItem(segmentData); // Create the visual track ellipse

        // Create collectibles for this segment
        for (const CollectibleData& cData : segmentData.collectibles) {
            CollectibleItem *item = new CollectibleItem(static_cast<int>(i), qDegreesToRadians(cData.angleDegrees));
            item->setOrbitOffset(cData.radialOffset); // Set its specific offset from the track's radius
            connect(item, &CollectibleItem::collectedSignal, this, &GameScene::handleCollectibleCollected);
            m_collectibles.append(item);
            // addItem(item); // Will be added and positioned in positionAndShowCollectibles
        }

        // Create obstacles for this segment
        for (const ObstacleData& oData : segmentData.obstacles) {
            ObstacleItem *item = new ObstacleItem(static_cast<int>(i), qDegreesToRadians(oData.angleDegrees));
            item->setOrbitOffset(oData.radialOffset); // Set its specific offset
            connect(item, &ObstacleItem::hitSignal, this, &GameScene::handleObstacleHit);
            m_obstacles.append(item);
            // addItem(item); // Will be added and positioned in positionAndShowObstacles
        }
    }
    qDebug() << "GameScene: Successfully processed" << m_levelData.segments.size() << "segments from TrackData.";
    return true;
}

void GameScene::addTrackItem(const TrackSegmentData& segmentData) {
    // Create a QGraphicsEllipseItem for the track
    QRectF bounds(segmentData.centerX - segmentData.radius,
                  segmentData.centerY - segmentData.radius,
                  2 * segmentData.radius,
                  2 * segmentData.radius);
    QGraphicsEllipseItem* trackItem = new QGraphicsEllipseItem(bounds);
    trackItem->setPen(m_trackPen);
    trackItem->setBrush(m_trackBrush); // Usually NoBrush for tracks
    trackItem->setZValue(0); // Tracks are at the base Z level
    addItem(trackItem);
    m_trackItems.append(trackItem);
}

void GameScene::positionAndShowCollectibles()
{
    for (CollectibleItem* collectible : m_collectibles) {
        if (!collectible) continue;

        if (collectible->scene() == nullptr) { // Add to scene if not already added
            addItem(collectible);
        }

        int trackIdx = collectible->getAssociatedTrackIndex();
        if (trackIdx >= 0 && static_cast<size_t>(trackIdx) < m_levelData.segments.size()) {
            const TrackSegmentData& segment = m_levelData.segments[trackIdx];
            collectible->updateVisualPosition(QPointF(segment.centerX, segment.centerY), segment.radius);
            collectible->setVisibleState(true); // Make sure it's visible
        } else {
            qWarning() << "Collectible has invalid track index:" << trackIdx << ". Hiding it.";
            collectible->setVisibleState(false);
        }
    }
}

void GameScene::positionAndShowObstacles()
{
    for (ObstacleItem* obstacle : m_obstacles) {
        if (!obstacle) continue;

        if (obstacle->scene() == nullptr) { // Add to scene if not already added
            addItem(obstacle);
        }

        int trackIdx = obstacle->getAssociatedTrackIndex();
        if (trackIdx >= 0 && static_cast<size_t>(trackIdx) < m_levelData.segments.size()) {
            const TrackSegmentData& segment = m_levelData.segments[trackIdx];
            obstacle->updateVisualPosition(QPointF(segment.centerX, segment.centerY), segment.radius);
            obstacle->setVisibleState(true); // Make sure it's visible
        } else {
            qWarning() << "Obstacle has invalid track index:" << trackIdx << ". Hiding it.";
            obstacle->setVisibleState(false);
        }
    }
}


void GameScene::updateInfiniteBackground() {
    if (m_backgroundTilePixmap.isNull() || m_backgroundTileSize.width() <= 0 || m_backgroundTileSize.height() <= 0 || views().isEmpty()) {
        // If no valid tile or no view, hide all tiles
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

    // Calculate the anchor point for the central tile based on view center
    qreal centralAnchorX = floor(viewCenter.x() / tileW) * tileW;
    qreal centralAnchorY = floor(viewCenter.y() / tileH) * tileH;

    int offset = (BG_GRID_SIZE - 1) / 2; // Offset to center the grid around the anchor
    int tileIndex = 0;

    for (int row = 0; row < BG_GRID_SIZE; ++row) {
        for (int col = 0; col < BG_GRID_SIZE; ++col) {
            if (tileIndex < m_backgroundTiles.size()) {
                QGraphicsPixmapItem *tile = m_backgroundTiles.at(tileIndex);
                qreal tileX = centralAnchorX + (col - offset) * tileW;
                qreal tileY = centralAnchorY + (row - offset) * tileH;
                tile->setPos(tileX, tileY);
                tile->setVisible(true); // Ensure tile is visible
            }
            tileIndex++;
        }
    }
}


void GameScene::updateGame()
{
    // ADDED: Debug log at the start of each game update tick
    qDebug() << "[UpdateGame TICK] Health:" << m_health
             << "Track:" << m_currentTrackIndex
             << "Angle:" << qRadiansToDegrees(m_currentAngle) // Log angle in degrees
             << "CanTakeDamage:" << m_canTakeDamage
             << "OrbitOffset:" << m_orbitOffset
             << "TimerActive:" << (m_timer ? m_timer->isActive() : false)
             << "GameOver:" << m_gameOver;


    // 首先检查是否与通关触发点碰撞
    if (!m_gameOver && m_ball && m_endTriggerPoint && m_endTriggerPoint->isVisible() && m_ball->isVisible() && m_ball->collidesWithItem(m_endTriggerPoint)) {
        qDebug() << "Spaceship collided with the end trigger point! Emitting endGameVideoRequested.";

        if(m_timer && m_timer->isActive()) { // Stop game logic timer
            m_timer->stop();
            qDebug() << "Game timer stopped due to end trigger.";
        }
        // Don't stop background music here, let the video player handle it or stop it after video.

        // Hide game elements, but keep score/health potentially for a "Level Cleared" screen before video
        if(m_ball) m_ball->setVisible(false);
        if (m_targetDot) m_targetDot->setVisible(false);
        // if (m_endTriggerPoint) m_endTriggerPoint->setVisible(false); // Optionally hide trigger

        // Hide HUD, or transition to a "Level Cleared" message
        if (m_healthText) m_healthText->setVisible(false);
        if (m_scoreText) m_scoreText->setVisible(false);
        if (m_judgmentText) m_judgmentText->setVisible(false);

        emit endGameVideoRequested(); // Signal to main window to play video
        // m_gameOver might be set by the main window after video or by another mechanism.
        // For now, we just stop the timer and emit the signal.
        return; // Important to return here to prevent further game logic for this tick
    }


    if (m_gameOver) {
        // If game is over (e.g., from health depletion or after video), do nothing further in update.
        return;
    }

    updateInfiniteBackground(); // Update background scrolling

    // Basic checks for game viability
    if (m_levelData.segments.empty() || !m_ball) {
        if (!m_gameOver) { // If not already game over, end it now
            qWarning() << "Game ending: No level data or no ball.";
            endGame();
        }
        return;
    }
    if (m_currentTrackIndex < 0 || static_cast<size_t>(m_currentTrackIndex) >= m_levelData.segments.size()) {
        qWarning() << "Invalid currentTrackIndex:" << m_currentTrackIndex << ". Ending game.";
        endGame();
        return;
    }

    // Game logic: Move ball, check collisions, update UI
    const TrackSegmentData& currentSegment = m_levelData.segments[m_currentTrackIndex];
    qreal effectiveBallRadiusOnTrack = currentSegment.radius + m_orbitOffset;
    // Ensure effective radius is not too small, especially if ball is on inner orbit very close to center
    if (effectiveBallRadiusOnTrack < BALL_RADIUS) effectiveBallRadiusOnTrack = BALL_RADIUS;

    qreal currentAngularVelocity = (effectiveBallRadiusOnTrack > 0.01) ? (m_linearSpeed / effectiveBallRadiusOnTrack) : 0; // Avoid division by zero
    qreal timeDelta = m_timer->interval() / 1000.0; // Time since last update in seconds
    qreal angleDelta = currentAngularVelocity * timeDelta;

    m_currentAngle += angleDelta * m_rotationDirection;

    // Normalize angle to be within [0, 2*PI)
    while (m_currentAngle >= 2.0 * M_PI) m_currentAngle -= 2.0 * M_PI;
    while (m_currentAngle < 0) m_currentAngle += 2.0 * M_PI;

    updateBallPosition(); // Update visual position of the ball

    // Collision checks
    checkBallCollectibleCollisions();
    checkBallObstacleCollisions();
    if (m_canTakeDamage) { // Only check for track collisions if not in cooldown
        checkTrackCollisions();
    }
    // Note: endGame() might be called within collision checks if health drops to 0.
    // If so, m_gameOver will be true, and the next tick will return early.


    // Update view to follow the ball and position HUD elements
    if (!views().isEmpty() && m_ball && m_ball->isVisible()) { // Check if ball is visible
        QGraphicsView* view = views().first();
        qreal yOffsetScene = 50; // How much the camera leads the ball vertically (in scene units)
        QPointF ballPixmapCenter = m_ball->sceneBoundingRect().center();
        QPointF targetCenterPoint = ballPixmapCenter - QPointF(0, yOffsetScene); // Camera target point
        view->centerOn(targetCenterPoint);

        // Position HUD elements relative to the view
        QRectF viewRectForHUD = view->mapToScene(view->viewport()->geometry()).boundingRect();

        if (m_healthText && m_healthText->isVisible()) { // Check visibility
            m_healthText->setPos(viewRectForHUD.topLeft() + QPointF(20, 20)); // Top-left corner
        }
        if (m_scoreText && m_scoreText->isVisible()) { // Check visibility
            QRectF scoreRect = m_scoreText->boundingRect();
            m_scoreText->setPos(viewRectForHUD.topRight() + QPointF(-scoreRect.width() - 20, 20)); // Top-right corner
        }
    }
}


void GameScene::updateBallPosition() {
    if (!m_ball || m_levelData.segments.empty() || m_currentTrackIndex < 0 || static_cast<size_t>(m_currentTrackIndex) >= m_levelData.segments.size()) return;

    const TrackSegmentData& currentSegmentData = m_levelData.segments[m_currentTrackIndex];
    qreal effectiveRadius = currentSegmentData.radius + m_orbitOffset;

    // Prevent ball from going inside the track center point if on inner orbit
    if (effectiveRadius < BALL_RADIUS && m_orbitOffset < 0) effectiveRadius = BALL_RADIUS;
    // This case should ideally not happen if orbitOffset is always +/- (BALL_RADIUS + ORBIT_PADDING)
    // but as a safeguard:
    else if (effectiveRadius < 0 && m_orbitOffset > 0) effectiveRadius = BALL_RADIUS;


    qreal x_center = currentSegmentData.centerX + effectiveRadius * qCos(m_currentAngle);
    qreal y_center = currentSegmentData.centerY + effectiveRadius * qSin(m_currentAngle);

    // Position the ball pixmap item; its origin is top-left by default
    if (m_ball->pixmap().isNull()) { // Fallback for simple QGraphicsEllipseItem if pixmap failed
        m_ball->setPos(x_center - BALL_RADIUS, y_center - BALL_RADIUS);
    } else {
        // Center the pixmap around (x_center, y_center)
        m_ball->setPos(x_center - m_ball->boundingRect().width() / 2.0,
                       y_center - m_ball->boundingRect().height() / 2.0);
    }

    // Rotate the ball to align with its direction of travel (tangent to the circle)
    // Angle of the tangent line is m_currentAngle + PI/2 (or -PI/2 depending on rotation direction)
    qreal tangentAngleRadians = m_currentAngle + m_rotationDirection * (M_PI / 2.0);
    qreal rotationAngleDegrees = qRadiansToDegrees(tangentAngleRadians);
    rotationAngleDegrees += 90.0; // Adjust if spaceship image points "up" by default
    m_ball->setRotation(rotationAngleDegrees);
}

void GameScene::updateTargetDotPosition() {
    if (!m_targetDot || m_levelData.segments.empty() || m_currentTrackIndex < 0 || static_cast<size_t>(m_currentTrackIndex) >= m_levelData.segments.size()) {
        if(m_targetDot) m_targetDot->setVisible(false); // Hide if invalid state
        return;
    }

    const TrackSegmentData& currentSegmentData = m_levelData.segments[m_currentTrackIndex];
    // Position the target dot at the "top" of the current track (where ANGLE_TOP is)
    QPointF judgmentPoint = QPointF(currentSegmentData.centerX, currentSegmentData.centerY) +
                            QPointF(currentSegmentData.radius * qCos(ANGLE_TOP),
                                    currentSegmentData.radius * qSin(ANGLE_TOP));
    m_targetDot->setPos(judgmentPoint); // setPos is relative to parent, for top-level items it's scene coords
    m_targetDot->setVisible(true);
}

void GameScene::hideJudgmentText() {
    if (m_judgmentText) m_judgmentText->setVisible(false);
}

void GameScene::enableDamageTaking() {
    m_canTakeDamage = true;
    qDebug() << "[DamageCooldown] Cooldown finished. m_canTakeDamage set to true.";
}

void GameScene::checkTrackCollisions() {
    // Only check if ball is on the outer orbit and can take damage
    if (m_orbitOffset <= 0 || !m_ball || m_levelData.segments.empty() || !m_canTakeDamage || m_currentTrackIndex < 0) return;

    QPointF ballVisualCenter = m_ball->sceneBoundingRect().center(); // Center of the ball's visual representation
    bool collisionOccurred = false;
    size_t collidedTrackIndex = static_cast<size_t>(-1); // Initialize with an invalid index

    // Check collision with ALL OTHER tracks
    for (size_t i = 0; i < m_levelData.segments.size(); ++i) {
        if (static_cast<int>(i) == m_currentTrackIndex) continue; // Skip current track

        const TrackSegmentData& otherSegment = m_levelData.segments[i];
        QPointF otherTrackCenter(otherSegment.centerX, otherSegment.centerY);
        qreal distanceToOtherTrackCenter = QLineF(ballVisualCenter, otherTrackCenter).length();

        // MODIFIED COLLISION DETECTION:
        // Original: if (distanceToOtherTrackCenter < otherSegment.radius + BALL_RADIUS)
        // The factor TRACK_COLLISION_EFFECTIVE_RADIUS_FACTOR (e.g., 0.8) makes the ball effectively "thinner"
        // for the purpose of this specific collision check, requiring a deeper overlap.
        if (distanceToOtherTrackCenter < otherSegment.radius + (BALL_RADIUS * TRACK_COLLISION_EFFECTIVE_RADIUS_FACTOR)) {
            collisionOccurred = true;
            collidedTrackIndex = i;
            break;
        }
    }

    if (collisionOccurred) {
        qDebug() << "[TrackCollision] Occurred! Health BEFORE deduction:" << m_health
                 << " Current track:" << m_currentTrackIndex
                 << " Collided with track:" << collidedTrackIndex
                 << " Can take damage:" << m_canTakeDamage
                 << " OrbitOffset:" << m_orbitOffset
                 << " EffectiveBallRadiusForCheck:" << (BALL_RADIUS * TRACK_COLLISION_EFFECTIVE_RADIUS_FACTOR);


        m_health--;
        updateHealthDisplay();
        triggerExplosionEffect(); // Show visual feedback

        // Force ball to inner orbit of current track as a penalty/evasive maneuver
        m_orbitOffset = -(BALL_RADIUS + ORBIT_PADDING); // Negative for inner orbit
        updateBallPosition(); // Immediately update position

        m_canTakeDamage = false; // Start cooldown
        m_damageCooldownTimer->start(DAMAGE_COOLDOWN_MS); // Use the constant (log showed 500ms, ensure this is intended)

        qDebug() << "[TrackCollision] Occurred! Health AFTER deduction:" << m_health
                 << " New OrbitOffset:" << m_orbitOffset
                 << " CanTakeDamage set to false. Cooldown started for" << DAMAGE_COOLDOWN_MS << "ms.";

        if (m_health <= 0) {
            endGame();
        }
    }
}


void GameScene::checkBallCollectibleCollisions() {
    if (!m_ball || !m_ball->isVisible()) return; // No ball or ball is hidden

    // Get all items colliding with the ball
    QList<QGraphicsItem*> collidingItemsList = m_ball->collidingItems();

    for (QGraphicsItem* item : collidingItemsList) {
        CollectibleItem* collectible = dynamic_cast<CollectibleItem*>(item);
        if (collectible && collectible->isVisible() && !collectible->isCollected()) {
            collectible->collect(); // This will emit collectedSignal
        }
    }
}

void GameScene::checkBallObstacleCollisions() {
    if (!m_ball || !m_ball->isVisible()) return; // No ball or ball is hidden

    QList<QGraphicsItem*> collidingItemsList = m_ball->collidingItems();

    for (QGraphicsItem* item : collidingItemsList) {
        ObstacleItem* obstacle = dynamic_cast<ObstacleItem*>(item);
        // Check if it's an obstacle, it's visible, and hasn't been "hit" yet in a way that makes it non-interactive
        if (obstacle && obstacle->isVisible() && !obstacle->isHit() && m_canTakeDamage) { // Also check m_canTakeDamage here before processing hit
            obstacle->processHit(); // This will emit hitSignal
                // The actual damage is handled in handleObstacleHit slot
        } else if (obstacle && obstacle->isVisible() && !obstacle->isHit() && !m_canTakeDamage) {
            qDebug() << "[ObstacleCollisionCheck] Ball collided with obstacle, but m_canTakeDamage is false. No hit processed this tick.";
        }
    }
}


void GameScene::handleCollectibleCollected(CollectibleItem* item) {
    if (!item) return;
    qDebug() << "[CollectibleCollected] Collectible on track" << item->getAssociatedTrackIndex() << "collected. Score +" << item->getScoreValue();
    triggerCollectEffect(); // Show visual effect for collection
    m_score += item->getScoreValue();
    updateScoreDisplayAndSpeed(); // Update UI and potentially speed
    // The item itself will handle its visibility/removal from scene logic after collection
}

void GameScene::handleObstacleHit(ObstacleItem* item) {
    if (!item) return; // Should not happen if signal is emitted correctly

    if (!m_canTakeDamage) {
        qDebug() << "[ObstacleHit] Signal received for obstacle on track" << item->getAssociatedTrackIndex()
        << "but m_canTakeDamage is false. No damage taken. Health:" << m_health;
        // Obstacle might still play an animation or sound even if player doesn't take damage.
        // For now, if player is invincible, obstacle hit does nothing to player.
        return;
    }

    qDebug() << "[ObstacleHit] Obstacle Hit on track " << item->getAssociatedTrackIndex() << "! Health BEFORE deduction:" << m_health << "Can take damage:" << m_canTakeDamage;
    m_health--;
    updateHealthDisplay();
    triggerExplosionEffect(); // Player hit effect

    m_canTakeDamage = false; // Start invincibility cooldown
    m_damageCooldownTimer->start(DAMAGE_COOLDOWN_MS); // Use the constant

    qDebug() << "[ObstacleHit] Obstacle Hit! Health AFTER deduction:" << m_health << "CanTakeDamage set to false. Cooldown for" << DAMAGE_COOLDOWN_MS << "ms.";

    if (m_health <= 0) {
        endGame();
    }
    // The obstacle item itself might change its state (e.g., become inactive or play an animation) via its processHit() method.
}


void GameScene::switchTrack() {
    qDebug() << "[SwitchTrack] Entering switchTrack. Current track BEFORE switch:" << m_currentTrackIndex
             << " Health:" << m_health << " Can take damage:" << m_canTakeDamage
             << " OrbitOffset BEFORE:" << m_orbitOffset << " RotationDir BEFORE:" << m_rotationDirection
             << " Angle BEFORE:" << qRadiansToDegrees(m_currentAngle);

    if (static_cast<size_t>(m_currentTrackIndex + 1) >= m_levelData.segments.size()) {
        qDebug() << "[SwitchTrack] Attempted to switch past the last track (" << m_levelData.segments.size() -1 << "). No switch performed.";
        return;
    }

    m_currentTrackIndex++;
    m_rotationDirection = -m_rotationDirection; // Reverse rotation on new track
    m_currentAngle = ANGLE_BOTTOM; // Reset angle to a consistent starting point

    // ***** 修改核心逻辑：确保飞船在新轨道的内侧 *****
    // 原来的逻辑是确保在外侧:
    // m_orbitOffset = qAbs(m_orbitOffset);
    // if (m_orbitOffset < (BALL_RADIUS + ORBIT_PADDING) || m_orbitOffset == 0) {
    //      m_orbitOffset = BALL_RADIUS + ORBIT_PADDING;
    // }
    // 新逻辑：直接设置为内侧偏移
    m_orbitOffset = -(BALL_RADIUS + ORBIT_PADDING);
    // ***** 修改核心逻辑结束 *****

    updateTargetDotPosition(); // Update target dot for the new track
    updateBallPosition();    // Update ball position immediately for the new track and angle

    qDebug() << "[SwitchTrack] Switched to track:" << m_currentTrackIndex
             << " Health:" << m_health << " Can take damage:" << m_canTakeDamage
             << " New OrbitOffset:" << m_orbitOffset << " New RotationDir:" << m_rotationDirection
             << " New Angle:" << qRadiansToDegrees(m_currentAngle);
}


void GameScene::triggerExplosionEffect() {
    if (!m_explosionMovie || !m_explosionItem || !m_ball || !m_explosionDurationTimer) {
        qWarning() << "triggerExplosionEffect: One or more essential members are null (Movie, Item, Ball, or DurationTimer).";
        return;
    }
    if (!m_explosionMovie->isValid()) {
        qWarning() << "Explosion GIF is not valid, cannot play.";
        return;
    }

    if (m_explosionDurationTimer->isActive()) m_explosionDurationTimer->stop(); // Stop previous timer if any
    if (m_explosionMovie->state() == QMovie::Running) m_explosionMovie->stop(); // Stop previous animation

    m_explosionMovie->jumpToFrame(0); // Rewind to start

    QPointF effectCenter = m_ball->sceneBoundingRect().center(); // Position at ball's center
    QPixmap originalFrame = m_explosionMovie->currentPixmap();

    if (originalFrame.isNull()) {
        qWarning() << "Explosion GIF current frame is null for triggerExplosionEffect.";
        return;
    }

    qreal effectDisplayDiameter = 5.0 * BALL_RADIUS; // Make explosion larger than ball
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_explosionItem->setPixmap(scaledFrame);
    m_explosionItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0)); // Center the pixmap
    m_explosionItem->setVisible(true);
    m_explosionMovie->start();
    m_explosionDurationTimer->start(600); // Duration for the effect to be visible (adjust as needed)
    qDebug() << "[Effect] Explosion triggered at" << effectCenter;
}

void GameScene::updateExplosionFrame(int frameNumber) {
    Q_UNUSED(frameNumber);
    if (!m_explosionMovie || !m_explosionItem || m_explosionMovie->state() != QMovie::Running) return;
    if (!m_ball) return; // Ball might be null if game ends abruptly

    QPixmap originalFrame = m_explosionMovie->currentPixmap();
    if (originalFrame.isNull()) return;

    qreal effectDisplayDiameter = 5.0 * BALL_RADIUS;
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_explosionItem->setPixmap(scaledFrame);
    // Update position if ball is still visible and moving (optional, effect could be static)
    if (m_ball->isVisible()) {
        QPointF effectCenter = m_ball->sceneBoundingRect().center();
        m_explosionItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));
    }
}

void GameScene::hideExplosionEffect() {
    if (m_explosionItem) m_explosionItem->setVisible(false);
    // Timer is single-shot, no need to stop it here if it timed out.
    // If movie finished, it stops itself.
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) { // If movie finished before timer
        m_explosionDurationTimer->stop();
    }
    qDebug() << "[Effect] Explosion hidden (movie finished or timer forced).";
}

void GameScene::forceHideExplosion() {
    // Called by m_explosionDurationTimer timeout
    if (m_explosionMovie && m_explosionMovie->state() == QMovie::Running) {
        m_explosionMovie->stop();
    }
    if (m_explosionItem) {
        m_explosionItem->setVisible(false);
    }
    qDebug() << "[Effect] Explosion force hidden by duration timer.";
}


void GameScene::triggerCollectEffect() {
    if (!m_collectEffectMovie || !m_collectEffectItem || !m_collectEffectDurationTimer || !m_ball) {
        qWarning() << "triggerCollectEffect: One or more essential members are null.";
        return;
    }
    if (!m_collectEffectMovie->isValid()) {
        qWarning() << "Collect effect GIF is not valid, cannot play.";
        return;
    }

    if (m_collectEffectDurationTimer->isActive()) m_collectEffectDurationTimer->stop();
    if (m_collectEffectMovie->state() == QMovie::Running) m_collectEffectMovie->stop();

    m_collectEffectMovie->jumpToFrame(0);
    QPointF effectCenter = m_ball->sceneBoundingRect().center();
    QPixmap originalFrame = m_collectEffectMovie->currentPixmap();
    if (originalFrame.isNull()) {
        qWarning() << "Collect effect GIF current frame is null for triggerCollectEffect.";
        return;
    }

    qreal effectDisplayDiameter = DEFAULT_COLLECTIBLE_TARGET_SIZE * DEFAULT_COLLECTIBLE_EFFECT_SIZE_MULTIPLIER;
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_collectEffectItem->setPixmap(scaledFrame);
    m_collectEffectItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));
    m_collectEffectItem->setVisible(true);
    m_collectEffectMovie->start();
    m_collectEffectDurationTimer->start(600); // Adjust duration
    qDebug() << "[Effect] Collect effect triggered at" << effectCenter;
}


void GameScene::updateCollectEffectFrame(int frameNumber) {
    Q_UNUSED(frameNumber);
    if (!m_collectEffectMovie || !m_collectEffectItem || m_collectEffectMovie->state() != QMovie::Running) return;
    if (!m_ball) return;

    QPixmap originalFrame = m_collectEffectMovie->currentPixmap();
    if (originalFrame.isNull()) return;

    qreal effectDisplayDiameter = DEFAULT_COLLECTIBLE_TARGET_SIZE * DEFAULT_COLLECTIBLE_EFFECT_SIZE_MULTIPLIER;
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_collectEffectItem->setPixmap(scaledFrame);
    if (m_ball->isVisible()) { // Update position to follow ball
        QPointF effectCenter = m_ball->sceneBoundingRect().center();
        m_collectEffectItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));
    }
}

void GameScene::hideCollectEffect() {
    if (m_collectEffectItem) m_collectEffectItem->setVisible(false);
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) {
        m_collectEffectDurationTimer->stop();
    }
    qDebug() << "[Effect] Collect effect hidden.";
}

void GameScene::forceHideCollectEffect() {
    if (m_collectEffectMovie && m_collectEffectMovie->state() == QMovie::Running) {
        m_collectEffectMovie->stop();
    }
    if (m_collectEffectItem) {
        m_collectEffectItem->setVisible(false);
    }
    qDebug() << "[Effect] Collect effect force hidden by duration timer.";
}


void GameScene::handleGameOverRestart()
{
    qDebug() << "GameScene::handleGameOverRestart() CALLED. Initializing game.";
    // No need to hide GameOverDisplay here, initializeGame will do it if m_gameOverDisplay exists.
    initializeGame(); // This will reset everything and start a new game
}

void GameScene::handleGameOverReturnToMain()
{
    qDebug() << "GameScene::handleGameOverReturnToMain() CALLED. Emitting returnToStartScreenRequested signal...";
    // GameOverDisplay should be hidden by the main window or when the start screen is shown.
    emit returnToStartScreenRequested(); // Signal to the main application window
}


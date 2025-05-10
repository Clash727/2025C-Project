#include "Gamescene.h"
#include "trackdata.h"
#include "collectibleitem.h"
#include "obstacleitem.h"
#include <QGraphicsView>
#include <QKeyEvent>
#include <QFont>
#include <QFontDatabase> // 用于加载自定义字体
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDebug>
#include <QtMath>
#include <QGraphicsPixmapItem>
#include <QMovie>
#include <QGraphicsEllipseItem>

// --- 游戏常量 ---
const qreal ANGLE_TOP = 3 * M_PI / 2.0;
const qreal ANGLE_BOTTOM = M_PI / 2.0;
// 确保 DEFAULT_COLLECTIBLE_TARGET_SIZE 有定义，例如在 collectibleitem.cpp 中
// const qreal DEFAULT_COLLECTIBLE_TARGET_SIZE = 15.0;


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
    m_health(m_maxHealth), // m_maxHealth 在 gamescene.h 中定义
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
    m_gameOverText(nullptr),
    m_scoreText(nullptr),
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
    m_englishFontFamily("Arial"),      // 默认英文字体
    m_chineseFontFamily("SimSun")      // 默认中文字体 (例如宋体)
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

    // --- 动画 QMovie 和相关 Timer 初始化 ---
    m_explosionMovie = new QMovie(":/animations/explosion.gif", QByteArray(), this);
    if (!m_explosionMovie->isValid()) {
        qWarning() << "Failed to load explosion GIF: :/animations/explosion.gif";
    }
    connect(m_explosionMovie, &QMovie::frameChanged, this, &GameScene::updateExplosionFrame);
    connect(m_explosionMovie, &QMovie::finished, this, &GameScene::hideExplosionEffect);
    m_explosionDurationTimer = new QTimer(this);
    m_explosionDurationTimer->setSingleShot(true);
    connect(m_explosionDurationTimer, &QTimer::timeout, this, &GameScene::forceHideExplosion);

    m_collectEffectMovie = new QMovie(":/animations/collect_effect.gif", QByteArray(), this);
    if (!m_collectEffectMovie->isValid()) {
        qWarning() << "Failed to load collect effect GIF: :/animations/collect_effect.gif";
    }
    connect(m_collectEffectMovie, &QMovie::frameChanged, this, &GameScene::updateCollectEffectFrame);
    connect(m_collectEffectMovie, &QMovie::finished, this, &GameScene::hideCollectEffect);
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

    qDeleteAll(m_trackItems);
    m_trackItems.clear();

    if (m_ball && m_ball->scene() == this) { removeItem(m_ball); delete m_ball; m_ball = nullptr; }
    if (m_targetDot && m_targetDot->scene() == this) { removeItem(m_targetDot); delete m_targetDot; m_targetDot = nullptr; }

    if (m_sunItem && m_sunItem->scene() == this) { removeItem(m_sunItem); delete m_sunItem; m_sunItem = nullptr; }
    if (m_mercuryItem && m_mercuryItem->scene() == this) { removeItem(m_mercuryItem); delete m_mercuryItem; m_mercuryItem = nullptr; }
    if (m_venusItem && m_venusItem->scene() == this) { removeItem(m_venusItem); delete m_venusItem; m_venusItem = nullptr; }
    if (m_earthItem && m_earthItem->scene() == this) { removeItem(m_earthItem); delete m_earthItem; m_earthItem = nullptr; }
    if (m_marsItem && m_marsItem->scene() == this) { removeItem(m_marsItem); delete m_marsItem; m_marsItem = nullptr; }
    if (m_jupiterItem && m_jupiterItem->scene() == this) { removeItem(m_jupiterItem); delete m_jupiterItem; m_jupiterItem = nullptr; }
    if (m_saturnItem && m_saturnItem->scene() == this) { removeItem(m_saturnItem); delete m_saturnItem; m_saturnItem = nullptr; }
    if (m_uranusItem && m_uranusItem->scene() == this) { removeItem(m_uranusItem); delete m_uranusItem; m_uranusItem = nullptr; }
    if (m_neptuneItem && m_neptuneItem->scene() == this) { removeItem(m_neptuneItem); delete m_neptuneItem; m_neptuneItem = nullptr; }


    if (m_healthText && m_healthText->scene() == this) { removeItem(m_healthText); delete m_healthText; m_healthText = nullptr; }
    if (m_judgmentText && m_judgmentText->scene() == this) { removeItem(m_judgmentText); delete m_judgmentText; m_judgmentText = nullptr; }
    if (m_gameOverText && m_gameOverText->scene() == this) { removeItem(m_gameOverText); delete m_gameOverText; m_gameOverText = nullptr; }
    if (m_scoreText && m_scoreText->scene() == this) { removeItem(m_scoreText); delete m_scoreText; m_scoreText = nullptr; }

    if (m_explosionItem && m_explosionItem->scene() == this) { removeItem(m_explosionItem); delete m_explosionItem; m_explosionItem = nullptr; }
    if (m_collectEffectItem && m_collectEffectItem->scene() == this) { removeItem(m_collectEffectItem); delete m_collectEffectItem; m_collectEffectItem = nullptr; }
}


void GameScene::initializeGame()
{
    if (m_timer->isActive()) m_timer->stop();
    if (m_judgmentTimer->isActive()) m_judgmentTimer->stop();
    if (m_damageCooldownTimer->isActive()) m_damageCooldownTimer->stop();
    if (m_explosionMovie && m_explosionMovie->state() == QMovie::Running) m_explosionMovie->stop();
    if (m_collectEffectMovie && m_collectEffectMovie->state() == QMovie::Running) m_collectEffectMovie->stop();
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) m_explosionDurationTimer->stop();
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) m_collectEffectDurationTimer->stop();

    clearAllGameItems();

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

    // --- 加载自定义字体 ---
    int englishFontId = QFontDatabase::addApplicationFont(":/fonts/MyEnglishFont.ttf");
    if (englishFontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(englishFontId);
        if (!fontFamilies.isEmpty()) {
            m_englishFontFamily = fontFamilies.at(0);
            qDebug() << "Custom English font loaded:" << m_englishFontFamily;
        } else {
            qWarning() << "Failed to retrieve font family name for English font at :/fonts/MyEnglishFont.ttf. Using default:" << m_englishFontFamily;
        }
    } else {
        qWarning() << "Failed to load custom English font from :/fonts/MyEnglishFont.ttf. Using default:" << m_englishFontFamily;
    }

    int chineseFontId = QFontDatabase::addApplicationFont(":/fonts/MyChineseFont.ttf");
    if (chineseFontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(chineseFontId);
        if (!fontFamilies.isEmpty()) {
            m_chineseFontFamily = fontFamilies.at(0);
            qDebug() << "Custom Chinese font loaded:" << m_chineseFontFamily;
        } else {
            qWarning() << "Failed to retrieve font family name for Chinese font at :/fonts/MyChineseFont.ttf. Using default:" << m_chineseFontFamily;
        }
    } else {
        qWarning() << "Failed to load custom Chinese font from :/fonts/MyChineseFont.ttf. Using default:" << m_chineseFontFamily;
    }
    // --- 自定义字体加载结束 ---


    if (!loadLevelData(":/levels/level1.json")) {
        qCritical() << "Failed to load level data. Game cannot start.";
        m_gameOver = true;
        if (!m_gameOverText) {
            m_gameOverText = new QGraphicsTextItem();
            addItem(m_gameOverText);
        }
        QString errorHtml = QString("<p align='center'><span style=\"font-family: '%1'; font-size: 18pt; font-weight: bold;\">错误: 无法加载关卡数据!</span><br/>" // 调整字号和加粗
                                    "<span style=\"font-family: '%1'; font-size: 18pt; font-weight: bold;\">请检查文件 </span><span style=\"font-family: '%2'; font-size: 18pt; font-weight: bold;\">:/levels/level1.json</span></p>")
                                .arg(m_chineseFontFamily).arg(m_englishFontFamily);
        m_gameOverText->setHtml(errorHtml);
        // m_gameOverText->setFont(QFont(m_englishFontFamily, 18, QFont::Bold)); // HTML中已包含样式
        m_gameOverText->setDefaultTextColor(Qt::red);
        m_gameOverText->setZValue(3);
        m_gameOverText->setVisible(true);
        if (!views().isEmpty()) {
            QRectF viewRect = views().first()->mapToScene(views().first()->viewport()->geometry()).boundingRect();
            m_gameOverText->setPos(viewRect.center() - QPointF(m_gameOverText->boundingRect().width() / 2.0, m_gameOverText->boundingRect().height() / 2.0));
        }
        return;
    }

    // --- 创建和定位太阳及行星 ---
    // (代码与之前版本相同，此处为简洁省略，请确保这部分代码存在)
    if (!m_levelData.segments.empty()) {
        QPixmap originalSunPixmap(":/images/sun.png");
        if (originalSunPixmap.isNull()) {
            qWarning() << "Failed to load sun image: :/images/sun.png";
        } else {
            const int sunTargetWidth = 450;
            const int sunTargetHeight = 450;
            QPixmap scaledSunPixmap = originalSunPixmap.scaled(sunTargetWidth, sunTargetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_sunItem = new QGraphicsPixmapItem(scaledSunPixmap);
            const TrackSegmentData& firstTrack = m_levelData.segments[0];
            qreal sunX = firstTrack.centerX - m_sunItem->boundingRect().width() / 2.0;
            qreal sunY = firstTrack.centerY - m_sunItem->boundingRect().height() / 2.0;
            m_sunItem->setPos(sunX, sunY);
            m_sunItem->setZValue(-0.5);
            addItem(m_sunItem);
            m_sunItem->setVisible(true);
        }
    }
    const qreal planetBaseX = 0;
    const qreal planetZValue = -0.5;
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
        const int saturnTargetWidth = 330; const int saturnTargetHeight = 240;
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
        m_neptuneItem->setPos(planetBaseX - scaledNeptunePixmap.width() / 2.0, -16688 - scaledNeptunePixmap.height() / 2.0);
        m_neptuneItem->setZValue(planetZValue); addItem(m_neptuneItem); m_neptuneItem->setVisible(true);
    } else { qWarning() << "Failed to load neptune image: :/images/neptune.png"; }


    if (!m_levelData.segments.empty()) {
        const TrackSegmentData& firstSegment = m_levelData.segments.front();
        QRectF totalTracksRect(firstSegment.centerX - firstSegment.radius,
                               firstSegment.centerY - firstSegment.radius,
                               2 * firstSegment.radius,
                               2 * firstSegment.radius);
        for (size_t i = 1; i < m_levelData.segments.size(); ++i) {
            const TrackSegmentData& segment = m_levelData.segments[i];
            QRectF segmentRect(segment.centerX - segment.radius,
                               segment.centerY - segment.radius,
                               2 * segment.radius,
                               2 * segment.radius);
            totalTracksRect = totalTracksRect.united(segmentRect);
        }
        qreal padding = 200.0;
        if (m_neptuneItem) {
            totalTracksRect = totalTracksRect.united(m_neptuneItem->sceneBoundingRect().adjusted(-padding, -padding, padding, padding));
        } else if (m_sunItem) {
            totalTracksRect = totalTracksRect.united(m_sunItem->sceneBoundingRect().adjusted(-padding, -padding, padding, padding));
        }
        totalTracksRect.adjust(-padding, -padding, padding, padding);
        setSceneRect(totalTracksRect);
    } else {
        setSceneRect(-400, -300, 800, 600);
    }

    m_explosionItem = new QGraphicsPixmapItem();
    m_explosionItem->setZValue(1.5);
    m_explosionItem->setVisible(false);
    addItem(m_explosionItem);

    m_collectEffectItem = new QGraphicsPixmapItem();
    m_collectEffectItem->setZValue(1.4);
    m_collectEffectItem->setVisible(false);
    addItem(m_collectEffectItem);


    if (!m_levelData.segments.empty()) {
        QPixmap originalSpaceshipPixmap(":/images/spaceship.png");
        if (originalSpaceshipPixmap.isNull()) {
            qWarning() << "Failed to load spaceship image. Using fallback.";
            QPixmap fallbackPixmap(static_cast<int>(2 * BALL_RADIUS), static_cast<int>(2 * BALL_RADIUS));
            fallbackPixmap.fill(Qt::blue);
            m_ball = new QGraphicsPixmapItem(fallbackPixmap);
        } else {
            qreal targetDiameter = 2.0 * BALL_RADIUS;
            QPixmap scaledSpaceshipPixmap = originalSpaceshipPixmap.scaled(static_cast<int>(targetDiameter), static_cast<int>(targetDiameter), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_ball = new QGraphicsPixmapItem(scaledSpaceshipPixmap);
        }
        m_ball->setZValue(1.0);
        addItem(m_ball);
        m_ball->setTransformOriginPoint(m_ball->boundingRect().center());
    }

    if (!m_levelData.segments.empty()) {
        m_targetDot = new QGraphicsEllipseItem(-TARGET_DOT_RADIUS, -TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS);
        m_targetDot->setBrush(m_targetBrush);
        m_targetDot->setPen(m_targetPen);
        m_targetDot->setZValue(1.0);
        addItem(m_targetDot);
    }

    // --- 创建文本项 ---
    // setFont 在这里可以设置一个基础样式，但最终显示由 setHtml 控制
    m_healthText = new QGraphicsTextItem();
    m_healthText->setFont(QFont(m_englishFontFamily, 18)); // 基础字号，粗细等由HTML控制
    m_healthText->setZValue(2.0);
    addItem(m_healthText);

    m_judgmentText = new QGraphicsTextItem();
    m_judgmentText->setFont(QFont(m_englishFontFamily, 22)); // 基础字号
    m_judgmentText->setZValue(2.0);
    m_judgmentText->setVisible(false);
    addItem(m_judgmentText);

    m_scoreText = new QGraphicsTextItem();
    m_scoreText->setFont(QFont(m_englishFontFamily, 18)); // 基础字号
    m_scoreText->setDefaultTextColor(Qt::white);
    m_scoreText->setZValue(2.0);
    addItem(m_scoreText);
    // --- 文本项创建结束 ---


    positionAndShowCollectibles();
    positionAndShowObstacles();

    if(m_ball) updateBallPosition();
    if(m_targetDot) updateTargetDotPosition();
    updateHealthDisplay();
    updateScoreDisplayAndSpeed();

    if (!views().isEmpty()) {
        updateInfiniteBackground();
    }

    if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->source().isValid() && m_backgroundMusicPlayer->mediaStatus() == QMediaPlayer::NoMedia) {
    } else if (m_backgroundMusicPlayer && m_backgroundMusicPlayer->mediaStatus() == QMediaPlayer::LoadedMedia && m_backgroundMusicPlayer->playbackState() != QMediaPlayer::PlayingState) {
        m_backgroundMusicPlayer->play();
    }

    m_timer->start(16);
}

void GameScene::updateHealthDisplay() {
    if (m_healthText) {
        QString healthColorName = "darkGreen";
        if (m_health <= 0) healthColorName = "darkRed";
        else if (m_health <= m_maxHealth * 0.3) healthColorName = "red";
        else if (m_health <= m_maxHealth * 0.6) healthColorName = "orange";

        // 修改点：调整字号和加粗
        QString htmlText = QString("<span style=\"font-family: '%1'; font-size: 50pt; font-weight: bold; color: %3;\">生命: </span>" // 例如，生命标签20pt粗体
                                   "<span style=\"font-family: '%2'; font-size: 60pt; font-weight: bold; color: %3;\">%4 / %5</span>") // 例如，生命数字22pt粗体
                               .arg(m_chineseFontFamily)
                               .arg(m_chineseFontFamily)
                               .arg(healthColorName)
                               .arg(m_health)
                               .arg(m_maxHealth);
        m_healthText->setHtml(htmlText);
    }
}

void GameScene::updateScoreDisplayAndSpeed() {
    if (m_scoreText) {
        // 修改点：调整字号和加粗
        QString htmlText = QString("<span style=\"font-family: '%1'; font-size: 50pt; font-weight: bold; color: white;\">分数: </span>" // 例如，分数标签20pt粗体
                                   "<span style=\"font-family: '%2'; font-size: 60pt; font-weight: bold; color: white;\">%3</span>") // 例如，分数数字22pt粗体
                               .arg(m_chineseFontFamily)
                               .arg(m_chineseFontFamily)
                               .arg(m_score);
        m_scoreText->setHtml(htmlText);
    }

    int targetLevel = m_score / SCORE_THRESHOLD_FOR_SPEEDUP;
    if (targetLevel > m_speedLevel) {
        m_speedLevel = targetLevel;
        m_linearSpeed = BASE_LINEAR_SPEED * qPow(SPEEDUP_FACTOR, m_speedLevel);
    }
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

    if (m_levelData.segments.empty() || !m_ball) {
        QGraphicsScene::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_K && !event->isAutoRepeat()) {
        if (static_cast<size_t>(m_currentTrackIndex + 1) >= m_levelData.segments.size()) {
            event->accept(); return;
        }

        const TrackSegmentData& currentSegment = m_levelData.segments[m_currentTrackIndex];
        qreal effectiveBallRadiusOnTrack = currentSegment.radius + m_orbitOffset;
        if (effectiveBallRadiusOnTrack < BALL_RADIUS) effectiveBallRadiusOnTrack = BALL_RADIUS;

        qreal angleDiff = m_currentAngle - ANGLE_TOP;
        while (angleDiff <= -M_PI) angleDiff += 2.0 * M_PI;
        while (angleDiff > M_PI) angleDiff -= 2.0 * M_PI;
        qreal absAngleDiff = qAbs(angleDiff);

        qreal angularSpeed = (effectiveBallRadiusOnTrack > 0.01) ? (m_linearSpeed / effectiveBallRadiusOnTrack) : 0;
        qreal perfectAngleTolerance = angularSpeed * (PERFECT_MS / 1000.0);
        qreal goodAngleTolerance = angularSpeed * (GOOD_MS / 1000.0);

        QString judgmentTextStrKey;
        QString judgmentTextColorName;
        bool success = false;
        int scoreGainedThisHit = 0;

        if (absAngleDiff <= perfectAngleTolerance) {
            judgmentTextStrKey = "PERFECT!"; judgmentTextColorName = "yellow"; success = true; scoreGainedThisHit = SCORE_PERFECT;
        } else if (absAngleDiff <= goodAngleTolerance) {
            judgmentTextStrKey = "GOOD!"; judgmentTextColorName = "cyan"; success = true; scoreGainedThisHit = SCORE_GOOD;
        } else {
            m_health--;
            updateHealthDisplay();
            triggerExplosionEffect();
            if (m_health <= 0) { endGame(); event->accept(); return; }
        }

        if (!judgmentTextStrKey.isEmpty() && m_judgmentText) {
            // 修改点：调整字号和加粗 (例如，判定文本24pt粗体)
            QString htmlText = QString("<span style=\"font-family: '%1'; font-size: 50pt; font-weight: bold; color: %2;\">%3</span>")
                                   .arg(m_englishFontFamily)
                                   .arg(judgmentTextColorName)
                                   .arg(judgmentTextStrKey);
            m_judgmentText->setHtml(htmlText);
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


// endGame 函数修改
void GameScene::endGame() {
    if(m_gameOver) return;
    m_gameOver = true;
    qDebug() << "Game Over. Final Score:" << m_score;

    if (m_timer->isActive()) m_timer->stop();
    if (m_judgmentTimer->isActive()) m_judgmentTimer->stop();
    if (m_damageCooldownTimer->isActive()) m_damageCooldownTimer->stop();

    if (m_explosionMovie && m_explosionMovie->state() == QMovie::Running) m_explosionMovie->stop();
    if (m_explosionItem) m_explosionItem->setVisible(false);
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) m_explosionDurationTimer->stop();
    if (m_collectEffectMovie && m_collectEffectMovie->state() == QMovie::Running) m_collectEffectMovie->stop();
    if (m_collectEffectItem) m_collectEffectItem->setVisible(false);
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) m_collectEffectDurationTimer->stop();

    if (m_targetDot) m_targetDot->setVisible(false);
    if (m_ball) m_ball->setVisible(false);
    if (m_sunItem) m_sunItem->setVisible(false);
    if (m_mercuryItem) m_mercuryItem->setVisible(false);
    if (m_venusItem) m_venusItem->setVisible(false);
    if (m_earthItem) m_earthItem->setVisible(false);
    if (m_marsItem) m_marsItem->setVisible(false);
    if (m_jupiterItem) m_jupiterItem->setVisible(false);
    if (m_saturnItem) m_saturnItem->setVisible(false);
    if (m_uranusItem) m_uranusItem->setVisible(false);
    if (m_neptuneItem) m_neptuneItem->setVisible(false);


    for (CollectibleItem* c : m_collectibles) if(c) c->setVisibleState(false);
    for (ObstacleItem* o : m_obstacles) if(o) o->setVisibleState(false);

    if (m_healthText) m_healthText->setVisible(false);
    if (m_judgmentText) m_judgmentText->setVisible(false);
    if (m_scoreText) m_scoreText->setVisible(false);

    if (!m_gameOverText) {
        m_gameOverText = new QGraphicsTextItem();
        addItem(m_gameOverText);
    }
    // 修改点：调整游戏结束文本的字号和加粗
    QString gameOverHtml = QString(
                               "<p align='center'>"
                               "<span style=\"font-family: '%1'; font-size: 26pt; font-weight: bold; color: red;\">游戏结束</span><br/>" // 例如，26pt粗体
                               "<span style=\"font-family: '%1'; font-size: 26pt; font-weight: bold; color: red;\">最终分数: </span>"
                               "<span style=\"font-family: '%2'; font-size: 28pt; font-weight: bold; color: red;\">%3</span><br/>" // 例如，分数数字28pt粗体
                               "<span style=\"font-family: '%1'; font-size: 26pt; font-weight: bold; color: red;\">按 空格键 重新开始</span>"
                               "</p>")
                               .arg(m_chineseFontFamily)
                               .arg(m_englishFontFamily)
                               .arg(m_score);
    m_gameOverText->setHtml(gameOverHtml);
    m_gameOverText->setZValue(3);
    m_gameOverText->setVisible(true);
    if (!views().isEmpty()) {
        QGraphicsView* view = views().first();
        QRectF viewRect = view->mapToScene(view->viewport()->geometry()).boundingRect();
        m_gameOverText->setPos(viewRect.center().x() - m_gameOverText->boundingRect().width() / 2.0,
                               viewRect.center().y() - m_gameOverText->boundingRect().height() / 2.0);
    } else {
        m_gameOverText->setPos(-m_gameOverText->boundingRect().width()/2, -m_gameOverText->boundingRect().height()/2);
    }
}


// 其他所有函数定义 (loadLevelData, addTrackItem, updateGame, 动画处理函数等等)
// ... (请确保这些函数定义完整存在)
bool GameScene::loadLevelData(const QString& filename)
{
    m_levelData.segments.clear();
    qDeleteAll(m_trackItems);
    m_trackItems.clear();

    if (!m_levelData.loadLevelFromFile(filename)) {
        qCritical() << "GameScene: Failed to load level data using TrackData class from file:" << filename;
        return false;
    }

    if (m_levelData.segments.empty()) {
        qWarning() << "GameScene: Level data loaded successfully, but no track segments found in" << filename;
    }

    for (size_t i = 0; i < m_levelData.segments.size(); ++i) {
        const TrackSegmentData& segmentData = m_levelData.segments[i];
        addTrackItem(segmentData);

        for (const CollectibleData& cData : segmentData.collectibles) {
            CollectibleItem *item = new CollectibleItem(static_cast<int>(i), qDegreesToRadians(cData.angleDegrees));
            item->setOrbitOffset(cData.radialOffset);
            connect(item, &CollectibleItem::collectedSignal, this, &GameScene::handleCollectibleCollected);
            m_collectibles.append(item);
        }

        for (const ObstacleData& oData : segmentData.obstacles) {
            ObstacleItem *item = new ObstacleItem(static_cast<int>(i), qDegreesToRadians(oData.angleDegrees));
            item->setOrbitOffset(oData.radialOffset);
            connect(item, &ObstacleItem::hitSignal, this, &GameScene::handleObstacleHit);
            m_obstacles.append(item);
        }
    }
    qDebug() << "GameScene: Successfully processed" << m_levelData.segments.size() << "segments from TrackData.";
    return true;
}

void GameScene::addTrackItem(const TrackSegmentData& segmentData) {
    QRectF bounds(segmentData.centerX - segmentData.radius,
                  segmentData.centerY - segmentData.radius,
                  2 * segmentData.radius,
                  2 * segmentData.radius);
    QGraphicsEllipseItem* trackItem = new QGraphicsEllipseItem(bounds);
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
        if (trackIdx >= 0 && static_cast<size_t>(trackIdx) < m_levelData.segments.size()) {
            const TrackSegmentData& segment = m_levelData.segments[trackIdx];
            collectible->updateVisualPosition(QPointF(segment.centerX, segment.centerY), segment.radius);
            collectible->setZValue(0.5);
            collectible->setVisibleState(true);
        } else {
            qWarning() << "Collectible has invalid track index:" << trackIdx;
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
        if (trackIdx >= 0 && static_cast<size_t>(trackIdx) < m_levelData.segments.size()) {
            const TrackSegmentData& segment = m_levelData.segments[trackIdx];
            obstacle->updateVisualPosition(QPointF(segment.centerX, segment.centerY), segment.radius);
            obstacle->setZValue(0.6);
            obstacle->setVisibleState(true);
        } else {
            qWarning() << "Obstacle has invalid track index:" << trackIdx;
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

    if (m_levelData.segments.empty() || !m_ball) {
        if (!m_gameOver) endGame();
        return;
    }
    if (m_currentTrackIndex < 0 || static_cast<size_t>(m_currentTrackIndex) >= m_levelData.segments.size()) {
        endGame();
        return;
    }

    const TrackSegmentData& currentSegment = m_levelData.segments[m_currentTrackIndex];
    qreal effectiveBallRadiusOnTrack = currentSegment.radius + m_orbitOffset;
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
    if (m_canTakeDamage) checkTrackCollisions();

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
        if (m_judgmentText && m_judgmentText->isVisible()) {
            if (!m_targetDot || !m_targetDot->isVisible()) {
                QRectF judgmentRect = m_judgmentText->boundingRect();
                m_judgmentText->setPos(viewRectForHUD.center() - QPointF(judgmentRect.width() / 2.0, judgmentRect.height() / 2.0 + 50));
            }
        }
    }
}


void GameScene::updateBallPosition() {
    if (!m_ball || m_levelData.segments.empty() || m_currentTrackIndex < 0 || static_cast<size_t>(m_currentTrackIndex) >= m_levelData.segments.size()) return;

    const TrackSegmentData& currentSegmentData = m_levelData.segments[m_currentTrackIndex];
    qreal effectiveRadius = currentSegmentData.radius + m_orbitOffset;

    if (effectiveRadius < BALL_RADIUS && m_orbitOffset < 0) effectiveRadius = BALL_RADIUS;
    else if (effectiveRadius < 0 && m_orbitOffset > 0) effectiveRadius = BALL_RADIUS;

    qreal x_center = currentSegmentData.centerX + effectiveRadius * qCos(m_currentAngle);
    qreal y_center = currentSegmentData.centerY + effectiveRadius * qSin(m_currentAngle);

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
    if (!m_targetDot || m_levelData.segments.empty() || m_currentTrackIndex < 0 || static_cast<size_t>(m_currentTrackIndex) >= m_levelData.segments.size()) {
        if(m_targetDot) m_targetDot->setVisible(false);
        return;
    }
    const TrackSegmentData& currentSegmentData = m_levelData.segments[m_currentTrackIndex];
    QPointF judgmentPoint = QPointF(currentSegmentData.centerX, currentSegmentData.centerY) +
                            QPointF(currentSegmentData.radius * qCos(ANGLE_TOP),
                                    currentSegmentData.radius * qSin(ANGLE_TOP));
    m_targetDot->setPos(judgmentPoint);
    m_targetDot->setVisible(true);
}

void GameScene::hideJudgmentText() { if (m_judgmentText) m_judgmentText->setVisible(false); }
void GameScene::enableDamageTaking() { m_canTakeDamage = true; }

void GameScene::checkTrackCollisions() {
    if (m_orbitOffset <= 0 || !m_ball || m_levelData.segments.empty() || !m_canTakeDamage || m_currentTrackIndex < 0) return;

    QPointF ballVisualCenter = m_ball->sceneBoundingRect().center();

    bool collisionOccurred = false;
    for (size_t i = 0; i < m_levelData.segments.size(); ++i) {
        if (static_cast<int>(i) == m_currentTrackIndex) continue;

        const TrackSegmentData& otherSegment = m_levelData.segments[i];
        QPointF otherTrackCenter(otherSegment.centerX, otherSegment.centerY);
        qreal distanceToOtherTrackCenter = QLineF(ballVisualCenter, otherTrackCenter).length();

        if (distanceToOtherTrackCenter < otherSegment.radius + BALL_RADIUS) {
            collisionOccurred = true;
            break;
        }
    }

    if (collisionOccurred) {
        m_health--;
        updateHealthDisplay();
        triggerExplosionEffect();
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
            collectible->collect();
        }
    }
}

void GameScene::checkBallObstacleCollisions() {
    if (!m_ball) return;
    QList<QGraphicsItem*> collidingItemsList = m_ball->collidingItems();
    for (QGraphicsItem* item : collidingItemsList) {
        ObstacleItem* obstacle = dynamic_cast<ObstacleItem*>(item);
        if (obstacle && !obstacle->isHit()) {
            obstacle->processHit();
        }
    }
}

void GameScene::handleCollectibleCollected(CollectibleItem* item) {
    if (!item) return;
    triggerCollectEffect();
    m_score += item->getScoreValue();
    updateScoreDisplayAndSpeed();
}

void GameScene::handleObstacleHit(ObstacleItem* item) {
    if (!item || !m_canTakeDamage) return;

    m_health--;
    updateHealthDisplay();
    triggerExplosionEffect();

    m_canTakeDamage = false;
    m_damageCooldownTimer->start(DAMAGE_COOLDOWN_MS);

    if (m_health <= 0) {
        endGame();
    }
}


void GameScene::switchTrack() {
    if (static_cast<size_t>(m_currentTrackIndex + 1) >= m_levelData.segments.size()) return;

    m_currentTrackIndex++;
    m_rotationDirection = -m_rotationDirection;
    m_currentAngle = ANGLE_BOTTOM;
    m_orbitOffset = qAbs(m_orbitOffset);

    updateTargetDotPosition();
    updateBallPosition();
}

void GameScene::triggerExplosionEffect() {
    if (!m_explosionMovie || !m_explosionItem || !m_ball || !m_explosionDurationTimer) {
        qWarning() << "triggerExplosionEffect: One or more essential members are null. Movie:" << m_explosionMovie << "Item:" << m_explosionItem << "Ball:" << m_ball << "Timer:" << m_explosionDurationTimer;
        return;
    }
    if (!m_explosionMovie->isValid()) {
        qWarning() << "Explosion GIF is not valid, cannot play.";
        return;
    }

    if (m_explosionDurationTimer->isActive()) m_explosionDurationTimer->stop();
    if (m_explosionMovie->state() == QMovie::Running) m_explosionMovie->stop();
    m_explosionMovie->jumpToFrame(0);

    QPointF effectCenter = m_ball->sceneBoundingRect().center();
    QPixmap originalFrame = m_explosionMovie->currentPixmap();
    if (originalFrame.isNull()) {
        qWarning() << "Explosion GIF current frame is null for triggerExplosionEffect.";
        return;
    }

    qreal effectDisplayDiameter = 5.0 * BALL_RADIUS;
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_explosionItem->setPixmap(scaledFrame);
    m_explosionItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));
    m_explosionItem->setVisible(true);
    m_explosionMovie->start();
    m_explosionDurationTimer->start(600);
}

void GameScene::updateExplosionFrame(int frameNumber) {
    Q_UNUSED(frameNumber);
    if (!m_explosionMovie || !m_explosionItem || m_explosionMovie->state() != QMovie::Running) return;
    if (!m_ball) return;

    QPixmap originalFrame = m_explosionMovie->currentPixmap();
    if (originalFrame.isNull()) return;

    qreal effectDisplayDiameter = 5.0 * BALL_RADIUS;
    QSize targetSize(static_cast<int>(effectDisplayDiameter), static_cast<int>(effectDisplayDiameter));
    QPixmap scaledFrame = originalFrame.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_explosionItem->setPixmap(scaledFrame);
    if (m_ball->isVisible()) {
        QPointF effectCenter = m_ball->sceneBoundingRect().center();
        m_explosionItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));
    }
}

void GameScene::hideExplosionEffect() {
    if (m_explosionItem) m_explosionItem->setVisible(false);
    if (m_explosionDurationTimer && m_explosionDurationTimer->isActive()) m_explosionDurationTimer->stop();
}

void GameScene::forceHideExplosion() {
    if (m_explosionMovie && m_explosionMovie->state() == QMovie::Running) m_explosionMovie->stop();
    if (m_explosionItem) m_explosionItem->setVisible(false);
}

void GameScene::triggerCollectEffect() {
    if (!m_collectEffectMovie || !m_collectEffectItem || !m_collectEffectDurationTimer || !m_ball) {
        qWarning() << "triggerCollectEffect: One or more essential members are null. Movie:" << m_collectEffectMovie << "Item:" << m_collectEffectItem << "Timer:" << m_collectEffectDurationTimer << "Ball:" << m_ball;
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
    m_collectEffectDurationTimer->start(600);
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
    if (m_ball->isVisible()) {
        QPointF effectCenter = m_ball->sceneBoundingRect().center();
        m_collectEffectItem->setPos(effectCenter - QPointF(scaledFrame.width() / 2.0, scaledFrame.height() / 2.0));
    }
}

void GameScene::hideCollectEffect() {
    if (m_collectEffectItem) m_collectEffectItem->setVisible(false);
    if (m_collectEffectDurationTimer && m_collectEffectDurationTimer->isActive()) m_collectEffectDurationTimer->stop();
}

void GameScene::forceHideCollectEffect() {
    if (m_collectEffectMovie && m_collectEffectMovie->state() == QMovie::Running) m_collectEffectMovie->stop();
    if (m_collectEffectItem) m_collectEffectItem->setVisible(false);
}


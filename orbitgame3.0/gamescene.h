#ifndef GAMESCENE_H
#define GAMESCENE_H

#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QTimer>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <QPen>
#include <QBrush>
#include <QtMath> // For qDegreesToRadians, qCos, qSin, M_PI etc.
#include <QGraphicsTextItem>
#include <QList>
#include <QLineF>
#include <QPixmap>
#include <QSizeF>

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>

#include <QMovie>

#include "trackdata.h"
#include "collectibleitem.h"
#include "obstacleitem.h"

// --- 游戏常量 ---
const qreal BASE_LINEAR_SPEED = 200.0;
const int SCORE_PERFECT = 2;
const int SCORE_GOOD = 1;
const int SCORE_THRESHOLD_FOR_SPEEDUP = 30;
const qreal SPEEDUP_FACTOR = 1.1;
const qreal BALL_RADIUS = 10.0;
const qreal TARGET_DOT_RADIUS = 5.0;
const qreal ORBIT_PADDING = 3.0;
const qreal PERFECT_MS = 80.0;
const qreal GOOD_MS = 160.0;
const int DAMAGE_COOLDOWN_MS = 500;
const int BG_GRID_SIZE = 3;
const qreal DEFAULT_COLLECTIBLE_EFFECT_SIZE_MULTIPLIER = 4.0;
extern const qreal DEFAULT_COLLECTIBLE_TARGET_SIZE;


class GameScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit GameScene(QObject *parent = nullptr);
    ~GameScene();

    void initializeGame();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateGame();
    void hideJudgmentText();
    void enableDamageTaking();
    void handleCollectibleCollected(CollectibleItem* item);
    void handleObstacleHit(ObstacleItem* item);

    void triggerExplosionEffect();
    void updateExplosionFrame(int frameNumber);
    void hideExplosionEffect();
    void forceHideExplosion();

    void triggerCollectEffect();
    void updateCollectEffectFrame(int frameNumber);
    void hideCollectEffect();
    void forceHideCollectEffect();

private:
    // --- Game State Members ---
    bool m_gameOver;
    int m_currentTrackIndex;
    qreal m_currentAngle;
    qreal m_orbitOffset;
    bool m_canTakeDamage;

    // --- Movement and Speed ---
    qreal m_linearSpeed;
    int m_rotationDirection;

    // --- Score and Difficulty ---
    int m_score;
    int m_speedLevel;

    // --- Health System ---
    const int m_maxHealth = 10; // 你之前修改过的值
    int m_health;

    // --- Core Graphics Items ---
    QGraphicsPixmapItem *m_ball;
    QGraphicsEllipseItem *m_targetDot;
    QList<QGraphicsEllipseItem*> m_trackItems;
    QGraphicsPixmapItem *m_sunItem;
    QGraphicsPixmapItem *m_mercuryItem;
    QGraphicsPixmapItem *m_venusItem;   // 金星
    QGraphicsPixmapItem *m_earthItem;
    QGraphicsPixmapItem *m_marsItem;
    QGraphicsPixmapItem *m_jupiterItem;
    QGraphicsPixmapItem *m_saturnItem;
    QGraphicsPixmapItem *m_uranusItem;
    QGraphicsPixmapItem *m_neptuneItem;


    // --- Background Elements ---
    QList<QGraphicsPixmapItem*> m_backgroundTiles;
    QPixmap m_backgroundTilePixmap;
    QSizeF m_backgroundTileSize;

    // --- HUD and Feedback Text ---
    QGraphicsTextItem *m_healthText;
    QGraphicsTextItem *m_judgmentText;
    QGraphicsTextItem *m_gameOverText;
    QGraphicsTextItem *m_scoreText;

    // --- Drawing Styles ---
    QBrush m_trackBrush;
    QPen m_trackPen;
    QBrush m_targetBrush;
    QPen m_targetPen;

    // --- Timers ---
    QTimer *m_timer;
    QTimer *m_judgmentTimer;
    QTimer *m_damageCooldownTimer;

    // --- Level Data ---
    TrackData m_levelData;

    // --- Game Object Lists ---
    QList<CollectibleItem*> m_collectibles;
    QList<ObstacleItem*> m_obstacles;

    // --- Audio ---
    QMediaPlayer *m_backgroundMusicPlayer;
    QAudioOutput *m_audioOutput;

    // --- Animation Members (Explosion) ---
    QMovie *m_explosionMovie;
    QGraphicsPixmapItem *m_explosionItem;
    QTimer *m_explosionDurationTimer;

    // --- Animation Members (Collect Effect) ---
    QMovie *m_collectEffectMovie;
    QGraphicsPixmapItem *m_collectEffectItem;
    QTimer *m_collectEffectDurationTimer;

    // --- Font Family Names ---
    QString m_englishFontFamily; // 用于存储加载的英文字体家族名称
    QString m_chineseFontFamily; // 用于存储加载的中文字体家族名称


    // --- Private Helper Functions ---
    bool loadLevelData(const QString& filename);
    void addTrackItem(const TrackSegmentData& segmentData);

    void updateBallPosition();
    void updateTargetDotPosition();
    void switchTrack();
    void endGame();

    void updateHealthDisplay();
    void updateScoreDisplayAndSpeed();

    void checkTrackCollisions();
    void checkBallCollectibleCollisions();
    void checkBallObstacleCollisions();

    void clearAllGameItems();
    void clearAllCollectibles();
    void positionAndShowCollectibles();
    void clearAllObstacles();
    void positionAndShowObstacles();

    void updateInfiniteBackground();
};

#endif // GAMESCENE_H

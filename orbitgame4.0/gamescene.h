#ifndef GAMESCENE_H
#define GAMESCENE_H

#include <QGraphicsScene>
#include <QGraphicsEllipseItem> // 需要包含它，因为 m_endTriggerPoint 是这个类型
#include <QGraphicsPixmapItem>
#include <QTimer>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <QPen>
#include <QBrush>
#include <QtMath>
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
#include "gameoverdisplay.h"
#include "endtriggeritem.h" // <--- 包含新创建的 EndTriggerItem 头文件

// --- 游戏常量 ---
const qreal BASE_LINEAR_SPEED = 150.0;
const int SCORE_PERFECT = 2;
const int SCORE_GOOD = 1;
const int SCORE_THRESHOLD_FOR_SPEEDUP = 50;
const qreal SPEEDUP_FACTOR = 1.1;
const qreal BALL_RADIUS = 10.0;
const qreal TARGET_DOT_RADIUS = 5.0;
const qreal ORBIT_PADDING = 3.0;
const qreal PERFECT_MS = 80.0;
const qreal GOOD_MS = 160.0;
const int DAMAGE_COOLDOWN_MS = 500;
const int BG_GRID_SIZE = 3;
const qreal DEFAULT_COLLECTIBLE_EFFECT_SIZE_MULTIPLIER = 4.0;
extern const qreal DEFAULT_COLLECTIBLE_TARGET_SIZE; // 假设在 collectibleitem.h 中定义并初始化
// const qreal END_POINT_RADIUS = 15.0; // 已在 endtriggeritem.h 中定义为 DEFAULT_END_POINT_RADIUS，或者您可以在此统一定义


class GameScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit GameScene(QObject *parent = nullptr);
    ~GameScene();

    void initializeGame();

signals:
    void returnToStartScreenRequested(); // 用于生命耗尽后，从 GameOverDisplay 返回主菜单
    void endGameVideoRequested();        // <--- 新增信号：当碰到通关点时发出

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

    void handleGameOverRestart();    // 处理来自 GameOverDisplay 的重新开始请求
    void handleGameOverReturnToMain(); // 处理来自 GameOverDisplay 的返回主菜单请求

private:
    // --- Game State Members ---
    bool m_gameOver; // 主要用于标记生命耗尽的游戏结束状态
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
    const int m_maxHealth = 10;
    int m_health;

    // --- Core Graphics Items ---
    QGraphicsPixmapItem *m_ball;
    QGraphicsEllipseItem *m_targetDot;
    QList<QGraphicsEllipseItem*> m_trackItems;
    QGraphicsPixmapItem *m_sunItem;
    QGraphicsPixmapItem *m_mercuryItem;
    QGraphicsPixmapItem *m_venusItem;
    QGraphicsPixmapItem *m_earthItem;
    QGraphicsPixmapItem *m_marsItem;
    QGraphicsPixmapItem *m_jupiterItem;
    QGraphicsPixmapItem *m_saturnItem;
    QGraphicsPixmapItem *m_uranusItem;
    QGraphicsPixmapItem *m_neptuneItem;
    EndTriggerItem *m_endTriggerPoint; // <--- 通关触发点 (使用新类)


    // --- Background Elements ---
    QList<QGraphicsPixmapItem*> m_backgroundTiles;
    QPixmap m_backgroundTilePixmap;
    QSizeF m_backgroundTileSize;

    // --- HUD and Feedback Text ---
    QGraphicsTextItem *m_healthText;
    QGraphicsTextItem *m_judgmentText;
    GameOverDisplay *m_gameOverDisplay; // 用于生命耗尽的游戏结束界面
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
    QString m_englishFontFamily;
    QString m_chineseFontFamily;


    // --- Private Helper Functions ---
    bool loadLevelData(const QString& filename);
    void addTrackItem(const TrackSegmentData& segmentData);

    void updateBallPosition();
    void updateTargetDotPosition();
    void switchTrack();
    void endGame(); // 这是生命耗尽时的游戏结束处理

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

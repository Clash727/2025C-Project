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

#include <QMovie> // <<< 确保 QMovie 被包含

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

const int BG_GRID_SIZE = 3; // e.g., 3x3 grid of tiles for background
// 收集品动画可能需要的大小，可以根据你的GIF调整
const qreal DEFAULT_COLLECTIBLE_EFFECT_SIZE_MULTIPLIER = 4; // 收集品效果动画相对于收集品本身大小的倍数
extern const qreal DEFAULT_COLLECTIBLE_TARGET_SIZE; // 假设这个在 collectibleitem.h 中定义或在此处定义


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

    // 爆炸效果 (用于障碍物/失误)
    void triggerExplosionEffect();
    void updateExplosionFrame(int frameNumber);
    void hideExplosionEffect();
    void forceHideExplosion();

    // 收集品效果
    void triggerCollectEffect(); // <<< 修改: 移除 position 参数
    void updateCollectEffectFrame(int frameNumber);
    void hideCollectEffect();
    void forceHideCollectEffect();


private:
    // 游戏状态
    bool m_gameOver;
    int m_currentTrackIndex;
    qreal m_currentAngle;
    qreal m_orbitOffset;
    bool m_canTakeDamage;

    // 移动与速度
    qreal m_linearSpeed;
    int m_rotationDirection;

    // 得分与难度
    int m_score;
    int m_speedLevel;

    // 生命系统
    const int m_maxHealth = 54;
    int m_health;

    // 图形项
    QGraphicsPixmapItem *m_ball;
    QGraphicsEllipseItem *m_targetDot;
    QList<QGraphicsEllipseItem*> m_trackItems;

    // 背景相关 (无限滚动)
    QList<QGraphicsPixmapItem*> m_backgroundTiles;
    QPixmap m_backgroundTilePixmap;
    QSizeF m_backgroundTileSize;

    // HUD和反馈文本项
    QGraphicsTextItem *m_healthText;
    QGraphicsTextItem *m_judgmentText;
    QGraphicsTextItem *m_gameOverText;
    QGraphicsTextItem *m_scoreText;

    // 画刷和画笔
    QBrush m_trackBrush;
    QPen m_trackPen;
    QBrush m_targetBrush;
    QPen m_targetPen;

    // 计时器
    QTimer *m_timer;
    QTimer *m_judgmentTimer;
    QTimer *m_damageCooldownTimer;

    // 轨道数据
    QList<TrackData> m_tracks;

    // 收集品和障碍物数据
    QList<CollectibleItem*> m_collectibles;
    QList<ObstacleItem*> m_obstacles;

    // 背景音乐相关
    QMediaPlayer *m_backgroundMusicPlayer;
    QAudioOutput *m_audioOutput;

    // GIF 动画相关 (爆炸效果 - 用于障碍物和失误)
    QMovie *m_explosionMovie;
    QGraphicsPixmapItem *m_explosionItem;
    QTimer *m_explosionDurationTimer;

    // GIF 动画相关 (收集品效果)
    QMovie *m_collectEffectMovie;
    QGraphicsPixmapItem *m_collectEffectItem;
    QTimer *m_collectEffectDurationTimer;


    // 辅助函数
    bool loadTracksAndCollectiblesFromFile(const QString& filename);
    void addTrackItem(const TrackData& trackData);
    void updateBallPosition();
    void updateTargetDotPosition();
    void switchTrack();
    void endGame();
    void updateHealthDisplay();
    void updateScoreDisplayAndSpeed();
    void checkCollisions();

    void clearAllCollectibles();
    void positionAndShowCollectibles();
    void checkBallCollectibleCollisions();

    void clearAllObstacles();
    void positionAndShowObstacles();
    void checkBallObstacleCollisions();

    void updateInfiniteBackground();
};

#endif // GAMESCENE_H

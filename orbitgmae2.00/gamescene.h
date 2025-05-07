// Gamescene.h
#ifndef GAMESCENE_H
#define GAMESCENE_H

#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QTimer>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <QPen>
#include <QBrush>
#include <QtMath>
#include <QGraphicsTextItem>
#include <QList>
#include <QLineF>

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

// <<< 新增包含 >>>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
// <<< 结束新增包含 >>>

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
    int m_health;
    const int m_maxHealth = 5;

    // 图形项
    QGraphicsEllipseItem *m_ball;
    QGraphicsEllipseItem *m_targetDot;
    QList<QGraphicsEllipseItem*> m_trackItems;

    // HUD和反馈文本项
    QGraphicsTextItem *m_healthText;
    QGraphicsTextItem *m_judgmentText;
    QGraphicsTextItem *m_gameOverText;
    QGraphicsTextItem *m_scoreText;

    // 画刷和画笔
    QBrush m_ballBrush;
    QPen m_ballPen;
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

    // 收集品数据
    QList<CollectibleItem*> m_collectibles;
    QList<ObstacleItem*> m_obstacles;

    // <<< 新增背景音乐成员变量 >>>
    QMediaPlayer *m_backgroundMusicPlayer;
    QAudioOutput *m_audioOutput;
    // <<< 结束新增背景音乐成员变量 >>>

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
};

#endif // GAMESCENE_H

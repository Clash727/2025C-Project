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
#include <QGraphicsTextItem> // 已包含

// Struct to hold data for each circular track
struct TrackData {
    QPointF center;
    qreal radius;
    qreal tangentAngle; // Angle on *this* track where the *next* track will connect
    QRectF bounds; // Pre-calculated bounding rectangle for the ellipse item

    TrackData(const QPointF& c, qreal r) : center(c), radius(r) {
        bounds = QRectF(center.x() - radius, center.y() - radius, 2 * radius, 2 * radius);
    }
};

// --- 新增：常量 ---
const qreal BASE_LINEAR_SPEED = 200.0; // 基础线速度
const int SCORE_PERFECT = 2;          // Perfect 得分
const int SCORE_GOOD = 1;             // Good 得分
const int SCORE_THRESHOLD_FOR_SPEEDUP = 30; // 触发加速的分数阈值
const qreal SPEEDUP_FACTOR = 1.1;      // 速度增加因子
// --- 结束新增 ---


class GameScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit GameScene(QObject *parent = nullptr);
    ~GameScene();

    void initializeGame(); // Setup for a new game

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateGame(); // Game loop / physics update
    void hideJudgmentText(); // Hide the judgment text after a delay

private:
    // Game State
    bool m_gameOver;
    int m_currentTrackIndex; // Index of the track the ball is currently on
    qreal m_currentAngle; // Current angle of the ball on the current track (radians)

    // Constant Linear Speed and Rotation Direction
    qreal m_linearSpeed;       // 当前线速度 (会变化)
    int m_rotationDirection; // +1 表示一个方向，-1 表示另一个方向

    // --- 新增：计分和变速系统 ---
    int m_score;               // 当前分数
    int m_speedLevel;          // 当前速度等级 (基于分数达到阈值的次数)
    // --- 结束新增 ---


    // Health System
    int m_health;
    int m_maxHealth = 5; // Total health points

    // Judgment Timing (in milliseconds)
    const qreal PERFECT_MS = 80.0;
    const qreal GOOD_MS = 160.0;

    // Graphics Items
    QGraphicsEllipseItem *m_ball;
    QGraphicsEllipseItem *m_targetDot;
    QList<QGraphicsEllipseItem*> m_trackItems;

    // Text Items for HUD and Feedback
    QGraphicsTextItem *m_healthText;
    QGraphicsTextItem *m_judgmentText;
    QGraphicsTextItem *m_gameOverText;
    QGraphicsTextItem *m_scoreText; // --- 新增：分数文本项 ---

    // Pens and Brushes
    QBrush m_ballBrush;
    QPen m_ballPen;
    QBrush m_trackBrush;
    QPen m_trackPen;
    QBrush m_targetBrush;
    QPen m_targetPen;

    // Game Loop Timer
    QTimer *m_timer;

    // Timer for judgment text visibility
    QTimer *m_judgmentTimer;


    // Track Data
    QList<TrackData> m_tracks; // Data for all generated tracks


    // Helper Functions
    TrackData generateNextTrack(const TrackData& currentTrack);
    void addTrackItem(const TrackData& trackData);
    void updateBallPosition();
    void updateTargetDotPosition();
    void switchTrack(); // Logic for switching to the next track
    void endGame();     // Handle game over state
    void cleanupOldTracks(); // Remove track visuals that are far behind
    void updateHealthDisplay();
    void updateScoreDisplayAndSpeed(); // --- 新增：更新分数显示和速度的辅助函数 ---
};

#endif // GAMESCENE_H

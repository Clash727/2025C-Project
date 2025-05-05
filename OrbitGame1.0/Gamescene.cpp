#include "Gamescene.h"
#include <QGraphicsView>
#include <QtMath> // For qCos, qSin, M_PI, qAtan2, qDegreesToRadians, qRadiansToDegrees
#include <QDebug> // For printing debug information
#include <QGraphicsTextItem>
#include <QRandomGenerator> // For QRandomGenerator
#include <QFont> // Added for font
#include <QBrush> // Added for brushes


// Constants (这些也可以移到 .h 或单独的常量文件)
const qreal BALL_RADIUS = 10.0;
const qreal TARGET_DOT_RADIUS = 5.0;
const qreal INITIAL_TRACK_RADIUS = 100.0;
const qreal MIN_TRACK_RADIUS = 50.0;
const qreal MAX_TRACK_RADIUS = 150.0;
const int TRACK_HISTORY_LIMIT = 5; // How many old tracks to keep visually


GameScene::GameScene(QObject *parent)
    : QGraphicsScene(parent),
    m_ballBrush(Qt::white),
    m_ballPen(Qt::black, 1),
    m_trackBrush(Qt::NoBrush), // Tracks are just outlines
    m_trackPen(Qt::gray, 2),
    m_targetBrush(Qt::gray),
    m_targetPen(Qt::NoPen), // Target dot is solid

    // 初始化成员变量
    m_linearSpeed(BASE_LINEAR_SPEED), // 初始化为基础速度
    m_rotationDirection(1), // 初始化旋转方向

    // 初始化文本项指针为空
    m_healthText(nullptr),
    m_judgmentText(nullptr),
    m_gameOverText(nullptr),
    m_scoreText(nullptr) // 初始化分数文本指针
{
    // 初始场景大小
    setSceneRect(-500, -500, 1000, 1000);

    // 设置游戏循环计时器
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GameScene::updateGame);

    // 设置判断文本计时器
    m_judgmentTimer = new QTimer(this);
    m_judgmentTimer->setSingleShot(true); // 计时器触发一次
    connect(m_judgmentTimer, &QTimer::timeout, this, &GameScene::hideJudgmentText); // 连接到隐藏文本的槽

    initializeGame(); // 调用 initializeGame 设置初始状态
    m_timer->start(16); // 启动游戏循环 (~60 FPS)
}

GameScene::~GameScene()
{
    // Qt 会自动管理子对象和连接，无需手动删除计时器和图形项
}

void GameScene::initializeGame()
{
    m_gameOver = false;
    m_currentTrackIndex = 0;
    m_currentAngle = M_PI; // 球从第一个切点的对面开始

    // 重置生命值
    m_health = m_maxHealth;

    // 重置计分、速度等级和线速度
    m_score = 0;
    m_speedLevel = 0;
    m_linearSpeed = BASE_LINEAR_SPEED; // 每次重新开始都重置为基础速度
    m_rotationDirection = 1; // 游戏重新开始时重置方向

    // 清除现有项目 (如果重启)
    clear(); // 移除所有图形项，包括旧轨道和文本项
    m_tracks.clear();
    m_trackItems.clear(); // 清空视觉轨道列表

    // 重置文本项指针 (因为 clear() 会删除它们)
    m_healthText = nullptr;
    m_judgmentText = nullptr;
    m_gameOverText = nullptr;
    m_scoreText = nullptr; // 重置分数文本指针


    // 创建第一个轨道
    TrackData track1(QPointF(0, 0), INITIAL_TRACK_RADIUS);
    track1.tangentAngle = QRandomGenerator::global()->bounded(2.0 * M_PI);
    m_tracks.append(track1);
    addTrackItem(track1); // 调用 addTrackItem

    // 生成第二个轨道
    TrackData track2 = generateNextTrack(track1);
    m_tracks.append(track2);
    addTrackItem(track2); // 调用 addTrackItem

    // 创建球
    m_ball = new QGraphicsEllipseItem();
    m_ball->setRect(-BALL_RADIUS, -BALL_RADIUS, 2 * BALL_RADIUS, 2 * BALL_RADIUS);
    m_ball->setBrush(m_ballBrush);
    m_ball->setPen(m_ballPen);
    m_ball->setZValue(1);
    addItem(m_ball);
    updateBallPosition(); // 初始化时放置球

    // 创建目标点
    m_targetDot = new QGraphicsEllipseItem();
    m_targetDot->setRect(-TARGET_DOT_RADIUS, -TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS, 2 * TARGET_DOT_RADIUS);
    m_targetDot->setBrush(m_targetBrush);
    m_targetDot->setPen(m_targetPen);
    m_targetDot->setZValue(1);
    addItem(m_targetDot);
    updateTargetDotPosition(); // 将点放置在第一个切点
    m_targetDot->setVisible(true); // 确保目标点在开始时可见

    // 设置生命值文本
    m_healthText = new QGraphicsTextItem();
    m_healthText->setFont(QFont("Arial", 16, QFont::Bold));
    m_healthText->setDefaultTextColor(Qt::green);
    m_healthText->setZValue(2);
    addItem(m_healthText);
    updateHealthDisplay(); // 设置初始文本

    // 设置判断结果文本
    m_judgmentText = new QGraphicsTextItem();
    m_judgmentText->setFont(QFont("Arial", 20, QFont::Bold));
    m_judgmentText->setZValue(2);
    m_judgmentText->setVisible(false); // 初始时隐藏
    addItem(m_judgmentText);

    // 设置分数文本
    m_scoreText = new QGraphicsTextItem();
    m_scoreText->setFont(QFont("Arial", 16, QFont::Bold));
    m_scoreText->setDefaultTextColor(Qt::white); // 设置颜色
    m_scoreText->setZValue(2); // 绘制在最上面
    addItem(m_scoreText);
    updateScoreDisplayAndSpeed(); // 初始化分数显示（此时应显示为 0）

    // 确保计时器正在运行
    if (!m_timer->isActive()) {
        m_timer->start(16);
    }
}

// Generates data for the next track, tangent to the current one
TrackData GameScene::generateNextTrack(const TrackData& currentTrack) {
    qreal angle = currentTrack.tangentAngle;
    QPointF tangentPoint = currentTrack.center + QPointF(
                               currentTrack.radius * qCos(angle),
                               currentTrack.radius * qSin(angle)
                               );

    qreal range = MAX_TRACK_RADIUS - MIN_TRACK_RADIUS;
    qreal randomOffset = QRandomGenerator::global()->generateDouble() * range;
    qreal newRadius = MIN_TRACK_RADIUS + randomOffset;

    QPointF vec = tangentPoint - currentTrack.center;
    if (vec.manhattanLength() < 1e-6) {
        vec = QPointF(1, 0);
    }
    QPointF unitVec = vec / currentTrack.radius;

    QPointF newCenter = tangentPoint + unitVec * newRadius;
    TrackData nextTrack(newCenter, newRadius);
    nextTrack.tangentAngle = QRandomGenerator::global()->bounded(2.0 * M_PI);

    return nextTrack;
}

// Adds a visual representation of the track to the scene (确保这个函数存在!)
void GameScene::addTrackItem(const TrackData& trackData) {
    QGraphicsEllipseItem* trackItem = new QGraphicsEllipseItem(trackData.bounds); // 使用边界矩形创建椭圆项
    trackItem->setPen(m_trackPen);
    trackItem->setBrush(m_trackBrush);
    trackItem->setZValue(0); // Draw tracks below ball and dot
    addItem(trackItem);
    m_trackItems.append(trackItem); // Keep track of visual items for cleanup
}

void GameScene::updateGame()
{
    if (m_gameOver) return;

    // 1. 计算角速度和角度变化 (基于当前 m_linearSpeed)
    if (m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) return; // 安全检查

    const TrackData& currentTrack = m_tracks[m_currentTrackIndex];
    qreal currentRadius = currentTrack.radius;
    if (currentRadius < 1.0) currentRadius = 1.0; // 防止除零

    qreal currentAngularVelocity = m_linearSpeed / currentRadius; // 使用可能已改变的线速度
    qreal timeDelta = m_timer->interval() / 1000.0; // 毫秒转秒
    qreal angleDelta = currentAngularVelocity * timeDelta;

    m_currentAngle += angleDelta * m_rotationDirection; // 应用角度变化和方向

    // 保持角度在 [0, 2*PI)
    while (m_currentAngle >= 2.0 * M_PI) { m_currentAngle -= 2.0 * M_PI; }
    while (m_currentAngle < 0) { m_currentAngle += 2.0 * M_PI; }

    // 2. 更新球的位置
    updateBallPosition();

    // 3. 更新视图和 HUD 位置
    if (!views().isEmpty()) {
        QGraphicsView* view = views().first();
        view->centerOn(m_ball); // 视图跟随球

        // 获取视图在场景中的矩形，用于定位 HUD
        QRectF viewRect = view->mapToScene(view->viewport()->geometry()).boundingRect();

        // 定位生命值文本 (左上角)
        if (m_healthText) {
            m_healthText->setPos(viewRect.topLeft() + QPointF(10, 10)); // 加一点边距
        }

        // 定位分数文本 (右上角)
        if (m_scoreText) {
            QRectF scoreRect = m_scoreText->boundingRect(); // 获取文本边界以计算宽度
            m_scoreText->setPos(viewRect.topRight() - QPointF(scoreRect.width() + 10, -10)); // 右上角，加边距
        }

        // 定位 Game Over 文本 (如果游戏结束且文本存在)
        if (m_gameOverText && m_gameOver) {
            QRectF gameOverRect = m_gameOverText->boundingRect();
            QPointF viewCenter = viewRect.center();
            m_gameOverText->setPos(viewCenter.x() - gameOverRect.width()/2.0, viewCenter.y() - gameOverRect.height()/2.0);
        }

        // 扩展场景矩形以包含视图
        if (!sceneRect().contains(viewRect)) {
            setSceneRect(sceneRect().united(viewRect).adjusted(-100, -100, 100, 100));
        }
    }
    // 4. Game over condition checked in keyPressEvent
}

void GameScene::updateBallPosition() {
    if (!m_ball || m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) return;

    const TrackData& currentTrack = m_tracks[m_currentTrackIndex];
    qreal x = currentTrack.center.x() + currentTrack.radius * qCos(m_currentAngle);
    qreal y = currentTrack.center.y() + currentTrack.radius * qSin(m_currentAngle);
    // setPos 设置的是左上角坐标，所以需要减去半径
    m_ball->setPos(x - BALL_RADIUS, y - BALL_RADIUS);
}

void GameScene::updateTargetDotPosition() {
    if (!m_targetDot || m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) return;

    const TrackData& currentTrack = m_tracks[m_currentTrackIndex];
    qreal angle = currentTrack.tangentAngle;

    QPointF tangentPoint = currentTrack.center + QPointF(
                               currentTrack.radius * qCos(angle),
                               currentTrack.radius * qSin(angle)
                               );
    // setPos 设置的是左上角坐标
    m_targetDot->setPos(tangentPoint.x() - TARGET_DOT_RADIUS, tangentPoint.y() - TARGET_DOT_RADIUS);
    m_targetDot->setVisible(true); // 确保可见
}

void GameScene::keyPressEvent(QKeyEvent *event)
{
    // 游戏进行中，按下空格键且非自动重复
    if (!m_gameOver && event->key() == Qt::Key_Space && !event->isAutoRepeat()) {

        if (m_currentTrackIndex < 0 || m_currentTrackIndex >= m_tracks.size()) return; // 安全检查

        const TrackData& currentTrack = m_tracks[m_currentTrackIndex];
        qreal targetAngle = currentTrack.tangentAngle;
        qreal currentRadius = currentTrack.radius;

        // 计算角度差，处理环绕
        qreal angleDiff = m_currentAngle - targetAngle;
        while (angleDiff <= -M_PI) angleDiff += 2.0 * M_PI;
        while (angleDiff > M_PI) angleDiff -= 2.0 * M_PI;
        qreal absAngleDiff = qAbs(angleDiff);

        // 计算容错角度 (基于当前速度和半径)
        if (currentRadius < 1.0) currentRadius = 1.0; // 防错
        // 容错时间窗口 (秒) * 角速度 (弧度/秒) = 容错角度 (弧度)
        qreal perfectAngleTolerance = (m_linearSpeed / currentRadius) * (PERFECT_MS / 1000.0);
        qreal goodAngleTolerance = (m_linearSpeed / currentRadius) * (GOOD_MS / 1000.0);

        qDebug() << "Space Pressed! Angle:" << qRadiansToDegrees(m_currentAngle)
                 << "Target:" << qRadiansToDegrees(targetAngle)
                 << "Abs Diff (rad):" << absAngleDiff
                 << "Perfect Tol:" << perfectAngleTolerance
                 << "Good Tol:" << goodAngleTolerance;

        // 判断结果
        QString judgmentText;
        QColor textColor;
        bool success = false;
        int scoreGained = 0; // 本次操作获得的得分

        if (absAngleDiff <= perfectAngleTolerance) {
            judgmentText = "PERFECT!";
            textColor = Qt::yellow;
            success = true;
            scoreGained = SCORE_PERFECT; // 完美得分
        } else if (absAngleDiff <= goodAngleTolerance) {
            judgmentText = "GOOD!";
            textColor = Qt::cyan;
            success = true;
            scoreGained = SCORE_GOOD; // 良好得分
        } else {
            judgmentText = "MISS!";
            textColor = Qt::red;
            success = false; // Miss!
        }

        // 显示判断文本
        if (m_judgmentText) {
            m_judgmentText->setPlainText(judgmentText);
            m_judgmentText->setDefaultTextColor(textColor);
            m_judgmentText->setVisible(true);
            // 定位判断文本在目标点附近
            if (m_targetDot) {
                QPointF targetDotCenter = m_targetDot->pos() + QPointF(TARGET_DOT_RADIUS, TARGET_DOT_RADIUS);
                QRectF judgmentRect = m_judgmentText->boundingRect();
                qreal textX = targetDotCenter.x() - judgmentRect.width() / 2.0;
                qreal textY = targetDotCenter.y() - judgmentRect.height() / 2.0 - 30; // 向上偏移30像素
                m_judgmentText->setPos(textX, textY);
            }
            m_judgmentTimer->start(500); // 显示 800ms
        }

        // 处理成功或失败
        if (success) {
            // 增加分数并更新显示/速度
            m_score += scoreGained;
            qDebug() << "Score:" << m_score << "(+" << scoreGained << ")";
            updateScoreDisplayAndSpeed(); // 调用更新函数

            switchTrack(); // 切换到下一个轨道
        } else {
            // Miss: 扣血并检查游戏结束
            m_health--;
            qDebug() << "Health:" << m_health;
            updateHealthDisplay(); // 更新 HUD

            if (m_health <= 0) {
                endGame(); // 触发 Game Over
            }
        }
    }
    // 游戏结束后，按下空格键重新开始
    else if (m_gameOver && event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        initializeGame(); // 重新初始化游戏
    }
    // 其他按键事件传递给基类
    else {
        QGraphicsScene::keyPressEvent(event);
    }
}

// Slot to hide the judgment text
void GameScene::hideJudgmentText()
{
    if (m_judgmentText) {
        m_judgmentText->setVisible(false);
    }
}

// Helper function to update the health display text
void GameScene::updateHealthDisplay()
{
    if (m_healthText) {
        m_healthText->setPlainText(QString("Health: %1 / %2").arg(m_health).arg(m_maxHealth));
        // 根据血量改变颜色
        if (m_health > m_maxHealth / 2) {
            m_healthText->setDefaultTextColor(Qt::green);
        } else if (m_health > 1) {
            m_healthText->setDefaultTextColor(Qt::yellow);
        } else {
            m_healthText->setDefaultTextColor(Qt::red);
        }
    }
}

// Helper function to update score display and check for speed up
void GameScene::updateScoreDisplayAndSpeed()
{
    // 1. 更新分数显示
    if (m_scoreText) {
        m_scoreText->setPlainText(QString("Score: %1").arg(m_score));
        // 手动更新一下位置，因为文字长度可能变化导致右上角定位不准
        if (!views().isEmpty()) {
            QGraphicsView* view = views().first();
            QRectF viewRect = view->mapToScene(view->viewport()->geometry()).boundingRect();
            QRectF scoreRect = m_scoreText->boundingRect();
            m_scoreText->setPos(viewRect.topRight() - QPointF(scoreRect.width() + 10, -10));
        }
    }

    // 2. 检查是否达到速度提升阈值
    int targetLevel = m_score / SCORE_THRESHOLD_FOR_SPEEDUP; // 计算目标速度等级

    if (targetLevel > m_speedLevel) { // 如果目标等级高于当前记录的等级
        // 计算新速度: 基础速度 * (加速因子 ^ 目标等级)
        m_linearSpeed = BASE_LINEAR_SPEED * qPow(SPEEDUP_FACTOR, targetLevel);
        m_speedLevel = targetLevel; // 更新当前速度等级

        qDebug() << "SPEED UP! Level:" << m_speedLevel << "New Speed:" << m_linearSpeed;

        // (可选) 在这里添加加速的视觉/声音提示
    }
}


void GameScene::switchTrack() {
    // 确保下一个轨道数据存在，如果列表末尾没有就生成
    if (m_currentTrackIndex + 1 >= m_tracks.size()) {
        qWarning() << "Next track data missing, generating on the fly.";
        TrackData nextNextTrack = generateNextTrack(m_tracks.last());
        m_tracks.append(nextNextTrack);
        addTrackItem(nextNextTrack);
    }

    qDebug() << "Switching to track" << m_currentTrackIndex + 1;

    // --- 更新游戏状态 ---
    m_currentTrackIndex++; // 移动到下一个轨道索引
    m_rotationDirection = -m_rotationDirection; // 反转旋转方向
    qDebug() << "Rotation direction reversed:" << m_rotationDirection;

    // --- 计算球在新轨道上的起始角度 ---
    const TrackData& oldTrack = m_tracks[m_currentTrackIndex - 1]; // 旧轨道
    const TrackData& newTrack = m_tracks[m_currentTrackIndex];   // 新轨道

    // 切点 (理论上球应该精确在这个位置)
    QPointF tangentPoint = oldTrack.center + QPointF(
                               oldTrack.radius * qCos(oldTrack.tangentAngle),
                               oldTrack.radius * qSin(oldTrack.tangentAngle)
                               );

    // 计算切点相对于 *新* 圆心的角度
    qreal dx = tangentPoint.x() - newTrack.center.x();
    qreal dy = tangentPoint.y() - newTrack.center.y();
    m_currentAngle = qAtan2(dy, dx); // 设置球在新轨道上的初始角度

    // 使用新轨道和新角度重新计算精确位置，确保切换平滑
    updateBallPosition();

    // --- 生成更远的轨道，保持轨道池有足够数据 ---
    if (m_tracks.size() <= m_currentTrackIndex + 1) {
        TrackData nextNextTrack = generateNextTrack(newTrack);
        m_tracks.append(nextNextTrack);
        addTrackItem(nextNextTrack);
        qDebug() << "Generated next track during switch.";
    }

    // --- 更新目标点到新轨道的切点 ---
    updateTargetDotPosition();

    // --- 清理旧的视觉轨道项 ---
    cleanupOldTracks();
}

// Removes visual track items that are far behind the current track
void GameScene::cleanupOldTracks() {
    const int lookBehindMargin = 2; // 保留当前和之前的N个视觉轨道

    while (!m_trackItems.isEmpty())
    {
        // 估计 m_trackItems.first() 对应的 track 在 m_tracks 中的索引
        int firstTrackDataIndex = m_tracks.size() - m_trackItems.size();

        // 如果这个索引比当前索引减去边距还要小，说明太旧了
        if (firstTrackDataIndex < m_currentTrackIndex - lookBehindMargin)
        {
            QGraphicsEllipseItem* oldItem = m_trackItems.first(); // 获取最旧的视觉项
            removeItem(oldItem); // 从场景移除
            delete oldItem;      // 释放内存 (因为我们 new 了它)
            m_trackItems.removeFirst(); // 从我们的列表中移除指针
            //qDebug() << "Cleaned up old track visual. Oldest index:" << firstTrackDataIndex << ", Current index:" << m_currentTrackIndex;
        } else {
            // 如果第一个视觉项还不够旧，停止清理
            break;
        }
    }
    // 注意: 这里只清除了视觉项 (QGraphicsEllipseItem),
    // m_tracks 中的数据 (TrackData) 没有被清除，会持续增长。
    // 如果需要，可以添加逻辑来清理 m_tracks 中的旧数据。
}

void GameScene::endGame() {
    if(m_gameOver) return; // 防止重复执行

    m_gameOver = true; // 设置游戏结束标志
    m_timer->stop();   // 停止游戏循环
    m_judgmentTimer->stop(); // 停止判断文本计时器

    // 隐藏游戏元素
    if (m_targetDot) m_targetDot->setVisible(false);
    if (m_ball) m_ball->setVisible(false);
    if (m_healthText) m_healthText->setVisible(false); // 隐藏血条
    if (m_judgmentText) m_judgmentText->setVisible(false); // 隐藏残留的判断文本
    if (m_scoreText) m_scoreText->setVisible(false); // 隐藏分数

    qDebug() << "GAME OVER. Final Score:" << m_score; // 在控制台输出最终分数

    // 显示 Game Over 文本
    if (!m_gameOverText) { // 如果文本对象不存在则创建
        m_gameOverText = new QGraphicsTextItem();
        addItem(m_gameOverText); // 添加到场景
    }
    // 设置 Game Over 文本内容，包含最终分数
    m_gameOverText->setPlainText(QString("GAME OVER\nFinal Score: %1\nPress SPACE to Restart").arg(m_score));
    QFont font("Arial", 24, QFont::Bold); // 设置字体
    m_gameOverText->setFont(font);
    m_gameOverText->setDefaultTextColor(Qt::red); // 设置颜色
    m_gameOverText->setZValue(2); // 确保在最上层

    // 位置在 updateGame 中根据视图中心处理，这里确保它可见
    m_gameOverText->setVisible(true);
    // (可选) 可能需要手动调用 updateGame 一次来正确定位文本，因为计时器已停止
    // updateGame();
}

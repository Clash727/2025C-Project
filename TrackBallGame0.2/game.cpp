#include "game.h"
#include <QDebug>

Game::Game(QObject *parent) : QObject(parent),
    m_ball(Ball(15)),
    m_currentTrackIndex(0),
    m_ballSpeed(0.02),
    m_score(0),
    m_gameOver(false)
{
    // 设置更新计时器
    connect(&m_updateTimer, &QTimer::timeout, this, &Game::update);

    // 初始化游戏
    reset();
}

Game::~Game()
{
    m_updateTimer.stop();
}

void Game::start()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(16);  // 约60 FPS
        m_gameTimer.start();
    }
}

void Game::pause()
{
    m_updateTimer.stop();
}

void Game::resume()
{
    m_updateTimer.start(16);
}

void Game::reset()
{
    m_tracks.clear();
    m_dots.clear();
    m_currentTrackIndex = 0;
    m_ballSpeed = 0.02;
    m_score = 0;
    m_gameOver = false;

    generateInitialTracks();

    emit scoreChanged(m_score);
    emit updated();
}

bool Game::processSpaceKey()
{
    // 检查当前轨道上的球是否与某个点相交
    if (checkCollision()) {
        // 移动到下一个轨道
        if (m_currentTrackIndex < m_tracks.size() - 1) {
            m_currentTrackIndex++;

            // 如果接近最后一个轨道，生成新轨道
            if (m_currentTrackIndex >= m_tracks.size() - 2) {
                generateNextTrack();
            }

            // 增加分数
            m_score++;
            emit scoreChanged(m_score);

            // 每得10分增加难度
            if (m_score % 10 == 0) {
                m_ballSpeed *= 1.1;
            }

            return true;
        }
    } else {
        // 未击中，游戏结束
        m_gameOver = true;
        m_updateTimer.stop();
        emit gameOver();
    }

    return false;
}

QVector<Track> Game::tracks() const
{
    return m_tracks;
}

Ball Game::ball() const
{
    return m_ball;
}

QVector<Dot> Game::dots() const
{
    return m_dots;
}

int Game::currentTrackIndex() const
{
    return m_currentTrackIndex;
}

int Game::score() const
{
    return m_score;
}

bool Game::isGameOver() const
{
    return m_gameOver;
}

void Game::update()
{
    if (m_gameOver) return;

    // 更新球的位置
    Track& currentTrack = m_tracks[m_currentTrackIndex];
    qreal currentAngle = currentTrack.angle();
    currentAngle += m_ballSpeed;
    if (currentAngle > 2 * M_PI) {
        currentAngle -= 2 * M_PI;
    }
    currentTrack.setAngle(currentAngle);

    // 有概率创建新的圆点
    if (QRandomGenerator::global()->generateDouble() < DOT_SPAWN_PROBABILITY) {
        createDot();
    }

    // 删除超出可见范围的轨道和点
    while (m_tracks.size() > INITIAL_TRACKS + 10) {
        if (m_currentTrackIndex > 0) {
            m_tracks.removeFirst();
            m_currentTrackIndex--;
        }
    }

    // 清理不再需要的点
    for (int i = m_dots.size() - 1; i >= 0; i--) {
        bool dotOnRemovedTrack = true;
        for (const Track& track : m_tracks) {
            QPointF tangentPoint;
            if (m_currentTrackIndex > 0) {
                tangentPoint = m_tracks[m_currentTrackIndex - 1].tangentPoint(track);
            } else if (m_currentTrackIndex < m_tracks.size() - 1) {
                tangentPoint = track.tangentPoint(m_tracks[m_currentTrackIndex + 1]);
            }

            if (QLineF(m_dots[i].position(), tangentPoint).length() < 2.0) {
                dotOnRemovedTrack = false;
                break;
            }
        }

        if (dotOnRemovedTrack) {
            m_dots.removeAt(i);
        }
    }

    emit updated();
}

void Game::generateInitialTracks()
{
    // 创建起始圆轨道
    QPointF center(400, 300);  // 假定游戏区域中心
    qreal radius = QRandomGenerator::global()->bounded(MIN_RADIUS, MAX_RADIUS);
    m_tracks.append(Track(center, radius));

    // 创建初始轨道
    for (int i = 0; i < INITIAL_TRACKS - 1; i++) {
        generateNextTrack();
    }
}

void Game::generateNextTrack()
{
    if (m_tracks.isEmpty()) return;

    Track& lastTrack = m_tracks.last();

    // 决定新轨道的方向（上/下/左/右）
    int direction = QRandomGenerator::global()->bounded(4); // 0:右, 1:下, 2:左, 3:上
    qreal angle = direction * M_PI / 2.0;

    // 计算新轨道的半径
    qreal newRadius = QRandomGenerator::global()->bounded(MIN_RADIUS, MAX_RADIUS);

    // 计算新轨道的中心位置（使轨道相切）
    qreal centerDist = lastTrack.radius() + newRadius;
    QPointF newCenter = lastTrack.center() + QPointF(
                            centerDist * qCos(angle),
                            centerDist * qSin(angle)
                            );

    // 添加新轨道
    m_tracks.append(Track(newCenter, newRadius));
}

void Game::createDot()
{
    if (m_tracks.size() < 2) return;

    // 随机选择两个相邻轨道
    int trackIndex = QRandomGenerator::global()->bounded(m_tracks.size() - 1);

    // 计算相切点
    QPointF tangentPoint = m_tracks[trackIndex].tangentPoint(m_tracks[trackIndex + 1]);

    // 创建圆点
    m_dots.append(Dot(tangentPoint));
}

bool Game::checkCollision()
{
    if (m_dots.isEmpty()) return false;

    // 获取当前球的位置
    QPointF ballPos = m_tracks[m_currentTrackIndex].ballPosition();

    // 检查是否与任何点碰撞
    for (int i = 0; i < m_dots.size(); ++i) {
        QLineF line(ballPos, m_dots[i].position());
        if (line.length() <= m_ball.radius() + m_dots[i].radius()) {
            m_dots.removeAt(i);
            return true;
        }
    }

    return false;
}

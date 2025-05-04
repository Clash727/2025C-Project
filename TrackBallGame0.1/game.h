#ifndef GAME_H
#define GAME_H

#include <QObject>
#include <QVector>
#include <QTimer>
#include <QElapsedTimer>
#include <QRandomGenerator>

class Game : public QObject
{
    Q_OBJECT
public:
    explicit Game(QObject *parent = nullptr);
    ~Game();

    void start();
    void pause();
    void resume();
    void reset();

    bool processSpaceKey();

    QVector<Track> tracks() const;
    Ball ball() const;
    QVector<Dot> dots() const;
    int currentTrackIndex() const;
    int score() const;
    bool isGameOver() const;

signals:
    void updated();
    void gameOver();
    void scoreChanged(int score);

private slots:
    void update();

private:
    void generateInitialTracks();
    void generateNextTrack();
    void createDot();
    bool checkCollision();

    QVector<Track> m_tracks;
    QVector<Dot> m_dots;
    Ball m_ball;

    QTimer m_updateTimer;
    QElapsedTimer m_gameTimer;

    int m_currentTrackIndex;
    qreal m_ballSpeed;
    int m_score;
    bool m_gameOver;

    static constexpr int INITIAL_TRACKS = 3;
    static constexpr qreal MIN_RADIUS = 40.0;
    static constexpr qreal MAX_RADIUS = 120.0;
    static constexpr qreal DOT_SPAWN_PROBABILITY = 0.02;
    static constexpr qreal BALL_SPEED_INCREMENT = 0.0001;
};

#endif // GAME_H

#include "gameview.h"

GameView::GameView(QWidget *parent)
    : QWidget(parent)
{
    // 设置窗口属性
    setFixedSize(800, 600);
    setWindowTitle("轨道球游戏");

    // 创建游戏实例
    m_game = new Game(this);
    connect(m_game, &Game::updated, this, &GameView::updateView);
    connect(m_game, &Game::gameOver, this, &GameView::handleGameOver);

    // 创建UI元素
    m_scoreLabel = new QLabel("分数: 0", this);
    m_scoreLabel->setGeometry(10, 10, 100, 30);

    m_gameOverLabel = new QLabel("游戏结束!", this);
    m_gameOverLabel->setAlignment(Qt::AlignCenter);
    m_gameOverLabel->setStyleSheet("font-size: 24px; color: red;");
    m_gameOverLabel->setGeometry(300, 250, 200, 50);
    m_gameOverLabel->hide();

    m_restartButton = new QPushButton("重新开始", this);
    m_restartButton->setGeometry(350, 310, 100, 30);
    m_restartButton->hide();
    connect(m_restartButton, &QPushButton::clicked, this, &GameView::restartGame);

    // 连接分数变化信号
    connect(m_game, &Game::scoreChanged, [this](int score) {
        m_scoreLabel->setText(QString("分数: %1").arg(score));
    });

    // 开始游戏
    m_game->start();
    setFocus();
}

GameView::~GameView()
{
    delete m_game;
}

void GameView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景
    painter.fillRect(rect(), Qt::black);

    // 绘制所有轨道
    painter.setPen(QPen(Qt::white, 2));
    for (const Track& track : m_game->tracks()) {
        painter.drawEllipse(track.center(), track.radius(), track.radius());
    }

    // 绘制所有点
    painter.setBrush(Qt::red);
    painter.setPen(Qt::NoPen);
    for (const Dot& dot : m_game->dots()) {
        painter.drawEllipse(dot.position(), dot.radius(), dot.radius());
    }

    // 绘制球
    painter.setBrush(Qt::white);
    const Track& currentTrack = m_game->tracks()[m_game->currentTrackIndex()];
    QPointF ballPos = currentTrack.ballPosition();
    painter.drawEllipse(ballPos, m_game->ball().radius(), m_game->ball().radius());
}

void GameView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !m_game->isGameOver()) {
        m_game->processSpaceKey();
    }
}

void GameView::updateView()
{
    update();  // 刷新视图
}

void GameView::handleGameOver()
{
    m_gameOverLabel->show();
    m_restartButton->show();
}

void GameView::restartGame()
{
    m_game->reset();
    m_game->start();
    m_gameOverLabel->hide();
    m_restartButton->hide();
    setFocus();
}

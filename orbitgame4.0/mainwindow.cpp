#include "mainwindow.h"
#include "gamescene.h" // 确保 GameScene 的定义可见
#include "startscene.h"
#include <QGraphicsView>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QStackedWidget>
#include <QUrl>
#include <QDebug>
#include <QResizeEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(nullptr)
    , m_graphicsView(nullptr)
    , m_gameScene(nullptr)
    , m_startScene(nullptr)
    , m_mediaPlayer(nullptr)
    , m_videoWidget(nullptr)
    , m_mainStackedWidget(nullptr)
    , m_currentGameState(GameState::ShowingStartScreen)
{
    setupCustomUiElements();
    showStartScreen();
}

MainWindow::~MainWindow()
{
    cleanupMediaPlayer();
}

void MainWindow::setupCustomUiElements()
{
    m_mainStackedWidget = new QStackedWidget(this);
    setCentralWidget(m_mainStackedWidget);

    m_graphicsView = new QGraphicsView(this);
    m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mainStackedWidget->addWidget(m_graphicsView);

    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setStyleSheet("background-color: black;");
    m_mainStackedWidget->addWidget(m_videoWidget);

    m_startScene = new StartScene(this);
    m_gameScene = new GameScene(this); // GameScene 实例在这里创建

    // 初始化 QMediaPlayer
    m_mediaPlayer = new QMediaPlayer(this);
    m_mediaPlayer->setVideoOutput(m_videoWidget); // 只需要设置一次 Video Output
    connect(m_mediaPlayer, QOverload<QMediaPlayer::Error, const QString &>::of(&QMediaPlayer::errorOccurred),
            this, [this](QMediaPlayer::Error error, const QString &errorString){
                qWarning() << "Video Player Error (Current State: " << static_cast<int>(m_currentGameState) << "):" << error << errorString;
                cleanupMediaPlayer(); // 清理播放器状态
                // 根据当前状态决定下一步操作
                if (m_currentGameState == GameState::PlayingIntroVideo) {
                    startGameplay();
                } else if (m_currentGameState == GameState::PlayingEndVideo) {
                    showStartScreen();
                } else {
                    showStartScreen(); // 默认返回开始界面
                }
            });


    connect(m_startScene, &StartScene::startGameClicked, this, &MainWindow::handleStartGameClicked);
    connect(m_startScene, &StartScene::tutorialClicked, this, &MainWindow::handleTutorialClicked);

    qDebug() << "MainWindow::setupCustomUiElements - m_gameScene pointer:" << m_gameScene;
    if (m_gameScene) {
        bool connReturn = connect(m_gameScene, &GameScene::returnToStartScreenRequested,
                                  this, &MainWindow::handleReturnToStartScreen);
        qDebug() << "Connection [GameScene::returnToStartScreenRequested -> MainWindow::handleReturnToStartScreen] successful:" << connReturn;

        bool connEndVideo = connect(m_gameScene, &GameScene::endGameVideoRequested,
                                    this, &MainWindow::handleEndGameVideoRequestedProcessing);
        qDebug() << "Connection [GameScene::endGameVideoRequested -> MainWindow::handleEndGameVideoRequestedProcessing] successful:" << connEndVideo;
    } else {
        qWarning() << "MainWindow::setupCustomUiElements - m_gameScene is null, cannot connect signals!";
    }

    resize(800,600);
}


void MainWindow::showStartScreen()
{
    qDebug() << "MainWindow::showStartScreen() CALLED.";
    setCurrentGameState(GameState::ShowingStartScreen);

    if (m_mediaPlayer && m_mediaPlayer->playbackState() != QMediaPlayer::StoppedState) {
        qDebug() << "MainWindow::showStartScreen - Stopping media player.";
        m_mediaPlayer->stop(); // 确保在切换场景前停止任何视频播放
    }

    // 停止 GameScene 的背景音乐（如果它仍在播放）
   /* if (m_gameScene && m_gameScene->getBackgroundMusicPlayer() &&
        m_gameScene->getBackgroundMusicPlayer()->playbackState() == QMediaPlayer::PlayingState) {
        qDebug() << "MainWindow::showStartScreen - Stopping GameScene background music.";
        m_gameScene->getBackgroundMusicPlayer()->stop();
    }*/

    if (m_startScene && m_graphicsView && m_mainStackedWidget) {
        m_startScene->setupUi(m_graphicsView->size());
        m_graphicsView->setScene(m_startScene);
        m_mainStackedWidget->setCurrentWidget(m_graphicsView);
        m_graphicsView->setFocus();
        qDebug() << "MainWindow: Switched to m_graphicsView with StartScene.";
    } else {
        qWarning() << "MainWindow::showStartScreen() - One or more UI elements are null!";
    }
}

void MainWindow::handleStartGameClicked()
{
    qDebug() << "MainWindow::handleStartGameClicked() CALLED.";
    playIntroVideo();
}

void MainWindow::handleTutorialClicked()
{
    qDebug() << "MainWindow::handleTutorialClicked()";
    showTutorialScreen();
}

void MainWindow::playIntroVideo()
{
    qDebug() << "MainWindow::playIntroVideo() CALLED.";
    setCurrentGameState(GameState::PlayingIntroVideo);

    // 断开可能存在的旧连接，连接到开场视频的状态处理槽
    disconnect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, nullptr, nullptr);
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &MainWindow::onIntroVideoStateChanged);

    m_mediaPlayer->setSource(QUrl("qrc:/videos/intro_video.mp4"));

    if (m_mediaPlayer->source().isEmpty() || !m_mediaPlayer->source().isValid()) {
        qWarning() << "Intro video source is invalid or empty:" << m_mediaPlayer->source().toString();
        startGameplay(); // 直接开始游戏
        return;
    }
    if (m_mainStackedWidget && m_videoWidget) {
        m_mainStackedWidget->setCurrentWidget(m_videoWidget);
        m_mediaPlayer->play();
        qDebug() << "Attempting to play intro video.";
    } else {
        qWarning() << "MainWindow::playIntroVideo - m_mainStackedWidget or m_videoWidget is null!";
        startGameplay(); // 无法播放，直接开始游戏
    }
}

void MainWindow::onIntroVideoStateChanged(QMediaPlayer::PlaybackState state)
{
    if (m_currentGameState != GameState::PlayingIntroVideo) {
        qDebug() << "onIntroVideoStateChanged received state" << state << "but current game state is" << static_cast<int>(m_currentGameState);
        return;
    }
    qDebug() << "MainWindow::onIntroVideoStateChanged - New state:" << state;
    if (state == QMediaPlayer::StoppedState) {
        qDebug() << "Intro video playback stopped. Starting gameplay.";
        startGameplay();
    }
}

void MainWindow::startGameplay()
{
    qDebug() << "MainWindow::startGameplay() CALLED.";
    setCurrentGameState(GameState::PlayingGame);

    if (m_gameScene && m_graphicsView) {
        m_gameScene->initializeGame();
        m_graphicsView->setScene(m_gameScene);
    } else {
        qWarning() << "MainWindow::startGameplay() - m_gameScene or m_graphicsView is null!";
    }

    if (m_mainStackedWidget && m_graphicsView) {
        m_mainStackedWidget->setCurrentWidget(m_graphicsView);
    }

    if (m_graphicsView) {
        m_graphicsView->setFocus();
    }
    qDebug() << "Gameplay setup complete. Switched to GameScene.";
}

void MainWindow::handleEndGameVideoRequestedProcessing()
{
    qDebug() << "MainWindow::handleEndGameVideoRequestedProcessing() CALLED.";
    playEndVideo();
}

void MainWindow::playEndVideo()
{
    qDebug() << "MainWindow::playEndVideo() CALLED.";
    setCurrentGameState(GameState::PlayingEndVideo);

    // 断开可能存在的旧连接，连接到结束视频的状态处理槽
    disconnect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, nullptr, nullptr);
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &MainWindow::onEndVideoStateChanged);

    m_mediaPlayer->setSource(QUrl("qrc:/videos/end_video.mp4")); // 结束视频的路径

    if (m_mediaPlayer->source().isEmpty() || !m_mediaPlayer->source().isValid()) {
        qWarning() << "End video source is invalid or empty:" << m_mediaPlayer->source().toString();
        showStartScreen(); // 视频源有问题，直接返回主菜单
        return;
    }

    if (m_mainStackedWidget && m_videoWidget) {
        m_mainStackedWidget->setCurrentWidget(m_videoWidget);
        m_mediaPlayer->play();
        qDebug() << "Attempting to play end video...";
    } else {
        qWarning() << "MainWindow::playEndVideo - m_mainStackedWidget or m_videoWidget is null!";
        showStartScreen(); // 无法播放视频，直接返回
    }
}

void MainWindow::onEndVideoStateChanged(QMediaPlayer::PlaybackState state)
{
    if (m_currentGameState != GameState::PlayingEndVideo) {
        qDebug() << "onEndVideoStateChanged received state" << state << "but current game state is" << static_cast<int>(m_currentGameState);
        return;
    }
    qDebug() << "MainWindow::onEndVideoStateChanged - New state:" << state;
    if (state == QMediaPlayer::StoppedState) {
        qDebug() << "End video playback stopped. Returning to StartScreen.";
        showStartScreen();    // 返回主界面
    }
}

void MainWindow::showTutorialScreen()
{
    setCurrentGameState(GameState::ShowingTutorial);
    qDebug() << "MainWindow::showTutorialScreen() - To be implemented if it's a separate scene/state.";
    // 例如:
    // if (m_tutorialScene && m_graphicsView && m_mainStackedWidget) {
    //     m_graphicsView->setScene(m_tutorialScene);
    //     m_mainStackedWidget->setCurrentWidget(m_graphicsView);
    // }
}

void MainWindow::cleanupMediaPlayer()
{
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
        // m_mediaPlayer->setVideoOutput(nullptr); // 保持 video output 连接，除非 mediaPlayer 被销毁
        // 如果 mediaPlayer 会被频繁销毁和重建，那么在这里 setVideoOutput(nullptr) 是好的。
        // 如果 mediaPlayer 是一个长期存在的对象，则不需要每次都断开。
        // 当前设计是重用 m_mediaPlayer。
    }
    qDebug() << "MainWindow::cleanupMediaPlayer() - Media player stopped.";
}

void MainWindow::setCurrentGameState(GameState newState)
{
    if (m_currentGameState == newState) return;
    qDebug() << "MainWindow::setCurrentGameState - Changed from" << static_cast<int>(m_currentGameState) << "to" << static_cast<int>(newState);
    m_currentGameState = newState;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_graphicsView && m_graphicsView->scene()) {
        m_graphicsView->scene()->setSceneRect(0, 0, m_graphicsView->width(), m_graphicsView->height());
        if (m_startScene && m_currentGameState == GameState::ShowingStartScreen) {
            m_startScene->setupUi(m_graphicsView->size());
        }
    }
}

void MainWindow::handleReturnToStartScreen()
{
    qDebug() << "MainWindow::handleReturnToStartScreen() CALLED. Calling showStartScreen().";
    showStartScreen();
}

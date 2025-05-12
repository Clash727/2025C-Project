#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>

class QGraphicsView;
class GameScene;
class StartScene;
class QVideoWidget;
class QStackedWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void handleStartGameClicked();
    void handleTutorialClicked();
    void onIntroVideoStateChanged(QMediaPlayer::PlaybackState state); // <--- 改名以区分
    void onEndVideoStateChanged(QMediaPlayer::PlaybackState state);   // <--- 改名以区分
    void handleReturnToStartScreen();
    void handleEndGameVideoRequestedProcessing(); // <--- 改名以强调是处理来自GameScene的请求

private:
    enum class GameState {
        ShowingStartScreen,
        PlayingIntroVideo,
        ShowingTutorial,
        PlayingGame,
        PlayingEndVideo
    };

    void setCurrentGameState(GameState newState);
    void setupCustomUiElements();
    void showStartScreen();
    void playIntroVideo();
    void playEndVideo(); // <--- 新增一个专门播放结束视频的函数
    void startGameplay();
    void cleanupMediaPlayer(); // <--- 改名，更通用
    void showTutorialScreen();
    Ui::MainWindow *ui;
    QGraphicsView *m_graphicsView;
    GameScene *m_gameScene;
    StartScene *m_startScene;
    QMediaPlayer *m_mediaPlayer;
    QVideoWidget *m_videoWidget;
    QStackedWidget *m_mainStackedWidget;
    GameState m_currentGameState;
};
#endif // MAINWINDOW_H

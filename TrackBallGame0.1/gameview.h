#ifndef GAMEVIEW_H
#define GAMEVIEW_H

#include <QWidget>
#include <QPainter>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include "game.h"

class GameView : public QWidget
{
    Q_OBJECT

public:
    explicit GameView(QWidget *parent = nullptr);
    ~GameView();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateView();
    void handleGameOver();
    void restartGame();

private:
    Game *m_game;
    QLabel *m_scoreLabel;
    QLabel *m_gameOverLabel;
    QPushButton *m_restartButton;
    QVBoxLayout *m_layout;
};

#endif // GAMEVIEW_H

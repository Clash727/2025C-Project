#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView> // Include QGraphicsView
#include "Gamescene.h"   // Include GameScene

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    GameScene *m_gameScene = nullptr; // Pointer to our custom scene
    QGraphicsView *m_gameView = nullptr; // Pointer to the view widget
};
#endif // MAINWINDOW_H

#include "mainwindow.h"
#include "./ui_mainwindow.h" // Make sure this matches your UI file name

#include <QGraphicsView> // Include again here if not precompiled
#include <QVBoxLayout>  // For layout management

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // --- Game Setup ---
    // 1. Create the Scene
    m_gameScene = new GameScene(this); // Parent scene to window for lifetime management

    // 2. Create the View
    m_gameView = new QGraphicsView(this); // Create a QGraphicsView widget
    m_gameView->setScene(m_gameScene);   // Set the scene for the view

    // 3. View Optimization/Appearance
    m_gameView->setRenderHint(QPainter::Antialiasing); // Smoother graphics
    m_gameView->setCacheMode(QGraphicsView::CacheBackground); // Cache background drawing
    m_gameView->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate); // Optimize updates
    m_gameView->setDragMode(QGraphicsView::NoDrag); // Disable mouse dragging of the scene
    m_gameView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Disable scroll bars
    m_gameView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // --- Layout ---
    // Make the view the central widget (or add it to a layout)
    setCentralWidget(m_gameView); // Make the view fill the main window

    // Optional: Set a fixed or minimum size for the window
    setMinimumSize(800, 600); // Example size
    setWindowTitle("Orbit Switch Game");

    // Ensure the view gets focus to receive key events
    m_gameView->setFocus();
}

MainWindow::~MainWindow()
{
    delete ui;
    // m_gameScene TBD: deleted by QObject hierarchy if parented? Check Qt docs. Usually yes.
    // m_gameView likewise.
}

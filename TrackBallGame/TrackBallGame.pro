QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TrackBallGame
TEMPLATE = app

SOURCES += \
    main.cpp \
    game.cpp \
    gameview.cpp

HEADERS += \
    game.h \
    gameview.h \
    gameobjects.h

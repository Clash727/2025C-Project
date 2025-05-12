#ifndef GAMEOVERDISPLAY_H
#define GAMEOVERDISPLAY_H

#include <QGraphicsObject> // 使用 QGraphicsObject 以便支持信号和槽
#include <QGraphicsTextItem>
#include <QString>

class GameOverDisplay : public QGraphicsObject
{
    Q_OBJECT

public:
    // 构造函数接收字体名称，用于正确显示文本
    explicit GameOverDisplay(const QString& chineseFont, const QString& englishFont, QGraphicsItem *parent = nullptr);
    ~GameOverDisplay();

    // QGraphicsItem 接口，用于确定边界和绘制（如果需要自定义绘制）
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    // 显示游戏结束界面
    void showScreen(int score, const QPointF& viewCenter);
    // 隐藏游戏结束界面
    void hideScreen();

signals:
    void restartGameRequested();        // 请求重新开始游戏
    void returnToMainMenuRequested();   // 请求返回主菜单

protected:
    // 处理按键事件
    void keyPressEvent(QKeyEvent *event) override;

private:
    QGraphicsTextItem *m_displayText;   // 用于显示所有文本内容
    QString m_chineseFontFamily;
    QString m_englishFontFamily;
    bool m_isActive; // 标记此界面是否活动并应处理输入
};

#endif // GAMEOVERDISPLAY_H

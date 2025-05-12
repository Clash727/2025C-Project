#include "gameoverdisplay.h"
#include <QKeyEvent>
#include <QPainter>
#include <QDebug>           // 确保包含 QDebug
#include <QGraphicsScene>   // 确保包含 QGraphicsScene (为了 scene()->focusItem())

GameOverDisplay::GameOverDisplay(const QString& chineseFont, const QString& englishFont, QGraphicsItem *parent)
    : QGraphicsObject(parent),
    m_displayText(new QGraphicsTextItem(this)), // m_displayText 是 GameOverDisplay 的子项
    m_chineseFontFamily(chineseFont),
    m_englishFontFamily(englishFont),
    m_isActive(false)
{
    setZValue(5.0); // 确保游戏结束界面在其他元素之上
    setVisible(false); // 初始时隐藏
    setFlag(QGraphicsItem::ItemIsFocusable); // 使其能够接收键盘焦点和事件
}

GameOverDisplay::~GameOverDisplay()
{
    // m_displayText 作为子项，Qt会自动处理其销毁
}

QRectF GameOverDisplay::boundingRect() const
{
    if (m_displayText) {
        return m_displayText->boundingRect();
    }
    return QRectF();
}

void GameOverDisplay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(painter);
    Q_UNUSED(option);
    Q_UNUSED(widget);
    // 如果HTML中的div背景不满足需求，可以在此绘制自定义背景
}

void GameOverDisplay::showScreen(int score, const QPointF& viewCenter)
{
    QString gameOverHtml = QString(
                               "<div style='background-color: rgba(30, 30, 30, 0.85); padding: 30px; border-radius: 15px; text-align: center; min-width: 400px;'>"
                               "<span style=\"font-family: '%1'; font-size: 32pt; font-weight: bold; color: white;\">本轮得分</span><br/>"
                               "<span style=\"font-family: '%2'; font-size: 64pt; font-weight: bold; color: yellow;\">%3</span><br/><br/><br/>"
                               "<span style=\"font-family: '%1'; font-size: 24pt; color: lightcyan;\">按【空格键】重新开始</span><br/>"
                               "<span style=\"font-family: '%1'; font-size: 24pt; color: lightcyan;\">按【R键】返回主菜单</span>"
                               "</div>")
                               .arg(m_chineseFontFamily)
                               .arg(m_englishFontFamily)
                               .arg(score);

    m_displayText->setHtml(gameOverHtml);

    QRectF textBoundingRect = m_displayText->boundingRect();
    setPos(viewCenter.x() - textBoundingRect.width() / 2.0,
           viewCenter.y() - textBoundingRect.height() / 2.0);

    setVisible(true);
    m_isActive = true;      // 确保 isActive 为 true
    setFocus();             // 尝试设置焦点
    qDebug() << "GameOverDisplay::showScreen - Score:" << score << "Attempting to set focus. Has focus:" << hasFocus() << "Is active:" << m_isActive << "Is visible:" << isVisible(); // 调试焦点状态
}

void GameOverDisplay::hideScreen()
{
    setVisible(false);
    m_isActive = false;
    // 如果此项当前拥有场景焦点，则清除它
    if (hasFocus() && scene()) { // 先检查 scene() 是否有效
        if (scene()->focusItem() == this) {
            clearFocus();
        }
    }
    qDebug() << "GameOverDisplay hidden.";
}

void GameOverDisplay::keyPressEvent(QKeyEvent *event)
{
    if (!m_isActive || !isVisible()) { // 如果界面不活动或不可见，不处理按键
        qDebug() << "GameOverDisplay::keyPressEvent - Inactive or not visible, passing event.";
        QGraphicsObject::keyPressEvent(event); // 调用基类实现
        return;
    }

    qDebug() << "GameOverDisplay::keyPressEvent received key:" << event->key() << "(R is" << Qt::Key_R << ", Space is" << Qt::Key_Space << ")"; // 调试接收到的按键

    if (hasFocus()) { // 调试是否拥有焦点
        qDebug() << "GameOverDisplay has focus.";
    } else {
        qDebug() << "GameOverDisplay DOES NOT have focus. Event might not be processed correctly by this item.";
    }

    if (event->key() == Qt::Key_R && !event->isAutoRepeat()) {
        qDebug() << "R key pressed in GameOverDisplay. Emitting returnToMainMenuRequested signal."; // 调试信号发射
        hideScreen();
        emit returnToMainMenuRequested();
        event->accept();
    } else if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        qDebug() << "Space key pressed in GameOverDisplay. Emitting restartGameRequested signal."; // 调试信号发射
        hideScreen();
        emit restartGameRequested();
        event->accept();
    } else {
        QGraphicsObject::keyPressEvent(event); // 其他键交给基类处理
    }
}

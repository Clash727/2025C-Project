#ifndef STARTSCENE_H
#define STARTSCENE_H

#include <QGraphicsScene>
#include <QString> // <--- 添加 QString 头文件

class QGraphicsPixmapItem;
class CustomClickableItem;
class QGraphicsRectItem;
class QGraphicsTextItem;
class QFontDatabase; // 前向声明或包含完整头文件，这里选择不包含以保持头文件简洁

class StartScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit StartScene(QObject *parent = nullptr);
    ~StartScene();

    void setupUi(const QSize& viewSize); // 根据视图大小调整布局

signals:
    void startGameClicked();
    void tutorialClicked();

private:
    void loadCustomFonts(); // <--- 新增：加载自定义字体的辅助方法
    void showTutorialContent(bool show, const QSize& viewSize);

    QGraphicsPixmapItem *m_backgroundItem;
    QGraphicsPixmapItem *m_titleItem;
    CustomClickableItem *m_startButton;
    CustomClickableItem *m_tutorialButton;

    // 教程相关的元素
    QGraphicsRectItem* m_tutorialPanel;
    QGraphicsTextItem* m_tutorialText;
    CustomClickableItem* m_tutorialCloseButton;
    bool m_isTutorialVisible;

    // 自定义字体家族名称
    QString m_chineseFontFamily; // <--- 新增：存储中文字体家族名称
    QString m_englishFontFamily; // <--- 新增：存储英文字体家族名称
};

#endif // STARTSCENE_H

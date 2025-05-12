// 文件: startscene.cpp
#include "startscene.h"
#include "customclickableitem.h"
#include <QPixmap>
#include <QGraphicsView>
#include <QDebug>
#include <QApplication>
#include <QFontDatabase> // <--- 添加 QFontDatabase 头文件

// --- 定义期望的按钮尺寸 ---
const qreal DESIRED_BUTTON_WIDTH = 1000.0;
const qreal DESIRED_BUTTON_HEIGHT = 250.0;
const qreal DESIRED_CLOSE_BUTTON_SIZE = 200.0;

// --- 定义元素间的垂直间距 (使用固定像素值) ---
const qreal TOP_MARGIN_TITLE_PIXELS = 300.0;
const qreal TITLE_TO_START_BUTTON_SPACING_PIXELS = 250.0;
const qreal BUTTON_SPACING_PIXELS = -150.0;

// --- 备用Y位置因子 ---
const qreal FALLBACK_Y_FACTOR_START_BUTTON = 0.40;
const qreal FALLBACK_Y_FACTOR_TUTORIAL_BUTTON_NO_TITLE_NO_START = 0.60;

// --- 教程面板内元素间距 ---
const qreal TEXT_TO_CLOSE_BUTTON_SPACING = 20.0;
const qreal CLOSE_BUTTON_BOTTOM_MARGIN = 20.0;


StartScene::StartScene(QObject *parent) : QGraphicsScene(parent),
    m_backgroundItem(nullptr),
    m_titleItem(nullptr),
    m_startButton(nullptr),
    m_tutorialButton(nullptr),
    m_tutorialPanel(nullptr),
    m_tutorialText(nullptr),
    m_tutorialCloseButton(nullptr),
    m_isTutorialVisible(false),
    m_chineseFontFamily("SimSun"), // <--- 初始化为默认中文字体
    m_englishFontFamily("Arial")   // <--- 初始化为默认英文字体
{
    loadCustomFonts(); // <--- 在构造函数中调用加载字体的方法
}

StartScene::~StartScene()
{
}

void StartScene::loadCustomFonts()
{
    // 假设您的自定义字体文件在资源路径 :/fonts/ 下
    // 请将 "YourChineseFontFile.ttf" 和 "YourEnglishFontFile.ttf" 替换为您的实际文件名
    int chineseFontId = QFontDatabase::addApplicationFont(":/fonts/MyChineseFont.ttf"); // 与 GameScene 保持一致
    if (chineseFontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(chineseFontId);
        if (!fontFamilies.isEmpty()) {
            m_chineseFontFamily = fontFamilies.at(0);
            qDebug() << "StartScene: Custom Chinese font loaded:" << m_chineseFontFamily;
        } else {
            qWarning() << "StartScene: Failed to retrieve font family name for Chinese font. Using default:" << m_chineseFontFamily;
        }
    } else {
        qWarning() << "StartScene: Failed to load custom Chinese font from ':/fonts/MyChineseFont.ttf'. Using default:" << m_chineseFontFamily;
    }

    int englishFontId = QFontDatabase::addApplicationFont(":/fonts/MyEnglishFont.ttf"); // 与 GameScene 保持一致
    if (englishFontId != -1) {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(englishFontId);
        if (!fontFamilies.isEmpty()) {
            m_englishFontFamily = fontFamilies.at(0);
            qDebug() << "StartScene: Custom English font loaded:" << m_englishFontFamily;
        } else {
            qWarning() << "StartScene: Failed to retrieve font family name for English font. Using default:" << m_englishFontFamily;
        }
    } else {
        qWarning() << "StartScene: Failed to load custom English font from ':/fonts/MyEnglishFont.ttf'. Using default:" << m_englishFontFamily;
    }
}

void StartScene::setupUi(const QSize& viewSize)
{
    qDebug() << "StartScene: setupUi called with viewSize:" << viewSize;
    qDebug() << "StartScene: Using fixed pixel spacing. TITLE_TO_START_BUTTON_SPACING_PIXELS =" << TITLE_TO_START_BUTTON_SPACING_PIXELS
             << "BUTTON_SPACING_PIXELS =" << BUTTON_SPACING_PIXELS;

    // 1. 背景设置
    const QString bgPath = ":/images/game_background.png";
    QPixmap bgPixmap(bgPath);
    if (bgPixmap.isNull()) {
        qWarning() << "StartScene: Failed to load background image '" << bgPath << "'. Using fallback darkGray brush.";
        setBackgroundBrush(Qt::darkGray);
    } else {
        setBackgroundBrush(QBrush(bgPixmap.scaled(viewSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)));
        qDebug() << "StartScene: Background image '" << bgPath << "' loaded and set.";
    }
    setSceneRect(0, 0, viewSize.width(), viewSize.height());


    // 2. 游戏标题
    const QString titlePath = ":/images/game_title.png";
    QPixmap titlePixmapOriginal(titlePath);
    qDebug() << "StartScene: Attempting to load title image '" << titlePath << "'. IsNull:" << titlePixmapOriginal.isNull() << "Size:" << titlePixmapOriginal.size();

    if (!titlePixmapOriginal.isNull()) {
        if (!m_titleItem) {
            m_titleItem = new QGraphicsPixmapItem();
            addItem(m_titleItem);
        }
        m_titleItem->setPixmap(titlePixmapOriginal);
        m_titleItem->setPos((viewSize.width() - m_titleItem->boundingRect().width()) / 2,
                            TOP_MARGIN_TITLE_PIXELS);
        m_titleItem->setVisible(true);
        qDebug() << "StartScene: Title item created/updated. Pos:" << m_titleItem->pos() << "Size:" << m_titleItem->boundingRect().size() << "Visible:" << m_titleItem->isVisible();
    } else {
        qWarning() << "StartScene: Failed to load title image '" << titlePath << "'. Title will not be shown.";
        if (m_titleItem) {
            m_titleItem->setVisible(false);
        }
    }

    // 3. 开始按钮
    const QString startButtonPath = ":/images/start_button.png";
    QPixmap startButtonPixmapOriginal(startButtonPath);
    qDebug() << "StartScene: Attempting to load start button image '" << startButtonPath << "'. IsNull:" << startButtonPixmapOriginal.isNull() << "Size:" << startButtonPixmapOriginal.size();

    if (!startButtonPixmapOriginal.isNull()) {
        QPixmap startButtonPixmapScaled = startButtonPixmapOriginal.scaled(
            DESIRED_BUTTON_WIDTH, DESIRED_BUTTON_HEIGHT,
            Qt::KeepAspectRatio, Qt::SmoothTransformation
            );
        if (!m_startButton) {
            m_startButton = new CustomClickableItem(startButtonPixmapScaled);
            addItem(m_startButton);
            connect(m_startButton, &CustomClickableItem::clicked, this, &StartScene::startGameClicked);
        } else {
            m_startButton->setPixmap(startButtonPixmapScaled);
        }

        qreal startButtonX = (viewSize.width() - m_startButton->boundingRect().width()) / 2;
        qreal startButtonY;
        if (m_titleItem && m_titleItem->isVisible() && !m_titleItem->pixmap().isNull()) {
            startButtonY = m_titleItem->y() + m_titleItem->boundingRect().height() + TITLE_TO_START_BUTTON_SPACING_PIXELS;
        } else {
            startButtonY = viewSize.height() * FALLBACK_Y_FACTOR_START_BUTTON;
            qWarning() << "StartScene: Title item is not available for start button positioning, using fallback Y for start button.";
        }
        m_startButton->setPos(startButtonX, startButtonY);
        m_startButton->setVisible(true);
        qDebug() << "StartScene: Start button created/updated. Pos:" << m_startButton->pos() << "Size:" << m_startButton->boundingRect().size() << "Visible:" << m_startButton->isVisible() << "Enabled:" << m_startButton->isEnabled();
    } else {
        qWarning() << "StartScene: Failed to load start button image '" << startButtonPath << "'. Start button will not be shown.";
        if (m_startButton) {
            m_startButton->setVisible(false);
        }
    }

    // 4. 教程按钮
    const QString tutorialButtonPath = ":/images/tutorial_button.png";
    QPixmap tutorialButtonPixmapOriginal(tutorialButtonPath);
    qDebug() << "StartScene: Attempting to load tutorial button image '" << tutorialButtonPath << "'. IsNull:" << tutorialButtonPixmapOriginal.isNull() << "Size:" << tutorialButtonPixmapOriginal.size();

    if (!tutorialButtonPixmapOriginal.isNull()) {
        QPixmap tutorialButtonPixmapScaled = tutorialButtonPixmapOriginal.scaled(
            DESIRED_BUTTON_WIDTH, DESIRED_BUTTON_HEIGHT,
            Qt::KeepAspectRatio, Qt::SmoothTransformation
            );
        if (!m_tutorialButton) {
            m_tutorialButton = new CustomClickableItem(tutorialButtonPixmapScaled);
            addItem(m_tutorialButton);
            connect(m_tutorialButton, &CustomClickableItem::clicked, [this](){
                QSize currentViewSize;
                if (!this->views().isEmpty() && this->views().first()) {
                    currentViewSize = this->views().first()->size();
                } else {
                    currentViewSize = this->sceneRect().size().toSize();
                    if (currentViewSize.isEmpty() || !currentViewSize.isValid()){
                        qWarning() << "StartScene: Cannot determine valid view size for tutorial panel from scene/view. Using a default size (800x600).";
                        currentViewSize = QSize(800,600);
                    }
                }
                qDebug() << "StartScene: Tutorial button clicked. Determined currentViewSize for panel:" << currentViewSize;
                showTutorialContent(!m_isTutorialVisible, currentViewSize);
            });
        } else {
            m_tutorialButton->setPixmap(tutorialButtonPixmapScaled);
        }

        qreal tutorialButtonX = (viewSize.width() - m_tutorialButton->boundingRect().width()) / 2;
        qreal tutorialButtonY;
        if (m_startButton && m_startButton->isVisible() && !m_startButton->pixmap().isNull()) {
            tutorialButtonY = m_startButton->y() + m_startButton->boundingRect().height() + BUTTON_SPACING_PIXELS;
        } else if (m_titleItem && m_titleItem->isVisible() && !m_titleItem->pixmap().isNull()) {
            tutorialButtonY = m_titleItem->y() + m_titleItem->boundingRect().height() + TITLE_TO_START_BUTTON_SPACING_PIXELS + BUTTON_SPACING_PIXELS + 10;
            qWarning() << "StartScene: Start button is not available for tutorial button positioning, using title item as fallback for tutorial button.";
        }
        else {
            tutorialButtonY = viewSize.height() * FALLBACK_Y_FACTOR_TUTORIAL_BUTTON_NO_TITLE_NO_START;
            qWarning() << "StartScene: Neither title nor start button is available for tutorial button positioning, using fallback Y for tutorial button.";
        }
        m_tutorialButton->setPos(tutorialButtonX, tutorialButtonY);
        m_tutorialButton->setVisible(true);
        qDebug() << "StartScene: Tutorial button created/updated. Pos:" << m_tutorialButton->pos() << "Size:" << m_tutorialButton->boundingRect().size() << "Visible:" << m_tutorialButton->isVisible() << "Enabled:" << m_tutorialButton->isEnabled();
    } else {
        qWarning() << "StartScene: Failed to load tutorial button image '" << tutorialButtonPath << "'. Tutorial button will not be shown.";
        if (m_tutorialButton) {
            m_tutorialButton->setVisible(false);
        }
    }

    showTutorialContent(false, viewSize);
    qDebug() << "StartScene: setupUi completed. Initial tutorial visibility:" << m_isTutorialVisible;
}

void StartScene::showTutorialContent(bool show, const QSize& viewSize) {
    m_isTutorialVisible = show;
    qDebug() << "StartScene: showTutorialContent called. Show:" << show << "with viewSize:" << viewSize;

    if (viewSize.isEmpty() || !viewSize.isValid()) {
        qWarning() << "StartScene: showTutorialContent called with invalid viewSize:" << viewSize << ". Aborting panel creation/update.";
        return;
    }

    if (show) {
        if (!m_tutorialPanel) {
            m_tutorialPanel = new QGraphicsRectItem(0, 0, viewSize.width() * 0.7, viewSize.height() * 0.6);
            m_tutorialPanel->setBrush(QColor(0, 0, 0, 180));
            m_tutorialPanel->setPen(Qt::NoPen);
            addItem(m_tutorialPanel);

            m_tutorialText = new QGraphicsTextItem(m_tutorialPanel);
            m_tutorialText->setDefaultTextColor(Qt::white);

            // --- 使用加载的字体名称更新教程文本 ---
            QString tutorialHtml = QString(
                                       "<div style='text-align: center; font-size: 25pt; line-height: 1.6;'>"
                                       "<h1><span style=\"font-family: '%1';\">操作提示</span></h1>" // 使用 m_chineseFontFamily
                                       "<p><span style=\"font-family: '%2'; font-weight: bold;\">Tips:</span></p>" // 使用 m_englishFontFamily
                                       "<p><span style=\"font-family: '%1';\">按 <span style=\"font-family: '%2'; font-weight: bold;\">J</span> 键切换轨道的内外侧</span></p>"
                                       "<p><span style=\"font-family: '%1';\">按 <span style=\"font-family: '%2'; font-weight: bold;\">K</span> 键切换到下一条轨道</span></p>"
                                       "<p><span style=\"font-family: '%1'; color: #FFA500;\">在他处乱按 <span style=\"font-family: '%2'; font-weight: bold;\">K</span> 会扣血哦，</span></p>"
                                       "<p><span style=\"font-family: '%1'; color: #FFA500;\">请确保飞船到达切换点再按下!</span></p>"
                                       //"<br><p><span style=\"font-family: '%1';\">点击下方关闭按钮返回</span></p>"
                                       "</div>")
                                       .arg(m_chineseFontFamily) // %1 对应中文字体
                                       .arg(m_englishFontFamily); // %2 对应英文字体
            m_tutorialText->setHtml(tutorialHtml);
            // --- 教程文本修改结束 ---

            m_tutorialText->setTextWidth(m_tutorialPanel->rect().width() * 0.9);

            const QString closeButtonPath = ":/images/close_button.png";
            QPixmap closeButtonPixmapOriginal(closeButtonPath);
            qDebug() << "StartScene: Attempting to load close button image '" << closeButtonPath << "'. IsNull:" << closeButtonPixmapOriginal.isNull() << "Size:" << closeButtonPixmapOriginal.size();
            if(closeButtonPixmapOriginal.isNull()) {
                qWarning() << "StartScene: Failed to load close button image '" << closeButtonPath << "'. Close button for tutorial will not be shown.";
            } else {
                QPixmap closeButtonPixmapScaled = closeButtonPixmapOriginal.scaled(
                    DESIRED_CLOSE_BUTTON_SIZE, DESIRED_CLOSE_BUTTON_SIZE,
                    Qt::KeepAspectRatio, Qt::SmoothTransformation
                    );
                if (!m_tutorialCloseButton) {
                    m_tutorialCloseButton = new CustomClickableItem(closeButtonPixmapScaled, m_tutorialPanel);
                    connect(m_tutorialCloseButton, &CustomClickableItem::clicked, [this](){
                        QSize currentViewSizeForClose;
                        if (!this->views().isEmpty() && this->views().first()) {
                            currentViewSizeForClose = this->views().first()->size();
                        } else {
                            currentViewSizeForClose = this->sceneRect().size().toSize();
                            if (currentViewSizeForClose.isEmpty() || !currentViewSizeForClose.isValid()){
                                qWarning() << "StartScene: Cannot determine valid view size for closing tutorial. Using a default size (800x600).";
                                currentViewSizeForClose = QSize(800,600);
                            }
                        }
                        showTutorialContent(false, currentViewSizeForClose);
                    });
                } else {
                    m_tutorialCloseButton->setPixmap(closeButtonPixmapScaled);
                }
                qDebug() << "StartScene: Tutorial close button created/updated.";
            }
        }

        m_tutorialPanel->setRect(0, 0, viewSize.width() * 0.7, viewSize.height() * 0.6);
        m_tutorialText->setTextWidth(m_tutorialPanel->rect().width() * 0.9);

        m_tutorialPanel->setPos((viewSize.width() - m_tutorialPanel->rect().width()) / 2,
                                (viewSize.height() - m_tutorialPanel->rect().height()) / 2);
        m_tutorialText->setPos( (m_tutorialPanel->rect().width() - m_tutorialText->boundingRect().width()) / 2,
                               m_tutorialPanel->rect().height() * 0.1 );

        if(m_tutorialCloseButton && !m_tutorialCloseButton->pixmap().isNull() && m_tutorialCloseButton->pixmap().width() > 0) {
            qreal closeButtonX = (m_tutorialPanel->rect().width() - m_tutorialCloseButton->boundingRect().width()) / 2;
            qreal closeButtonY = m_tutorialText->y() + m_tutorialText->boundingRect().height() + TEXT_TO_CLOSE_BUTTON_SPACING;
            if (closeButtonY + m_tutorialCloseButton->boundingRect().height() + CLOSE_BUTTON_BOTTOM_MARGIN > m_tutorialPanel->rect().height()) {
                closeButtonY = m_tutorialPanel->rect().height() - m_tutorialCloseButton->boundingRect().height() - CLOSE_BUTTON_BOTTOM_MARGIN;
            }
            m_tutorialCloseButton->setPos(closeButtonX, closeButtonY);
            m_tutorialCloseButton->setVisible(true);
            qDebug() << "StartScene: Tutorial close button positioned. Pos:" << m_tutorialCloseButton->pos() << " BoundingRect: " << m_tutorialCloseButton->boundingRect();
        } else if (m_tutorialCloseButton) {
            m_tutorialCloseButton->setVisible(false);
            qDebug() << "StartScene: Tutorial close button object exists but pixmap is invalid or not loaded, hiding button.";
        }

        m_tutorialPanel->setVisible(true);
        m_tutorialPanel->setZValue(10);
        qDebug() << "StartScene: Tutorial panel is now visible.";

        if(m_startButton) m_startButton->setEnabled(false);
        if(m_tutorialButton) m_tutorialButton->setEnabled(false);
        qDebug() << "StartScene: Main screen buttons (start/tutorial) disabled.";

    } else {
        if (m_tutorialPanel) {
            m_tutorialPanel->setVisible(false);
            qDebug() << "StartScene: Tutorial panel is now hidden.";
        }
        if(m_startButton) m_startButton->setEnabled(true);
        if(m_tutorialButton) m_tutorialButton->setEnabled(true);
        qDebug() << "StartScene: Main screen buttons (start/tutorial) enabled.";
    }
}

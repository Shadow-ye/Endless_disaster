#include "MainWindow.h"

#include "Audio.h"
#include "Codex.h"
#include "Cursors.h"
#include "GameWidget.h"
#include "LoadingWidget.h"
#include "MenuTitle.h"
#include "Platform.h"
#include "SettingRow.h"
#include "Storage.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>

#include <functional>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScroller>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace {
QString formatTime(float t) {
    const int total = int(std::max(0.f, t));
    return QString("%1:%2")
        .arg(total / 60, 2, 10, QChar('0'))
        .arg(total % 60, 2, 10, QChar('0'));
}

constexpr const char* kSlotKeys[4] = {"R", "F", "C", "V"};

// 主菜单标题与英文副标题的字距（em 比例）：收紧后既聚拢，又不会挤成一团
constexpr qreal kTitleLetterSpacing = 0.12;
constexpr qreal kSubLetterSpacing = 0.28;

struct HeroInfo {
    HeroClass hero;
    const char* name;
    const char* role;
    const char* stats;
};

// 职业子菜单按此顺序列出；新角色不需要立绘也能在子菜单里选
constexpr HeroInfo kHeroes[] = {
    {HeroClass::Warrior, "战士", "战士", "HP 120  ARM 14  MP 60  CRT 12%"},
    {HeroClass::Sword, "女剑客", "剑客", "HP 95  ARM 8  MP 85  SPD+16  CRT 22%"},
    {HeroClass::Mage, "女魔法师", "法师", "HP 78  ARM 4  MP 130  CRT 12%"},
    {HeroClass::Robot, "机甲人", "机甲人", "HP 105  ARM 12  MP 90  CRT 12%"},
};

const HeroInfo& heroInfo(HeroClass hero) {
    for (const HeroInfo& info : kHeroes) {
        if (info.hero == hero) {
            return info;
        }
    }
    return kHeroes[0];
}

QString findStoryBackground() {
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    for (const QString& root : Platform::dataRoots()) {
        candidates << root + QStringLiteral("/故事背景.png")
                   << root + QStringLiteral("/assets/ui/story_bg.png")
                   << root + QStringLiteral("/assets/故事背景.png");
    }
    candidates << QDir(appDir).absoluteFilePath(QStringLiteral("../assets/ui/story_bg.png"))
               << QDir(appDir).absoluteFilePath(QStringLiteral("../故事背景.png"));
    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return {};
}

QString findHeroIdleSheet(const QString& folder) {
    const QString relative = QStringLiteral("/assets/") + folder + QStringLiteral("/idle.png");
    for (const QString& root : Platform::dataRoots()) {
        const QString path = root + relative;
        if (QFile::exists(path)) {
            return path;
        }
    }
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        const QString path = dir.filePath(QStringLiteral("assets/") + folder + QStringLiteral("/idle.png"));
        if (QFile::exists(path)) {
            return path;
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return {};
}

// 取待机第 0 帧，裁掉透明边，最近邻放到槽位里才不会发糊。
QPixmap loadHeroPixelPortrait(const QString& folder, int frameSize, bool faceLeft) {
    const QString path = findHeroIdleSheet(folder);
    if (path.isEmpty() || frameSize <= 0) {
        return {};
    }
    QImage sheet(path);
    if (sheet.isNull() || sheet.width() < frameSize || sheet.height() < frameSize) {
        return {};
    }
    const QImage frame = sheet.copy(0, 0, frameSize, frameSize).convertToFormat(QImage::Format_ARGB32);
    int minX = frameSize;
    int minY = frameSize;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < frame.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
        for (int x = 0; x < frame.width(); ++x) {
            if (qAlpha(line[x]) == 0) {
                continue;
            }
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }
    if (maxX < minX) {
        return {};
    }
    QImage body = frame.copy(minX, minY, maxX - minX + 1, maxY - minY + 1);
    if (faceLeft) {
        body = body.mirrored(true, false);
    }
    return QPixmap::fromImage(body);
}

void placePrepareArt(QLabel* label, const QPixmap& src, const QRect& slot, Qt::Alignment align) {
    if (!label || src.isNull() || slot.width() <= 0 || slot.height() <= 0) {
        return;
    }
    const int fit = std::min(slot.width() / src.width(), slot.height() / src.height());
    const QPixmap scaled = fit >= 1
        ? src.scaled(src.width() * fit, src.height() * fit, Qt::IgnoreAspectRatio, Qt::FastTransformation)
        : src.scaled(slot.size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    label->setPixmap(scaled);
    QRect geo(slot.topLeft(), scaled.size());
    if (align.testFlag(Qt::AlignBottom)) {
        geo.moveBottom(slot.bottom());
    } else if (align.testFlag(Qt::AlignVCenter)) {
        geo.moveTop(slot.center().y() - scaled.height() / 2);
    } else {
        geo.moveTop(slot.top());
    }
    if (align.testFlag(Qt::AlignRight)) {
        geo.moveRight(slot.right());
    } else if (align.testFlag(Qt::AlignHCenter)) {
        geo.moveLeft(slot.center().x() - scaled.width() / 2);
    } else {
        geo.moveLeft(slot.left());
    }
    label->setGeometry(geo);
}

void swapDuplicate(std::array<QComboBox*, 4>& boxes, std::array<int, 4>& prev, int slot, int count) {
    if (slot < 0 || slot >= count || !boxes[slot]) {
        return;
    }
    const int chosen = boxes[slot]->currentData().toInt();
    const int previous = prev[slot];
    for (int i = 0; i < count; ++i) {
        if (i == slot || !boxes[i]) {
            continue;
        }
        if (boxes[i]->currentData().toInt() == chosen) {
            QSignalBlocker blocker(boxes[i]);
            const int index = boxes[i]->findData(previous);
            if (index >= 0) {
                boxes[i]->setCurrentIndex(index);
            }
            prev[i] = previous;
            break;
        }
    }
    prev[slot] = chosen;
}

// 主菜单底图由页面自己画，而不是拿一个兄弟 QLabel 垫底。
// 透明子控件（标题、按钮）压在兄弟控件上时，安卓首帧/脏矩形重绘经常拿不到底图，
// 旧像素（按钮残影、标题只亮一半）会一直留着；切后台再回来因为后备缓冲被重新分配才正常。
// 改成父控件自绘后，任何一次重绘都先铺底图、再画子控件，从根上消掉这类残影。
class MenuPage : public QWidget {
public:
    explicit MenuPage(QWidget* parent = nullptr) : QWidget(parent) {
        // 整块区域每帧都会填满，告诉 Qt 不用再擦一遍，也避免脏矩形里留旧内容
        setAttribute(Qt::WA_OpaquePaintEvent, true);
    }

    void setBackdrop(const QPixmap& source) {
        if (!source.isNull() && source.cacheKey() == source_.cacheKey()) {
            return;
        }
        source_ = source;
        scaled_ = QPixmap();
        update();
    }

    // 叠在底图之上、子控件之下的绘制（安卓：标题必须画在这层不透明底图上，
    // 单独放在透明子控件里白字会被安卓整层丢掉）
    void setOverlayPaint(std::function<void(QPainter&)> fn) { overlayPaint_ = std::move(fn); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0x3b, 0x35, 0x2e));
        if (!source_.isNull()) {
            if (scaled_.isNull()) {
                // 完整居中：按窗口等比缩放后整体可见，图里烘焙的顶部横幅不会被裁掉两端的字
                scaled_ = source_.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            painter.drawPixmap(QPoint((width() - scaled_.width()) / 2, (height() - scaled_.height()) / 2), scaled_);
        }
        if (overlayPaint_) {
            overlayPaint_(painter);
        }
    }

    void resizeEvent(QResizeEvent* event) override {
        scaled_ = QPixmap();
        QWidget::resizeEvent(event);
    }

private:
    QPixmap source_;
    QPixmap scaled_;
    std::function<void(QPainter&)> overlayPaint_;
};

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle("无尽之灾");
    resize(1100, 700);

    stack_ = new QStackedWidget(this);
    // 底图交给页面自己画（见上面的 MenuPage），不再用兄弟 QLabel 垫底
    auto* menuPage = new MenuPage(stack_);
    menu_ = menuPage;
    menu_->setObjectName("menuRoot");
    const QString bgPath = findStoryBackground();
    if (!bgPath.isEmpty()) {
        menuBgPix_ = QPixmap(bgPath);
        menuPage->setBackdrop(menuBgPix_);
    }

    menuContent_ = new QWidget(menu_);
    menuContent_->setObjectName("menuContent");
    menuContent_->setAttribute(Qt::WA_StyledBackground, true);
    menuContent_->setStyleSheet(
        "QWidget#menuContent { background: transparent; }"
        "QLabel#dim { color: #d7c7b4; background: rgba(12,10,9,150); padding: 4px 10px; }");
    auto* layout = new QVBoxLayout(menuContent_);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(40, 36, 40, 40);
    layout->setSpacing(14);

    QFont titleFont("KaiTi", 42);
    // 标题自绘（红色辉光 + 深红立体投影 + 故障错位，效果对齐展示页 site/），
    // 字号与字距由 layoutMenuTitle 按窗口宽度算好再交给它
    // 标题与英文副标题装进同一个容器：容器内的行距单独收紧（紧贴），
    // 对外仍是一个整体，和下面的记录条、按钮保持原来的正常间距
    auto* titleBlock = new QWidget(menuContent_);
    titleBlock->setObjectName(QStringLiteral("menuTitleBlock"));
    titleBlock->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titleBlock->setStyleSheet(QStringLiteral("QWidget#menuTitleBlock { background: transparent; }"));
    auto* titleLayout = new QVBoxLayout(titleBlock);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(0);
    menuTitle_ = new MenuTitle(titleBlock);
    titleLayout->addWidget(menuTitle_);
    menuSubTitle_ = new MenuTitle(titleBlock);
    menuSubTitle_->setText(QStringLiteral("ENDLESS DISASTER"));
    titleLayout->addWidget(menuSubTitle_);
    layout->addWidget(titleBlock);

    recordLabel_ = new QLabel(menuContent_);
    recordLabel_->setObjectName("dim");
    recordLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(recordLabel_);

    auto* start = new QPushButton("出发", menuContent_);
    continueButton_ = new QPushButton("继续", menuContent_);
    auto* achievementsBtn = new QPushButton("成就", menuContent_);
    auto* settingsBtn = new QPushButton("设置", menuContent_);
    auto* quit = new QPushButton("离开", menuContent_);
    for (QPushButton* btn : {start, continueButton_, achievementsBtn, settingsBtn, quit}) {
        btn->setFixedWidth(280);
        layout->addWidget(btn, 0, Qt::AlignHCenter);
    }

    // 放在右下角而不是按钮列里：手机横屏时标题已占去大半高度
    menuLinks_ = new QWidget(menu_);
    menuLinks_->setObjectName("menuLinks");
    menuLinks_->setAttribute(Qt::WA_StyledBackground, true);
    menuLinks_->setStyleSheet(
        "QWidget#menuLinks { background: transparent; }"
        "QLabel#dim { color: #d7c7b4; background: rgba(12,10,9,150); padding: 4px 10px; }"
        "QPushButton { padding: 6px 12px; text-align: center; }");
    auto* linksLayout = new QHBoxLayout(menuLinks_);
    linksLayout->setContentsMargins(0, 0, 0, 0);
    linksLayout->setSpacing(8);
    auto* versionLabel = new QLabel(QStringLiteral("v" ED_VERSION), menuLinks_);
    versionLabel->setObjectName("dim");
    auto* releasesBtn = new QPushButton(QStringLiteral("历史版本"), menuLinks_);
    auto* detailsBtn = new QPushButton(QStringLiteral("游戏详情"), menuLinks_);
    linksLayout->addWidget(versionLabel);
    linksLayout->addWidget(releasesBtn);
    linksLayout->addWidget(detailsBtn);
    connect(releasesBtn, &QPushButton::clicked, this, [] {
        Audio::instance().play(SfxId::Ui);
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Shadow-ye/Endless_disaster/releases")));
    });
    connect(detailsBtn, &QPushButton::clicked, this, [] {
        Audio::instance().play(SfxId::Ui);
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Shadow-ye/Endless_disaster")));
    });
    layoutMenuBackground();
    layoutMenuTitle();

    prepare_ = new QWidget(stack_);
    prepare_->setObjectName("prepareRoot");
    prepare_->setAttribute(Qt::WA_StyledBackground, true);
    prepare_->setStyleSheet("QWidget#prepareRoot { background: #f4efe6; }");

    auto makeArtLabel = [this](const QString& objectName) {
        auto* label = new QLabel(prepare_);
        label->setObjectName(objectName);
        label->setAlignment(Qt::AlignCenter);
        label->setScaledContents(false);
        label->setStyleSheet(QString("QLabel#%1 { background: transparent; border: none; }").arg(objectName));
        label->setCursor(Cursors::hand());
        label->installEventFilter(this);
        return label;
    };
    prepareWarriorArt_ = makeArtLabel(QStringLiteral("prepareWarriorArt"));
    prepareMageArt_ = makeArtLabel(QStringLiteral("prepareMageArt"));
    prepareSwordArt_ = makeArtLabel(QStringLiteral("prepareSwordArt"));
    prepareWarriorPix_ = loadHeroPixelPortrait(QStringLiteral("hero_warrior"), 64, true);
    prepareMagePix_ = loadHeroPixelPortrait(QStringLiteral("hero_mage"), 64, true);
    prepareSwordPix_ = loadHeroPixelPortrait(QStringLiteral("hero_sword"), 64, false);
    prepareWarriorOpacity_ = new QGraphicsOpacityEffect(prepareWarriorArt_);
    prepareMageOpacity_ = new QGraphicsOpacityEffect(prepareMageArt_);
    prepareSwordOpacity_ = new QGraphicsOpacityEffect(prepareSwordArt_);
    prepareWarriorArt_->setGraphicsEffect(prepareWarriorOpacity_);
    prepareMageArt_->setGraphicsEffect(prepareMageOpacity_);
    prepareSwordArt_->setGraphicsEffect(prepareSwordOpacity_);

    prepareContent_ = new QWidget(prepare_);
    prepareContent_->setObjectName("prepareContent");
    prepareContent_->setAttribute(Qt::WA_StyledBackground, true);
    prepareContent_->setStyleSheet(
        "QWidget#prepareContent { background: transparent; }"
        "QFrame#preparePanel { background: rgba(20,17,15,210); border: 1px solid #5c3a32; }"
        "QLabel#title { color: #f2e6d8; background: transparent; }"
        "QLabel#dim { color: #d7c7b4; background: rgba(12,10,9,120); padding: 4px 10px; }");
    auto* prepareOuter = new QVBoxLayout(prepareContent_);
    prepareOuter->setContentsMargins(0, 0, 0, 0);
    prepareOuter->setAlignment(Qt::AlignCenter);
    auto* prepareCard = new QFrame(prepareContent_);
    prepareCard->setObjectName("preparePanel");
    prepareCard->setMaximumWidth(460);
    auto* prepareLayout = new QVBoxLayout(prepareCard);
    prepareLayout->setContentsMargins(22, 22, 22, 22);
    prepareLayout->setSpacing(8);
    auto* prepareTitle = new QLabel("出发之前", prepareCard);
    prepareTitle->setObjectName("title");
    prepareTitle->setAlignment(Qt::AlignCenter);
    prepareTitle->setFont(titleFont);
    prepareLayout->addWidget(prepareTitle);
    skillHint_ = new QLabel(prepareCard);
    skillHint_->setObjectName("dim");
    skillHint_->setAlignment(Qt::AlignCenter);
    skillHint_->setWordWrap(true);
    prepareLayout->addWidget(skillHint_);
    glossary_ = new QLabel(
        "HP 生命　MP 魔力　STA 体力　SHD 护盾\n"
        "ARM 护甲　CRT 暴击　SPD 移速　LV 等级　XP 经验　SCORE 积分\n"
        "浅水区＝腐化史莱姆的 boss 房：跳上棋盘格排雷，boss 才会浮出",
        prepareCard);
    glossary_->setObjectName("dim");
    glossary_->setAlignment(Qt::AlignCenter);
    glossary_->setWordWrap(true);
    prepareLayout->addWidget(glossary_);
    // 手机横屏高度有限，名词解释并入下方可滚动的说明里
    glossary_->setVisible(!Platform::touchUi());

    heroButton_ = new QPushButton(prepareCard);
    heroButton_->setStyleSheet("QPushButton { text-align: center; }");
    prepareLayout->addWidget(heroButton_);
    heroStats_ = new QLabel(prepareCard);
    heroStats_->setObjectName("dim");
    heroStats_->setAlignment(Qt::AlignCenter);
    prepareLayout->addWidget(heroStats_);

    auto makePickRow = [&](int count, const int* pool, int poolSize, const int* defaults, int minWidth,
                           QComboBox** boxes, QPushButton** buttons, void (MainWindow::*onPicked)(int)) {
        auto* row = new QWidget(prepareCard);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 4, 0, 4);
        rowLayout->setSpacing(8);
        for (int i = 0; i < count; ++i) {
            auto* col = new QVBoxLayout();
            col->setSpacing(2);
            auto* key = new QLabel(kSlotKeys[i], row);
            key->setAlignment(Qt::AlignCenter);
            key->setStyleSheet("color:#c45c48; font-weight:bold; background:transparent;");
            auto* box = new QComboBox(row);
            box->setMinimumWidth(minWidth);
            fillSkillBox(box, pool, poolSize, defaults[i]);
            boxes[i] = box;
            col->addWidget(key);
            col->addWidget(box);
            if (Platform::touchUi()) {
                buttons[i] = makeSkillPickButton(box, row);
                col->addWidget(buttons[i]);
            }
            rowLayout->addLayout(col);
            connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i, onPicked](int) {
                (this->*onPicked)(i);
            });
        }
        prepareLayout->addWidget(row);
        return row;
    };
    const int warriorDefaults[3] = {kSkillSpin, kSkillSwordQi, kSkillThrust};
    warriorPickRow_ = makePickRow(3, kWarriorSkillPool, int(std::size(kWarriorSkillPool)), warriorDefaults, 100,
        warriorSkillBoxes_.data(), warriorSkillButtons_.data(), &MainWindow::onWarriorSkillPicked);
    const int mageDefaults[4] = {kSkillMageBolt, kSkillNova, kSkillFlight, kSkillBurial};
    magePickRow_ = makePickRow(4, kMageSkillPool, int(std::size(kMageSkillPool)), mageDefaults, 90,
        mageSkillBoxes_.data(), mageSkillButtons_.data(), &MainWindow::onMageSkillPicked);
    magePickRow_->setVisible(false);
    const int robotDefaults[3] = {kSkillScatter, kSkillMissile, kSkillBoost};
    robotPickRow_ = makePickRow(3, kRobotSkillPool, int(std::size(kRobotSkillPool)), robotDefaults, 100,
        robotSkillBoxes_.data(), robotSkillButtons_.data(), &MainWindow::onRobotSkillPicked);
    robotPickRow_->setVisible(false);

    skillDetail_ = new QTextBrowser(prepareCard);
    skillDetail_->setOpenExternalLinks(false);
    skillDetail_->setOpenLinks(false);
    skillDetail_->setMinimumHeight(Platform::touchUi() ? 80 : 140);
    skillDetail_->setMaximumHeight(200);
    skillDetail_->setStyleSheet("QTextBrowser { background: transparent; border: none; color: #cbbfae; }");
    if (Platform::touchUi()) {
        QScroller::grabGesture(skillDetail_->viewport(), QScroller::LeftMouseButtonGesture);
    }
    prepareLayout->addWidget(skillDetail_, 1);

    auto* go = new QPushButton("进入", prepareCard);
    auto* back = new QPushButton("返回", prepareCard);
    if (Platform::touchUi()) {
        auto* actionRow = new QHBoxLayout();
        actionRow->setSpacing(8);
        actionRow->addWidget(back);
        actionRow->addWidget(go);
        prepareLayout->addLayout(actionRow);
    } else {
        prepareLayout->addWidget(go);
        prepareLayout->addWidget(back);
    }
    prepareOuter->addWidget(prepareCard, 0, Qt::AlignHCenter | Qt::AlignVCenter);

    // 页内浮层子菜单：职业选择与触屏的技能选择共用
    picker_ = new QWidget(prepare_);
    picker_->setObjectName("picker");
    picker_->setAttribute(Qt::WA_StyledBackground, true);
    picker_->setStyleSheet(
        "QWidget#picker { background: rgba(0,0,0,150); }"
        "QFrame#pickerCard { background: #14110f; border: 1px solid #5c3a32; }"
        "QLabel { color: #f2e6d8; background: transparent; }"
        "QPushButton { padding: 10px 8px; }"
        "QPushButton:checked { background: #5c3a32; color: #f2e6d8; }");
    picker_->installEventFilter(this);
    auto* pickerOuter = new QVBoxLayout(picker_);
    pickerOuter->setAlignment(Qt::AlignCenter);
    auto* pickerCard = new QFrame(picker_);
    pickerCard->setObjectName("pickerCard");
    pickerCard->setMinimumWidth(380);
    auto* pickerLayout = new QVBoxLayout(pickerCard);
    pickerLayout->setContentsMargins(18, 18, 18, 18);
    pickerLayout->setSpacing(10);
    pickerTitle_ = new QLabel(pickerCard);
    pickerTitle_->setAlignment(Qt::AlignCenter);
    pickerLayout->addWidget(pickerTitle_);
    pickerGrid_ = new QGridLayout();
    pickerGrid_->setSpacing(8);
    pickerLayout->addLayout(pickerGrid_);
    auto* pickerCancel = new QPushButton("取消", pickerCard);
    pickerLayout->addWidget(pickerCancel);
    connect(pickerCancel, &QPushButton::clicked, this, [this] { picker_->hide(); });
    pickerOuter->addWidget(pickerCard, 0, Qt::AlignCenter);
    picker_->hide();
    layoutPrepareArt();
    updatePrepareArtHighlight();

    game_ = new GameWidget(stack_);
    loading_ = new LoadingWidget(stack_);

    // 普通页面：不接触摸、不自绘、不声明不透明区域。
    // 这些「安卓优化」曾经把整页的脏矩形上屏搞坏（值在变、画面冻住，点别处才一次性跳过去），
    // 后台缓冲的刷新交给 Qt 常规路径（和 Windows 完全一致）。
    settings_ = new QWidget(stack_);
    settings_->setObjectName("settingsRoot");
    settings_->setAttribute(Qt::WA_StyledBackground, true);
    settings_->setStyleSheet("QWidget#settingsRoot { background: #0c0a09; }");
    auto* settingsOuter = new QVBoxLayout(settings_);
    settingsOuter->setAlignment(Qt::AlignCenter);
    auto* settingsCard = new QWidget(settings_);
    settingsCard->setObjectName("panel");
    // QWidget 本身不画背景，QSS 的 background / border 需要这个属性才会落到卡片上
    // （原来这里是 QFrame，换过来后必须补上，否则卡片的底色和边框会整块不画）
    settingsCard->setAttribute(Qt::WA_StyledBackground, true);
    settingsCard->setFixedWidth(420);
    // 和暂停菜单里的设置面板用逐字相同的样式表：背景、标签、勾选框走同一条绘制路径，
    // 滑块拿到的调色板与几何约束也就和游戏内完全一致
    settingsCard->setStyleSheet(
        "QWidget#panel { background: #14110f; border: 1px solid #5c3a32; }"
        "QLabel { background: transparent; color: #d7c7b4; border: none; }"
        "QLabel#title { color: #e4d4c4; }"
        "QCheckBox { color: #d7c7b4; }");
    auto* settingsLayout = new QVBoxLayout(settingsCard);
    settingsLayout->setContentsMargins(28, 28, 28, 28);
    settingsLayout->setSpacing(12);
    auto* settingsTitle = new QLabel("设置", settingsCard);
    settingsTitle->setObjectName("title");
    settingsTitle->setAlignment(Qt::AlignCenter);
    settingsTitle->setFont(titleFont);
    settingsLayout->addWidget(settingsTitle);
    // 音效 / BGM 照「天气系统」+「画面亮度」这两个已经确认正常的控件来搭：
    // 开关独占一行（与天气系统完全同构），音量滑块单独一行、行内不带开关（与亮度条完全同构）。
    // 曾经把开关和滑块塞进同一个 row：在这张静止的设置页上，那一行的重绘就是不上屏
    // （值在变、画面冻住，点别的控件才一次性跳过去），而同样结构在游戏内却没事。
    // 拆成两个既有形状之后，就没有任何一处是"只有这一行才有"的布局组合了。
    sfxCheck_ = new QCheckBox("开启音效", settingsCard);
    settingsLayout->addWidget(sfxCheck_);
    const SettingRow sfxRow = makeSettingRow(settingsCard, "音效音量", 0, 100, 0, false);
    sfxSlider_ = sfxRow.slider;
    sfxVolumeLabel_ = sfxRow.value;
    settingsLayout->addWidget(sfxRow.row);

    bgmCheck_ = new QCheckBox("开启 BGM", settingsCard);
    settingsLayout->addWidget(bgmCheck_);
    const SettingRow bgmRow = makeSettingRow(settingsCard, "BGM 音量", 0, 100, 0, false);
    bgmSlider_ = bgmRow.slider;
    bgmVolumeLabel_ = bgmRow.value;
    settingsLayout->addWidget(bgmRow.row);

    guideCheck_ = new QCheckBox("开局立即获得天赋「世界指引」", settingsCard);
    guideCheck_->setToolTip("开启后新开局直接拥有「世界指引」：按 G（安卓点右上角「寻路」）标出迷宫遗迹方向并画出通路，不必先击杀 20 个入侵怪物。");
    settingsLayout->addWidget(guideCheck_);

    showFpsCheck_ = new QCheckBox("显示帧率", settingsCard);
    showFpsCheck_->setToolTip("开启后游戏画面顶部计时旁显示 FPS、单帧耗时(ms) 与场上怪物/粒子数量，仅在游戏进行时刷新。");
    settingsLayout->addWidget(showFpsCheck_);

    weatherCheck_ = new QCheckBox("天气系统", settingsCard);
    weatherCheck_->setToolTip("局内每隔一段时间刮起一场天灾：雨天、雷雨天、暴风雪、冰雹、沙尘暴、台风，"
                              "每场持续半分多钟，之间有一段平静期。\n"
                              "雷雨天的落雷与冰雹的碎击都只伤怪物，不会伤到你。关掉后本局不再出现任何天气。");
    settingsLayout->addWidget(weatherCheck_);

    const SettingRow brightRow = makeSettingRow(settingsCard, "画面亮度", 50, 150, 0, false);
    brightSlider_ = brightRow.slider;
    brightValueLabel_ = brightRow.value;
    brightSlider_->setToolTip("整幅游戏画面的明暗，100% 为默认。末世滤镜只压暗地图，角色与特效保持原亮度。");
    brightRow.row->setToolTip(brightSlider_->toolTip());
    settingsLayout->addWidget(brightRow.row);

    auto* settingsBack = new QPushButton("返回", settingsCard);
    settingsLayout->addWidget(settingsBack);

    // 安卓上这张静止设置页的局部重绘不一定被提交上屏：只标该控件自己那点脏区时，
    // 时不时就有某一项一直冻着（数值在变、画面不动），等点了别的控件才一起补画出来；
    // 而且换一下控件的形状 / 顺序，"冻住的那一项"就换人——说明问题不在控件本身，
    // 而在这页的局部重绘能不能上屏。所以这里统一兜底：任何一项改值 / 勾选，
    // 都把整页标脏一次。同一次事件循环里的多次 update() 会被 Qt 合并，
    // 拖动时约等于每帧一次整页重绘，代价可控。
    auto nudgePage = [this] {
        if (settings_) {
            settings_->update();
        }
    };
    for (QCheckBox* box : {sfxCheck_, bgmCheck_, guideCheck_, showFpsCheck_, weatherCheck_}) {
        connect(box, &QCheckBox::toggled, this, [nudgePage](bool) { nudgePage(); });
    }
    for (QSlider* slider : {sfxSlider_, bgmSlider_, brightSlider_}) {
        connect(slider, &QSlider::valueChanged, this, [nudgePage](int) { nudgePage(); });
    }

    settingsOuter->addWidget(settingsCard, 0, Qt::AlignHCenter);

    // 拖动音量 / 亮度条时不再逐格写盘：安卓上每次落盘都要 fsync，
    // 拖动产生的上百个 valueChanged 全去写 settings.json 会把手感拖成一格一顿。
    // 拖动中只让音量实时生效，停手（或离开设置页）后再统一落盘。
    settingsSaveTimer_ = new QTimer(this);
    settingsSaveTimer_->setSingleShot(true);
    settingsSaveTimer_->setInterval(220);
    connect(settingsSaveTimer_, &QTimer::timeout, this, [this] {
        // 到点只把已经暂存的快照写下去（saveSettingsUi 会重新暂存并重启定时器，别在这里绕回去）。
        // 若此时手还按在滑块上就往后推：拖动途中一次 fsync 就能让手柄顿一格。
        if ((sfxSlider_ && sfxSlider_->isSliderDown()) || (bgmSlider_ && bgmSlider_->isSliderDown())
            || (brightSlider_ && brightSlider_->isSliderDown())) {
            settingsSaveTimer_->start();
            return;
        }
        Storage::flushSettings();
    });

    achievements_ = new QWidget(stack_);
    achievements_->setObjectName("achievementsRoot");
    achievements_->setAttribute(Qt::WA_StyledBackground, true);
    achievements_->setStyleSheet("QWidget#achievementsRoot { background: #0c0a09; }");
    auto* achievementsOuter = new QVBoxLayout(achievements_);
    achievementsOuter->setAlignment(Qt::AlignCenter);
    auto* achievementsCard = new QFrame(achievements_);
    achievementsCard->setObjectName("panel");
    achievementsCard->setFixedWidth(560);
    auto* achievementsLayout = new QVBoxLayout(achievementsCard);
    achievementsLayout->setContentsMargins(28, 28, 28, 28);
    achievementsLayout->setSpacing(12);
    auto* achievementsTitle = new QLabel("成就", achievementsCard);
    achievementsTitle->setObjectName("title");
    achievementsTitle->setAlignment(Qt::AlignCenter);
    achievementsTitle->setFont(titleFont);
    achievementsLayout->addWidget(achievementsTitle);
    achievementSummary_ = new QLabel(achievementsCard);
    achievementSummary_->setObjectName("dim");
    achievementSummary_->setAlignment(Qt::AlignCenter);
    achievementsLayout->addWidget(achievementSummary_);
    achievementList_ = new QTextBrowser(achievementsCard);
    achievementList_->setOpenExternalLinks(false);
    achievementList_->setOpenLinks(false);
    achievementList_->setMinimumHeight(Platform::touchUi() ? 160 : 260);
    achievementList_->setStyleSheet("QTextBrowser { background: transparent; border: none; color: #cbbfae; }");
    if (Platform::touchUi()) {
        QScroller::grabGesture(achievementList_->viewport(), QScroller::LeftMouseButtonGesture);
    }
    achievementsLayout->addWidget(achievementList_, 1);
    auto* achievementsBack = new QPushButton("返回", achievementsCard);
    achievementsLayout->addWidget(achievementsBack);
    achievementsOuter->addWidget(achievementsCard, 0, Qt::AlignHCenter);

    stack_->addWidget(menu_);
    stack_->addWidget(prepare_);
    stack_->addWidget(settings_);
    stack_->addWidget(achievements_);
    stack_->addWidget(game_);
    stack_->addWidget(loading_);

    // 加载页进度走满后由它自己发信号收尾；超时兜底保证不会困在加载页
    loadingTimeout_ = new QTimer(this);
    loadingTimeout_->setSingleShot(true);
    connect(loadingTimeout_, &QTimer::timeout, this, [this] { finishLoading(); });
    connect(loading_, &LoadingWidget::finished, this, [this] { finishLoading(); });

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(stack_);

    connect(start, &QPushButton::clicked, this, [this] {
        Audio::instance().play(SfxId::Ui);
        showPrepare();
    });
    connect(back, &QPushButton::clicked, this, [this] {
        Audio::instance().play(SfxId::Ui);
        showMenu();
    });
    connect(go, &QPushButton::clicked, this, [this] {
        Audio::instance().play(SfxId::Ui);
        startPrepared();
    });
    connect(continueButton_, &QPushButton::clicked, this, [this] {
        Audio::instance().play(SfxId::Ui);
        continueGame();
    });
    connect(settingsBtn, &QPushButton::clicked, this, [this] {
        Audio::instance().play(SfxId::Ui);
        showSettings();
    });
    connect(settingsBack, &QPushButton::clicked, this, [this] {
        saveSettingsUi();
        Audio::instance().play(SfxId::Ui);
        showMenu();
    });
    connect(achievementsBtn, &QPushButton::clicked, this, [this] {
        Audio::instance().play(SfxId::Ui);
        showAchievements();
    });
    connect(achievementsBack, &QPushButton::clicked, this, [this] {
        Audio::instance().play(SfxId::Ui);
        showMenu();
    });
    connect(sfxCheck_, &QCheckBox::toggled, this, [this](bool on) {
        sfxSlider_->setEnabled(on);
        saveSettingsUi();
        if (on) {
            Audio::instance().play(SfxId::Ui);
        }
    });
    connect(sfxSlider_, &QSlider::valueChanged, this, [this](int value) {
        sfxVolumeLabel_->setText(QString("%1%").arg(value));
        // 拖动中只更新内存快照（零磁盘 I/O）；非拖动改值（点滑槽 / 滚轮 / 键盘）走防抖落盘。
        // 安卓上一次 QSaveFile::commit 的 fsync 就能卡一下，绝对不能在拖动途中写盘。
        pushSettingsUi(false);
        if (!sfxSlider_->isSliderDown()) {
            settingsSaveTimer_->start();
        }
    });
    connect(sfxSlider_, &QSlider::sliderReleased, this, [this] { saveSettingsUi(); });
    connect(bgmCheck_, &QCheckBox::toggled, this, [this](bool on) {
        bgmSlider_->setEnabled(on);
        saveSettingsUi();
        if (on) {
            // 结算后的 5 号曲正在放就别打断，交给 ensureBgmLoop 判断
            Audio::instance().ensureBgmLoop();
        }
    });
    connect(bgmSlider_, &QSlider::valueChanged, this, [this](int value) {
        bgmVolumeLabel_->setText(QString("%1%").arg(value));
        pushSettingsUi(false);
        if (!bgmSlider_->isSliderDown()) {
            settingsSaveTimer_->start();
        }
    });
    connect(bgmSlider_, &QSlider::sliderReleased, this, [this] { saveSettingsUi(); });
    connect(guideCheck_, &QCheckBox::toggled, this, [this](bool on) {
        (void)on;
        saveSettingsUi();
    });
    connect(showFpsCheck_, &QCheckBox::toggled, this, [this](bool on) {
        (void)on;
        saveSettingsUi();
    });
    connect(weatherCheck_, &QCheckBox::toggled, this, [this](bool on) {
        (void)on;
        saveSettingsUi();
    });
    connect(brightSlider_, &QSlider::valueChanged, this, [this](int value) {
        brightValueLabel_->setText(QString("%1%").arg(value));
        pushSettingsUi(false);
        if (!brightSlider_->isSliderDown()) {
            settingsSaveTimer_->start();
        }
    });
    connect(brightSlider_, &QSlider::sliderReleased, this, [this] { saveSettingsUi(); });
    connect(quit, &QPushButton::clicked, this, [] { QApplication::quit(); });
    connect(game_, &GameWidget::returnedToMenu, this, [this] { showMenu(); });
    connect(heroButton_, &QPushButton::clicked, this, [this] {
        Audio::instance().play(SfxId::Ui);
        openHeroPicker();
    });

    loadSettingsUi();
    refreshPrepareSkills();
    refreshMenu();
    if (Audio::instance().bgmEnabled()) {
        Audio::instance().ensureBgmLoop();
    }
}

void MainWindow::fillSkillBox(QComboBox* box, const int* pool, int poolSize, int selected) {
    QSignalBlocker blocker(box);
    box->clear();
    for (int i = 0; i < poolSize; ++i) {
        box->addItem(skillText(pool[i]).name, pool[i]);
    }
    const int index = box->findData(selected);
    box->setCurrentIndex(index >= 0 ? index : 0);
}

QPushButton* MainWindow::makeSkillPickButton(QComboBox* box, QWidget* parent) {
    // 安卓上 QComboBox 的弹出列表是独立窗口，会被裁切且点选不生效；触屏改用页内浮层，下拉框仍作数据源
    box->setVisible(false);
    auto* button = new QPushButton(box->currentText(), parent);
    button->setStyleSheet("QPushButton { text-align: center; padding: 8px 6px; }");
    connect(button, &QPushButton::clicked, this, [this, box] {
        Audio::instance().play(SfxId::Ui);
        openSkillPicker(box);
    });
    return button;
}

void MainWindow::openPicker(const QString& title, const QStringList& options, int current, int columns,
    const std::function<void(int)>& onPick) {
    pickerTitle_->setText(title);
    while (QLayoutItem* item = pickerGrid_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    QWidget* card = pickerGrid_->parentWidget();
    for (int i = 0; i < options.size(); ++i) {
        auto* option = new QPushButton(options[i], card);
        option->setCheckable(true);
        option->setChecked(i == current);
        connect(option, &QPushButton::clicked, this, [this, onPick, i] {
            picker_->hide();
            onPick(i);
            Audio::instance().play(SfxId::Ui);
        });
        pickerGrid_->addWidget(option, i / columns, i % columns);
    }
    picker_->setGeometry(prepare_->rect());
    picker_->show();
    picker_->raise();
}

void MainWindow::openSkillPicker(QComboBox* box) {
    std::vector<QComboBox*> group;
    if (std::find(mageSkillBoxes_.begin(), mageSkillBoxes_.end(), box) != mageSkillBoxes_.end()) {
        group.assign(mageSkillBoxes_.begin(), mageSkillBoxes_.end());
    } else if (std::find(robotSkillBoxes_.begin(), robotSkillBoxes_.end(), box) != robotSkillBoxes_.end()) {
        group.assign(robotSkillBoxes_.begin(), robotSkillBoxes_.end());
    } else {
        group.assign(warriorSkillBoxes_.begin(), warriorSkillBoxes_.end());
    }
    const int slot = int(std::find(group.begin(), group.end(), box) - group.begin());
    QStringList options;
    for (int i = 0; i < box->count(); ++i) {
        const int skill = box->itemData(i).toInt();
        QString text = box->itemText(i);
        for (int other = 0; other < int(group.size()); ++other) {
            if (other != slot && group[other]->currentData().toInt() == skill) {
                text += QString("（与 %1 互换）").arg(kSlotKeys[other]);
            }
        }
        options << text;
    }
    openPicker(QString("选择 %1 键技能").arg(kSlotKeys[slot]), options, box->currentIndex(), 2,
        [box](int index) { box->setCurrentIndex(index); });
}

void MainWindow::openHeroPicker() {
    QStringList options;
    int current = 0;
    for (int i = 0; i < int(std::size(kHeroes)); ++i) {
        options << QString("%1　　%2").arg(kHeroes[i].name, kHeroes[i].stats);
        if (kHeroes[i].hero == hero_) {
            current = i;
        }
    }
    openPicker(QStringLiteral("选择职业"), options, current, 1,
        [this](int index) { selectHeroClass(kHeroes[index].hero); });
}

void MainWindow::onWarriorSkillPicked(int slot) {
    std::array<QComboBox*, 4> boxes{warriorSkillBoxes_[0], warriorSkillBoxes_[1], warriorSkillBoxes_[2], nullptr};
    std::array<int, 4> prev{warriorSkillPrev_[0], warriorSkillPrev_[1], warriorSkillPrev_[2], -1};
    swapDuplicate(boxes, prev, slot, 3);
    warriorSkillPrev_[0] = prev[0];
    warriorSkillPrev_[1] = prev[1];
    warriorSkillPrev_[2] = prev[2];
    refreshPrepareSkills();
}

void MainWindow::onMageSkillPicked(int slot) {
    swapDuplicate(mageSkillBoxes_, mageSkillPrev_, slot, 4);
    refreshPrepareSkills();
}

void MainWindow::onRobotSkillPicked(int slot) {
    std::array<QComboBox*, 4> boxes{robotSkillBoxes_[0], robotSkillBoxes_[1], robotSkillBoxes_[2], nullptr};
    std::array<int, 4> prev{robotSkillPrev_[0], robotSkillPrev_[1], robotSkillPrev_[2], -1};
    swapDuplicate(boxes, prev, slot, 3);
    robotSkillPrev_[0] = prev[0];
    robotSkillPrev_[1] = prev[1];
    robotSkillPrev_[2] = prev[2];
    refreshPrepareSkills();
}

void MainWindow::refreshPrepareSkills() {
    auto line = [](const QString& key, const SkillText& text) {
        return QString("<p style='margin:4px 0;'><span style='color:#c45c48;'>%1</span>　<b>%2</b><br>%3</p>")
            .arg(key, text.name, text.detail);
    };
    QString html;
    html += line("Q", guardSkillText());
    html += line("E", healSkillText());
    const HeroInfo& info = heroInfo(hero_);
    // 后缀用 » 而不是 ▸：U+25xx 几何符号在安卓系统字体里常缺字形，会渲染成豆腐块
    heroButton_->setText(QString("职业：%1　»").arg(info.name));
    heroStats_->setText(info.stats);
    const bool mage = hero_ == HeroClass::Mage;
    const bool robot = hero_ == HeroClass::Robot;
    warriorPickRow_->setVisible(!mage && !robot);
    magePickRow_->setVisible(mage);
    robotPickRow_->setVisible(robot);
    // 迷宫遗迹与「寻路」在所有平台都有：触屏用右上角按钮代替 G 键
    const QString guideNote = Platform::touchUi()
        ? QStringLiteral("击杀 20 个入侵怪物可获得天赋「世界指引」，用右上角「寻路」按钮开关（设置里可改为开局直接拥有）。")
        : QStringLiteral("击杀 20 个入侵怪物可获得天赋「世界指引」，按 G 使用寻路（设置里可改为开局直接拥有）。");
    if (mage) {
        skillHint_->setText("Q 防御　E 恢复　法师技能栏：R / F / C / V（自选，不可重复）");
        for (int i = 0; i < 4; ++i) {
            html += line(kSlotKeys[i], skillText(mageSkillBoxes_[i]->currentData().toInt()));
        }
        html += "<p style='margin:8px 0 0 0; color:#a89888;'>可选技能共九个，带走其中四个。" + guideNote + "</p>";
    } else {
        const auto& boxes = robot ? robotSkillBoxes_ : warriorSkillBoxes_;
        skillHint_->setText(QString("Q 防御　E 恢复　%1技能栏：R / F / C（自选，不可重复）").arg(info.role));
        for (int i = 0; i < 3; ++i) {
            html += line(kSlotKeys[i], skillText(boxes[i]->currentData().toInt()));
        }
        html += robot
            ? "<p style='margin:8px 0 0 0; color:#a89888;'>散射 / 爆破弹 / 推进 / 过载 / 蜂群 / 磁力场 / 战术医疗包 / 喷气背包 / 肘击，九选三分配到三个键位。" + guideNote + "</p>"
            : "<p style='margin:8px 0 0 0; color:#a89888;'>回旋斩 / 剑气 / 突刺 / 狂化，四选三分配到三个键位。" + guideNote + "</p>";
    }
    if (Platform::touchUi()) {
        html += "<p style='margin:8px 0 0 0; color:#a89888;'>" + glossary_->text().replace('\n', "<br>") + "</p>";
    }
    skillDetail_->setText(html);
    for (int i = 0; i < 3; ++i) {
        if (warriorSkillButtons_[i]) {
            warriorSkillButtons_[i]->setText(warriorSkillBoxes_[i]->currentText());
        }
        if (robotSkillButtons_[i]) {
            robotSkillButtons_[i]->setText(robotSkillBoxes_[i]->currentText());
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (mageSkillButtons_[i]) {
            mageSkillButtons_[i]->setText(mageSkillBoxes_[i]->currentText());
        }
    }
}

void MainWindow::refreshMenu() {
    const Records records = Storage::loadRecords();
    recordLabel_->setText(QString("最高用时 %1    最高积分 %2").arg(formatTime(records.bestTime)).arg(records.bestScore));
    continueButton_->setEnabled(Storage::hasContinue());
}

void MainWindow::layoutMenuBackground() {
    if (!menu_ || !menuContent_) {
        return;
    }
    const QRect r = menu_->rect();
    if (r.width() <= 0 || r.height() <= 0) {
        return;
    }
    if (auto* page = dynamic_cast<MenuPage*>(menu_)) {
        page->setBackdrop(menuBgPix_);
#ifdef Q_OS_ANDROID
        // 安卓：标题由菜单页在底图上画（透明子控件上 drawText 会被整层丢掉）
        page->setOverlayPaint([this](QPainter& painter) {
            for (MenuTitle* title : {menuTitle_, menuSubTitle_}) {
                if (!title || !title->isVisible()) {
                    continue;
                }
                painter.save();
                painter.translate(title->mapTo(menu_, QPoint(0, 0)));
                title->paintOn(painter);
                painter.restore();
            }
        });
        if (menuTitle_) {
            menuTitle_->setHostWidget(menu_);
        }
        if (menuSubTitle_) {
            menuSubTitle_->setHostWidget(menu_);
        }
#endif
    }
    menuContent_->setGeometry(r);
    menuContent_->raise();
    if (menuLinks_) {
        const QSize size = menuLinks_->sizeHint();
        menuLinks_->setGeometry(r.width() - size.width() - 16, r.height() - size.height() - 14, size.width(), size.height());
        menuLinks_->raise();
    }
    layoutMenuTitle();
    // 标题字号是按窗口宽度算的，setTitleFont / setFixedHeight 会让 LayoutRequest 推迟到
    // 下一轮事件循环。安卓首次进入时窗口尺寸变化最大，第一帧往往按旧布局先把按钮画出来，
    // 等布局生效后按钮移到新位置，而透明容器下的背景图是兄弟 QLabel、旧位置的脏矩形不会被它
    // 重绘，于是留下按钮色的阶梯残影。这里同步跑完布局，再整块重画一遍避开这个首帧竞态。
    if (QLayout* contentLayout = menuContent_->layout()) {
        contentLayout->activate();
    }
    menu_->update();
}

void MainWindow::layoutMenuTitle() {
    if (!menuTitle_ || !menu_ || menu_->width() < 320) {
        // 构造阶段窗口还没拿到真实尺寸，此时按极窄宽度算出来的字号小得离谱，
        // 会在首帧留下一版错误的标题/布局，等真正 show 后再次变化就成了残影。
        return;
    }
    // 左右各留 30px：辉光与故障错位都在文字之外，贴着布局边缘会被切
    const int maxW = qMax(120, menu_->width() - 64 - 60);
    // 目标最大 500px；受窗口宽度限制，尽量撑满。
    const int maxPx = 500;
    const QString text = QStringLiteral("无尽之灾");
    // 标题用界面的无衬线粗体：笔画方正好看，辉光与故障错位也压得住；
    // 原来的楷体细笔画叠上发光会糊成一团
    QFont font(Platform::uiFontFamily());
    font.setBold(true);
    font.setStyleStrategy(QFont::PreferAntialias);

    int lo = 32;
    int hi = maxPx;
    int best = 32;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        font.setPixelSize(mid);
        font.setLetterSpacing(QFont::AbsoluteSpacing, qMax(4.0, mid * 0.06));
        const QFontMetrics fm(font);
        if (fm.horizontalAdvance(text) <= maxW && fm.height() <= int(menu_->height() * 0.55)) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    // 按窗口撑满后收 30%，再按要求整体缩 20%（两次）
    best = std::max(16, int(best * 0.7 * 0.8 * 0.8));
    // 字距收紧：汉字 0.12em、英文 0.28em（之前是 0.25em / 0.5em，看着散）
    font.setPixelSize(best);
    font.setLetterSpacing(QFont::AbsoluteSpacing, std::max(2.0, best * kTitleLetterSpacing));

    // 英文副标题：与主标题同一套特效（辉光 / 立体投影 / 故障错位）。
    // 间距收紧后英文串变短，两侧就对不齐了，所以字号不再拍比例，
    // 而是二分到与汉字墨迹同宽——间距小、宽度仍一致
    const QString subText = QStringLiteral("ENDLESS DISASTER");
    const qreal titleInk = QFontMetricsF(font).tightBoundingRect(text).width();

    QFont subFont(Platform::uiFontFamily());
    subFont.setBold(true);
    subFont.setStyleStrategy(QFont::PreferAntialias);
    const auto subFontAt = [&](int pixelSize) {
        QFont probe(subFont);
        probe.setPixelSize(pixelSize);
        probe.setLetterSpacing(QFont::AbsoluteSpacing, std::max(2.0, pixelSize * kSubLetterSpacing));
        return probe;
    };
    int subLo = 8;
    int subHi = std::max(subLo, int(best * 0.6));
    while (subLo <= subHi) {
        const int mid = (subLo + subHi) / 2;
        if (QFontMetricsF(subFontAt(mid)).tightBoundingRect(subText).width() <= titleInk) {
            subLo = mid + 1;
        } else {
            subHi = mid - 1;
        }
    }
    // 字号只能取整数像素，先挑误差最小的一档，剩下的零头用字距找齐，两侧才对得严丝合缝
    const int below = std::max(8, subHi);
    const qreal errBelow = titleInk - QFontMetricsF(subFontAt(below)).tightBoundingRect(subText).width();
    const qreal errAbove = QFontMetricsF(subFontAt(below + 1)).tightBoundingRect(subText).width() - titleInk;
    subFont = subFontAt(errAbove < errBelow ? below + 1 : below);
    const qreal leftover = titleInk - QFontMetricsF(subFont).tightBoundingRect(subText).width();
    if (qAbs(leftover) > 0.5) {
        subFont.setLetterSpacing(QFont::AbsoluteSpacing,
            std::max(2.0, subFont.letterSpacing() + leftover / std::max(1, int(subText.size()))));
    }

    // 高度按墨迹算，不按行高算（行高含 leading，会让两行之间空出很大一块）
    menuTitle_->setTitleFont(font);
    menuTitle_->setFixedHeight(int(std::ceil(QFontMetricsF(font).tightBoundingRect(text).height())) + 14);

    menuSubTitle_->setText(subText);
    menuSubTitle_->setTitleFont(subFont);
    menuSubTitle_->setFixedHeight(int(std::ceil(QFontMetricsF(subFont).tightBoundingRect(subText).height())) + 6);
}

void MainWindow::layoutPrepareArt() {
    if (!prepare_ || !prepareContent_) {
        return;
    }
    const QRect r = prepare_->rect();
    if (r.width() <= 0 || r.height() <= 0) {
        return;
    }

    // 像素立绘：女剑客左下朝向菜单；战士右上、法师右下朝向菜单。
    const int w = r.width();
    const int h = r.height();
    const QRect swordSlot(0, int(h * 0.12), int(w * 0.30), int(h * 0.88));
    const QRect warriorSlot(int(w * 0.78), int(h * 0.02), int(w * 0.20), int(h * 0.32));
    const QRect mageSlot(int(w * 0.74), int(h * 0.58), int(w * 0.24), int(h * 0.40));
    placePrepareArt(prepareSwordArt_, prepareSwordPix_, swordSlot, Qt::AlignLeft | Qt::AlignBottom);
    placePrepareArt(prepareWarriorArt_, prepareWarriorPix_, warriorSlot, Qt::AlignRight | Qt::AlignTop);
    placePrepareArt(prepareMageArt_, prepareMagePix_, mageSlot, Qt::AlignRight | Qt::AlignBottom);

    const int panelW = qMin(460, qMax(340, int(w * 0.38)));
    const int panelX = (w - panelW) / 2;
    prepareContent_->setGeometry(panelX, int(h * 0.04), panelW, int(h * 0.92));

    prepareWarriorArt_->lower();
    prepareMageArt_->raise();
    prepareSwordArt_->raise();
    prepareContent_->raise();
    if (picker_) {
        picker_->setGeometry(r);
        picker_->raise();
    }
}

void MainWindow::updatePrepareArtHighlight() {
    const HeroClass hero = hero_;
    auto setOpacity = [](QGraphicsOpacityEffect* effect, bool selected) {
        if (effect) {
            effect->setOpacity(selected ? 1.0 : 0.48);
        }
    };
    setOpacity(prepareWarriorOpacity_, hero == HeroClass::Warrior);
    setOpacity(prepareSwordOpacity_, hero == HeroClass::Sword);
    setOpacity(prepareMageOpacity_, hero == HeroClass::Mage);
}

void MainWindow::selectHeroClass(HeroClass hero) {
    hero_ = hero;
    refreshPrepareSkills();
    updatePrepareArtHighlight();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == picker_ && event->type() == QEvent::MouseButtonPress) {
        return true;
    }
    if (watched == picker_ && event->type() == QEvent::MouseButtonRelease) {
        // 点在卡片外的遮罩上视为取消
        const QPoint pos = static_cast<QMouseEvent*>(event)->position().toPoint();
        if (!picker_->childAt(pos)) {
            picker_->hide();
        }
        return true;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        if (watched == prepareWarriorArt_) {
            selectHeroClass(HeroClass::Warrior);
            Audio::instance().play(SfxId::Ui);
            return true;
        }
        if (watched == prepareSwordArt_) {
            selectHeroClass(HeroClass::Sword);
            Audio::instance().play(SfxId::Ui);
            return true;
        }
        if (watched == prepareMageArt_) {
            selectHeroClass(HeroClass::Mage);
            Audio::instance().play(SfxId::Ui);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    layoutMenuBackground();
    layoutPrepareArt();
}

void MainWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    layoutMenuBackground();
    layoutPrepareArt();
    // 安卓首次进入：surface 尺寸 / DPR / 字体往往要等这一轮事件循环甚至更久才定下来。
    // 首帧按旧状态建出来的标题缓存会一直错到切后台再回来（resume 会重新走一遍 show + 布局）。
    // 这里在排布稳定前后各重排一次，并强制整块重画，等价于把 resume 时做的事情提前做掉。
    if (!menuFirstPaintDone_) {
        menuFirstPaintDone_ = true;
        QTimer::singleShot(0, this, [this] {
            layoutMenuBackground();
            layoutPrepareArt();
        });
        QTimer::singleShot(150, this, [this] {
            layoutMenuBackground();
            layoutPrepareArt();
            if (menu_ && menu_->isVisible()) {
                menu_->repaint();
            }
        });
    }
}

void MainWindow::showMenu() {
    if (picker_) {
        picker_->hide();
    }
    // 离开设置页 / 从游戏回主菜单：把没落盘的设置补写
    Storage::flushSettings();
    refreshMenu();
    stack_->setCurrentWidget(menu_);
    layoutMenuBackground();
    if (Audio::instance().bgmEnabled()) {
        Audio::instance().ensureBgmLoop();
    }
}

void MainWindow::showPrepare() {
    refreshPrepareSkills();
    updatePrepareArtHighlight();
    stack_->setCurrentWidget(prepare_);
    layoutPrepareArt();
}

void MainWindow::showSettings() {
    // 快照已经是最新的（拖动中也是先写快照），这里只把还没落盘的补写一次
    Storage::flushSettings();
    loadSettingsUi();
    stack_->setCurrentWidget(settings_);
}

void MainWindow::showAchievements() {
    refreshAchievements();
    stack_->setCurrentWidget(achievements_);
}

void MainWindow::refreshAchievements() {
    const QSet<QString> unlocked = Storage::loadAchievements();
    const std::vector<AchievementText>& list = achievementTexts();
    int ownedCount = 0;
    QString html;
    for (const AchievementText& text : list) {
        const bool owned = unlocked.contains(text.id);
        if (owned) {
            ++ownedCount;
        }
        // 未解锁的也列出来，但条件用问号藏起来，解锁后才显示
        const QString stateColor = owned ? QStringLiteral("#c45c48") : QStringLiteral("#5c5148");
        const QString nameColor = owned ? QStringLiteral("#f2e6d8") : QStringLiteral("#8a7b70");
        const QString detail = owned ? text.detail.toHtmlEscaped() : QStringLiteral("？？？");
        html += QString("<p style='margin:6px 0 16px 0;'>"
                        "<span style='color:%1;'>■</span> "
                        "<b style='color:%2;'>%3</b>"
                        "<span style='color:%1;'>　%4</span><br>"
                        "<span style='color:%1;'>%5</span></p>")
                    .arg(stateColor, nameColor, text.name.toHtmlEscaped(),
                        owned ? QStringLiteral("已解锁") : QStringLiteral("未解锁"), detail);
    }
    achievementList_->setHtml(html);
    achievementSummary_->setText(QString("已解锁 %1 / %2").arg(ownedCount).arg(int(list.size())));
}

void MainWindow::loadSettingsUi() {
    const AppSettings settings = Storage::loadSettings();
    QSignalBlocker b1(sfxCheck_);
    QSignalBlocker b2(sfxSlider_);
    QSignalBlocker b3(bgmCheck_);
    QSignalBlocker b4(bgmSlider_);
    QSignalBlocker b5(guideCheck_);
    QSignalBlocker b6(showFpsCheck_);
    QSignalBlocker b7(weatherCheck_);
    QSignalBlocker b8(brightSlider_);
    sfxCheck_->setChecked(settings.sfxEnabled);
    sfxSlider_->setValue(settings.sfxVolume);
    sfxSlider_->setEnabled(settings.sfxEnabled);
    sfxVolumeLabel_->setText(QString("%1%").arg(settings.sfxVolume));
    bgmCheck_->setChecked(settings.bgmEnabled);
    bgmSlider_->setValue(settings.bgmVolume);
    bgmSlider_->setEnabled(settings.bgmEnabled);
    bgmVolumeLabel_->setText(QString("%1%").arg(settings.bgmVolume));
    guideCheck_->setChecked(settings.guideAtStart);
    showFpsCheck_->setChecked(settings.showFps);
    weatherCheck_->setChecked(settings.weather);
    brightSlider_->setValue(settings.brightness);
    brightValueLabel_->setText(QString("%1%").arg(settings.brightness));
    Audio::instance().setSfxEnabled(settings.sfxEnabled);
    Audio::instance().setSfxVolume(settings.sfxVolume);
    Audio::instance().setBgmEnabled(settings.bgmEnabled);
    Audio::instance().setBgmVolume(settings.bgmVolume);
}

void MainWindow::saveSettingsUi() {
    // 拖动途中一律不落盘：安卓上一次 QSaveFile::commit 的 fsync 就能把主线程卡一下，
    // 被防抖定时器撞上就是一格一顿。这里只把当前值存进快照，停手 / 离开界面时再补写。
    if ((sfxSlider_ && sfxSlider_->isSliderDown()) || (bgmSlider_ && bgmSlider_->isSliderDown())
        || (brightSlider_ && brightSlider_->isSliderDown())) {
        pushSettingsUi(false);
        if (settingsSaveTimer_) {
            settingsSaveTimer_->start();
        }
        return;
    }
    if (settingsSaveTimer_) {
        settingsSaveTimer_->stop();
    }
    pushSettingsUi(true);
}

void MainWindow::pushSettingsUi(bool commit) {
    // 从快照出发再覆盖本页字段：其它字段（autoAim / showFps…）不会被默认值冲掉，
    // 也能把游戏内刚改过、还没落盘的值原样带过去。
    AppSettings settings = Storage::loadSettings();
    settings.sfxEnabled = sfxCheck_->isChecked();
    settings.sfxVolume = sfxSlider_->value();
    settings.bgmEnabled = bgmCheck_->isChecked();
    settings.bgmVolume = bgmSlider_->value();
    settings.guideAtStart = guideCheck_->isChecked();
    settings.showFps = showFpsCheck_->isChecked();
    settings.weather = weatherCheck_->isChecked();
    settings.brightness = brightSlider_->value();
    if (commit) {
        // 只有确定下来（松手 / 勾选开关 / 离开页面）才动音频：安卓上一次 setSfxVolume 会逐池
        // 刷几十个 QSoundEffect，拖动中每帧来一遍会把主线程占住，画面要等松手才刷得出来
        // （亮度条不碰音频，所以它一直是正常的）。
        Audio::instance().setSfxEnabled(settings.sfxEnabled);
        Audio::instance().setSfxVolume(settings.sfxVolume);
        Audio::instance().setBgmEnabled(settings.bgmEnabled);
        Audio::instance().setBgmVolume(settings.bgmVolume);
        // 落盘一律推给防抖定时器：QSaveFile::commit 里的 fsync 在安卓上要十几毫秒，
        // 直接写在点击回调里，开关自己那格的重绘就得排在它后面——手感跟拖滑块一个样。
        // 定时器到点写的是这份快照；切后台 / 退出前 main() 里还有 flushSettings 兜底。
        Storage::stageSettings(settings);
        if (settingsSaveTimer_) {
            settingsSaveTimer_->start();
        }
    } else {
        // 拖动中的预览只更新进程内快照与界面，零音频调用、零磁盘 I/O
        Storage::stageSettings(settings);
    }
}

void MainWindow::startPrepared() {
    // 出发界面选好的技能先记成加载参数，真正开局交给加载页分步完成
    std::array<int, 4> skills{0, 1, 2, -1};
    if (hero_ == HeroClass::Mage) {
        skills = {mageSkillBoxes_[0]->currentData().toInt(),
            mageSkillBoxes_[1]->currentData().toInt(),
            mageSkillBoxes_[2]->currentData().toInt(),
            mageSkillBoxes_[3]->currentData().toInt()};
    } else if (hero_ == HeroClass::Robot) {
        skills = {robotSkillBoxes_[0]->currentData().toInt(),
            robotSkillBoxes_[1]->currentData().toInt(),
            robotSkillBoxes_[2]->currentData().toInt(),
            -1};
    } else {
        skills = {warriorSkillBoxes_[0]->currentData().toInt(),
            warriorSkillBoxes_[1]->currentData().toInt(),
            warriorSkillBoxes_[2]->currentData().toInt(),
            -1};
    }
    startLoadingNewRun(hero_, skills);
}

void MainWindow::continueGame() {
    QJsonObject game;
    if (!Storage::loadContinue(game)) {
        QMessageBox::warning(this, "继续", "没有可用存档。");
        return;
    }
    startLoadingContinue(game);
}

void MainWindow::startLoadingNewRun(HeroClass hero, const std::array<int, 4>& skills) {
    loadingKind_ = LoadingKind::NewRun;
    loadingHero_ = hero;
    loadingSkills_ = skills;
    loadingSave_ = QJsonObject();
    beginLoading();
}

void MainWindow::startLoadingContinue(const QJsonObject& game) {
    loadingKind_ = LoadingKind::Continue;
    loadingSave_ = game;
    beginLoading();
}

int MainWindow::loadingStepCount() const {
    return loadingKind_ == LoadingKind::NewRun ? 5 : 2;
}

// 切到加载页并启动分步推进：每步之间让出事件循环，进度条动画不会卡住
void MainWindow::beginLoading() {
    loadingStep_ = 0;
    // 先让游戏侧停下：加载页露头的第一帧起就不再跑上一局的模拟
    game_->beginPrepare();
    stack_->setCurrentWidget(loading_);
    loading_->start();
    loading_->setProgress(0.f);
    loading_->setStageText(QStringLiteral("意识降临中，正在转译画面信息..."));
    loadingTimeout_->start(8000);
    QTimer::singleShot(0, this, &MainWindow::advanceLoading);
}

void MainWindow::advanceLoading() {
    const int count = loadingStepCount();
    if (loadingStep_ >= count) {
        // 步骤全做完：把进度交给加载页推到 100%，等它播完脉冲发 finished
        loading_->setProgress(1.f);
        return;
    }
    const int step = loadingStep_;
    loading_->setProgress(float(step) / float(count));
    runLoadingStep(step);
    ++loadingStep_;
    // 让出事件循环：加载页在这一帧刷新动画，下一步留到下一帧
    QTimer::singleShot(0, this, &MainWindow::advanceLoading);
}

void MainWindow::runLoadingStep(int step) {
    if (loadingKind_ == LoadingKind::Continue) {
        if (step == 0) {
            game_->prepareContinue(loadingSave_);
        } else {
            game_->prepareFinish();
        }
        return;
    }
    switch (step) {
    case 0:
        game_->prepareNew(loadingHero_, loadingSkills_[0], loadingSkills_[1], loadingSkills_[2], loadingSkills_[3]);
        break;
    case 1:
        game_->prepareNewStepReset();
        break;
    case 2:
        game_->prepareNewStepSpawns();
        break;
    case 3:
        game_->prepareNewStepRuin();
        break;
    default:
        game_->prepareFinish();
        break;
    }
}

void MainWindow::finishLoading() {
    loadingTimeout_->stop();
    if (stack_->currentWidget() != loading_) {
        return;
    }
    // 兜底：超时或某一步出问题时，把没跑完的步骤在这里一次补齐，绝不把玩家困在加载页
    while (loadingStep_ < loadingStepCount()) {
        runLoadingStep(loadingStep_);
        ++loadingStep_;
    }
    loading_->stop();
    loading_->setProgress(1.f);
    loadingStep_ = 0;
    loadingSave_ = QJsonObject();
    // 切页的同时才真正开始本局，保证加载页停留的时间不计入游戏时长
    game_->startRun();
    stack_->setCurrentWidget(game_);
    game_->setFocus();
}

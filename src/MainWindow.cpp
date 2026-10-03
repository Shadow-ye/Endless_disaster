#include "MainWindow.h"

#include "Audio.h"
#include "Codex.h"
#include "GameWidget.h"
#include "LoadingWidget.h"
#include "Platform.h"
#include "Storage.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
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
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle("无尽之灾");
    resize(1100, 700);

    stack_ = new QStackedWidget(this);
    menu_ = new QWidget(stack_);
    menu_->setObjectName("menuRoot");
    menu_->setAttribute(Qt::WA_StyledBackground, true);
    menu_->setStyleSheet("QWidget#menuRoot { background: transparent; }");
    menuBg_ = new QLabel(menu_);
    menuBg_->setObjectName("menuBg");
    menuBg_->setAlignment(Qt::AlignCenter);
    menuBg_->setScaledContents(false);
    menuBg_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    menuBg_->setStyleSheet("QLabel#menuBg { background: transparent; border: none; }");
    menuBg_->lower();
    const QString bgPath = findStoryBackground();
    if (!bgPath.isEmpty()) {
        menuBgPix_ = QPixmap(bgPath);
        if (!menuBgPix_.isNull()) {
            menuBg_->setPixmap(menuBgPix_);
        }
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
    // 全局 QSS 的 font-size:15px 会覆盖 setFont；必须在样式表里写字号。
    menuTitle_ = new QLabel(QStringLiteral("无尽之灾"), menuContent_);
    menuTitle_->setObjectName("menuHeroTitle");
    menuTitle_->setAlignment(Qt::AlignCenter);
    auto* titleShadow = new QGraphicsDropShadowEffect(menuTitle_);
    titleShadow->setBlurRadius(18);
    titleShadow->setOffset(0, 6);
    titleShadow->setColor(QColor(0, 0, 0, 180));
    menuTitle_->setGraphicsEffect(titleShadow);
    layout->addWidget(menuTitle_);

    recordLabel_ = new QLabel(menuContent_);
    recordLabel_->setObjectName("dim");
    recordLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(recordLabel_);

    auto* start = new QPushButton("出发", menuContent_);
    continueButton_ = new QPushButton("继续", menuContent_);
    auto* settingsBtn = new QPushButton("设置", menuContent_);
    auto* quit = new QPushButton("离开", menuContent_);
    for (QPushButton* btn : {start, continueButton_, settingsBtn, quit}) {
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
        label->setCursor(Qt::PointingHandCursor);
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

    settings_ = new QWidget(stack_);
    auto* settingsOuter = new QVBoxLayout(settings_);
    settingsOuter->setAlignment(Qt::AlignCenter);
    auto* settingsCard = new QFrame(settings_);
    settingsCard->setObjectName("panel");
    settingsCard->setFixedWidth(420);
    auto* settingsLayout = new QVBoxLayout(settingsCard);
    settingsLayout->setContentsMargins(28, 28, 28, 28);
    settingsLayout->setSpacing(12);
    auto* settingsTitle = new QLabel("设置", settingsCard);
    settingsTitle->setObjectName("title");
    settingsTitle->setAlignment(Qt::AlignCenter);
    settingsTitle->setFont(titleFont);
    settingsLayout->addWidget(settingsTitle);
    sfxCheck_ = new QCheckBox("开启音效", settingsCard);
    settingsLayout->addWidget(sfxCheck_);
    auto* volRow = new QWidget(settingsCard);
    auto* volLayout = new QHBoxLayout(volRow);
    volLayout->setContentsMargins(0, 0, 0, 0);
    auto* volLabel = new QLabel("音效音量", volRow);
    sfxSlider_ = new QSlider(Qt::Horizontal, volRow);
    sfxSlider_->setRange(0, 100);
    sfxVolumeLabel_ = new QLabel(volRow);
    sfxVolumeLabel_->setMinimumWidth(40);
    volLayout->addWidget(volLabel);
    volLayout->addWidget(sfxSlider_, 1);
    volLayout->addWidget(sfxVolumeLabel_);
    settingsLayout->addWidget(volRow);

    bgmCheck_ = new QCheckBox("开启 BGM", settingsCard);
    settingsLayout->addWidget(bgmCheck_);
    auto* bgmRow = new QWidget(settingsCard);
    auto* bgmLayout = new QHBoxLayout(bgmRow);
    bgmLayout->setContentsMargins(0, 0, 0, 0);
    auto* bgmLabel = new QLabel("BGM 音量", bgmRow);
    bgmSlider_ = new QSlider(Qt::Horizontal, bgmRow);
    bgmSlider_->setRange(0, 100);
    bgmVolumeLabel_ = new QLabel(bgmRow);
    bgmVolumeLabel_->setMinimumWidth(40);
    bgmLayout->addWidget(bgmLabel);
    bgmLayout->addWidget(bgmSlider_, 1);
    bgmLayout->addWidget(bgmVolumeLabel_);
    settingsLayout->addWidget(bgmRow);

    guideCheck_ = new QCheckBox("开局立即获得天赋「世界指引」", settingsCard);
    guideCheck_->setToolTip("开启后新开局直接拥有「世界指引」：按 G（安卓点右上角「寻路」）标出迷宫遗迹方向并画出通路，不必先击杀 20 个入侵怪物。");
    settingsLayout->addWidget(guideCheck_);

    auto* settingsBack = new QPushButton("返回", settingsCard);
    settingsLayout->addWidget(settingsBack);
    settingsOuter->addWidget(settingsCard, 0, Qt::AlignHCenter);

    stack_->addWidget(menu_);
    stack_->addWidget(prepare_);
    stack_->addWidget(settings_);
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
    connect(sfxCheck_, &QCheckBox::toggled, this, [this](bool on) {
        sfxSlider_->setEnabled(on);
        saveSettingsUi();
        if (on) {
            Audio::instance().play(SfxId::Ui);
        }
    });
    connect(sfxSlider_, &QSlider::valueChanged, this, [this](int value) {
        sfxVolumeLabel_->setText(QString("%1%").arg(value));
        saveSettingsUi();
    });
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
        saveSettingsUi();
    });
    connect(guideCheck_, &QCheckBox::toggled, this, [this](bool on) {
        (void)on;
        saveSettingsUi();
    });
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
    heroButton_->setText(QString("职业：%1　▸").arg(info.name));
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
        html += "<p style='margin:8px 0 0 0; color:#a89888;'>可选技能共六个，带走其中四个。" + guideNote + "</p>";
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
    if (!menu_ || !menuBg_ || !menuContent_) {
        return;
    }
    const QRect r = menu_->rect();
    if (r.width() <= 0 || r.height() <= 0) {
        return;
    }
    menuBg_->setGeometry(r);
    if (!menuBgPix_.isNull()) {
        // Cover-crop: scale to fill, then let QLabel clip to geometry.
        const QPixmap scaled = menuBgPix_.scaled(r.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        menuBg_->setPixmap(scaled);
    }
    menuContent_->setGeometry(r);
    menuBg_->lower();
    menuContent_->raise();
    if (menuLinks_) {
        const QSize size = menuLinks_->sizeHint();
        menuLinks_->setGeometry(r.width() - size.width() - 16, r.height() - size.height() - 14, size.width(), size.height());
        menuLinks_->raise();
    }
    layoutMenuTitle();
}

void MainWindow::layoutMenuTitle() {
    if (!menuTitle_ || !menu_) {
        return;
    }
    const int maxW = qMax(120, menu_->width() - 64);
    // 目标最大 500px；受窗口宽度限制，尽量撑满。
    const int maxPx = 500;
    const QString text = QStringLiteral("无尽之灾");
    QFont font(Platform::titleFontFamily());
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

    font.setPixelSize(best);
    font.setLetterSpacing(QFont::AbsoluteSpacing, qMax(4.0, best * 0.06));
    menuTitle_->setFont(font);
    // 样式表字号优先于全局 15px，必须写在这里。
    menuTitle_->setStyleSheet(QStringLiteral(
        "QLabel#menuHeroTitle {"
        "  color: #0a0807;"
        "  background: transparent;"
        "  font-family: '%3';"
        "  font-weight: 700;"
        "  font-size: %1px;"
        "  letter-spacing: %2px;"
        "  padding: 4px 8px;"
        "}").arg(best).arg(qMax(4, int(best * 0.06))).arg(Platform::titleFontFamily()));
    const QFontMetrics fm(font);
    menuTitle_->setFixedHeight(fm.height() + 20);
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
}

void MainWindow::showMenu() {
    if (picker_) {
        picker_->hide();
    }
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
    loadSettingsUi();
    stack_->setCurrentWidget(settings_);
}

void MainWindow::loadSettingsUi() {
    const AppSettings settings = Storage::loadSettings();
    QSignalBlocker b1(sfxCheck_);
    QSignalBlocker b2(sfxSlider_);
    QSignalBlocker b3(bgmCheck_);
    QSignalBlocker b4(bgmSlider_);
    QSignalBlocker b5(guideCheck_);
    sfxCheck_->setChecked(settings.sfxEnabled);
    sfxSlider_->setValue(settings.sfxVolume);
    sfxSlider_->setEnabled(settings.sfxEnabled);
    sfxVolumeLabel_->setText(QString("%1%").arg(settings.sfxVolume));
    bgmCheck_->setChecked(settings.bgmEnabled);
    bgmSlider_->setValue(settings.bgmVolume);
    bgmSlider_->setEnabled(settings.bgmEnabled);
    bgmVolumeLabel_->setText(QString("%1%").arg(settings.bgmVolume));
    guideCheck_->setChecked(settings.guideAtStart);
    Audio::instance().setSfxEnabled(settings.sfxEnabled);
    Audio::instance().setSfxVolume(settings.sfxVolume);
    Audio::instance().setBgmEnabled(settings.bgmEnabled);
    Audio::instance().setBgmVolume(settings.bgmVolume);
}

void MainWindow::saveSettingsUi() {
    AppSettings settings = Storage::loadSettings();
    settings.sfxEnabled = sfxCheck_->isChecked();
    settings.sfxVolume = sfxSlider_->value();
    settings.bgmEnabled = bgmCheck_->isChecked();
    settings.bgmVolume = bgmSlider_->value();
    settings.guideAtStart = guideCheck_->isChecked();
    Audio::instance().setSfxEnabled(settings.sfxEnabled);
    Audio::instance().setSfxVolume(settings.sfxVolume);
    Audio::instance().setBgmEnabled(settings.bgmEnabled);
    Audio::instance().setBgmVolume(settings.bgmVolume);
    Storage::saveSettings(settings);
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
    // 记录单步耗时：开局卡顿排查时直接看这条日志就能定位是哪一段重
    QElapsedTimer stepClock;
    stepClock.start();
    if (loadingKind_ == LoadingKind::Continue) {
        if (step == 0) {
            game_->prepareContinue(loadingSave_);
        } else {
            game_->prepareFinish();
        }
        qDebug("loading continue step %d: %lld ms", step, stepClock.elapsed());
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
    qDebug("loading new-run step %d: %lld ms", step, stepClock.elapsed());
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

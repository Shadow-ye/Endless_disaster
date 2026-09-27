#include "MainWindow.h"

#include "Audio.h"
#include "Codex.h"
#include "GameWidget.h"
#include "Platform.h"
#include "Storage.h"

#include <QApplication>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString formatTime(float t) {
    const int total = int(std::max(0.f, t));
    return QString("%1:%2")
        .arg(total / 60, 2, 10, QChar('0'))
        .arg(total % 60, 2, 10, QChar('0'));
}

constexpr const char* kSlotKeys[4] = {"R", "F", "C", "V"};

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

QString findCharacterImage(const QString& fileName) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    for (const QString& root : Platform::dataRoots()) {
        candidates << root + QStringLiteral("/character-img/") + fileName
                   << root + QStringLiteral("/assets/character-img/") + fileName;
    }
    candidates << QDir(appDir).absoluteFilePath(QStringLiteral("../assets/character-img/") + fileName)
               << QDir(appDir).absoluteFilePath(QStringLiteral("../build/character-img/") + fileName);
    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return {};
}

QPixmap loadCharacterPixmap(const QString& fileName) {
    const QString path = findCharacterImage(fileName);
    if (path.isEmpty()) {
        return {};
    }
    return QPixmap(path);
}

void placePrepareArt(QLabel* label, const QPixmap& src, const QRect& slot, Qt::Alignment align) {
    if (!label || src.isNull() || slot.width() <= 0 || slot.height() <= 0) {
        return;
    }
    const QPixmap scaled = src.scaled(slot.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
    prepareWarriorPix_ = loadCharacterPixmap(QStringLiteral("战士.png"));
    prepareMagePix_ = loadCharacterPixmap(QStringLiteral("法师.png"));
    prepareSwordPix_ = loadCharacterPixmap(QStringLiteral("女剑士.png"));
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
        "QLabel#dim { color: #d7c7b4; background: rgba(12,10,9,120); padding: 4px 10px; }"
        "QRadioButton { color: #d7c7b4; background: transparent; }");
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
    skillHint_ = new QLabel("点击立绘或选项选择职业　Q 防御　E 恢复", prepareCard);
    skillHint_->setObjectName("dim");
    skillHint_->setAlignment(Qt::AlignCenter);
    skillHint_->setWordWrap(true);
    prepareLayout->addWidget(skillHint_);
    auto* glossary = new QLabel(
        "HP 生命　MP 魔力　STA 体力　SHD 护盾\n"
        "ARM 护甲　CRT 暴击　SPD 移速　LV 等级　XP 经验　SCORE 积分",
        prepareCard);
    glossary->setObjectName("dim");
    glossary->setAlignment(Qt::AlignCenter);
    glossary->setWordWrap(true);
    prepareLayout->addWidget(glossary);

    classGroup_ = new QButtonGroup(this);
    auto* warrior = new QRadioButton("战士    HP 120  ARM 14  MP 60  CRT 12%", prepareCard);
    auto* sword = new QRadioButton("女剑客  HP 95  ARM 8  MP 85  SPD+16  CRT 22%", prepareCard);
    auto* mage = new QRadioButton("女魔法师  HP 78  ARM 4  MP 130  CRT 12%", prepareCard);
    warrior->setChecked(true);
    classGroup_->addButton(warrior, int(HeroClass::Warrior));
    classGroup_->addButton(sword, int(HeroClass::Sword));
    classGroup_->addButton(mage, int(HeroClass::Mage));
    for (QRadioButton* button : {warrior, sword, mage}) {
        prepareLayout->addWidget(button);
    }

    warriorPickRow_ = new QWidget(prepareCard);
    auto* warriorLayout = new QHBoxLayout(warriorPickRow_);
    warriorLayout->setContentsMargins(0, 4, 0, 4);
    warriorLayout->setSpacing(8);
    const int warriorDefaults[3] = {kSkillSpin, kSkillSwordQi, kSkillThrust};
    for (int i = 0; i < 3; ++i) {
        auto* col = new QVBoxLayout();
        col->setSpacing(2);
        auto* key = new QLabel(kSlotKeys[i], warriorPickRow_);
        key->setAlignment(Qt::AlignCenter);
        key->setStyleSheet("color:#c45c48; font-weight:bold; background:transparent;");
        auto* box = new QComboBox(warriorPickRow_);
        box->setMinimumWidth(100);
        fillSkillBox(box, kWarriorSkillPool, int(std::size(kWarriorSkillPool)), warriorDefaults[i]);
        warriorSkillBoxes_[i] = box;
        col->addWidget(key);
        col->addWidget(box);
        warriorLayout->addLayout(col);
        connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i](int) {
            onWarriorSkillPicked(i);
        });
    }
    prepareLayout->addWidget(warriorPickRow_);

    magePickRow_ = new QWidget(prepareCard);
    auto* mageLayout = new QHBoxLayout(magePickRow_);
    mageLayout->setContentsMargins(0, 4, 0, 4);
    mageLayout->setSpacing(8);
    const int mageDefaults[4] = {kSkillMageBolt, kSkillNova, kSkillFlight, kSkillBurial};
    for (int i = 0; i < 4; ++i) {
        auto* col = new QVBoxLayout();
        col->setSpacing(2);
        auto* key = new QLabel(kSlotKeys[i], magePickRow_);
        key->setAlignment(Qt::AlignCenter);
        key->setStyleSheet("color:#c45c48; font-weight:bold; background:transparent;");
        auto* box = new QComboBox(magePickRow_);
        box->setMinimumWidth(90);
        fillSkillBox(box, kMageSkillPool, int(std::size(kMageSkillPool)), mageDefaults[i]);
        mageSkillBoxes_[i] = box;
        col->addWidget(key);
        col->addWidget(box);
        mageLayout->addLayout(col);
        connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i](int) {
            onMageSkillPicked(i);
        });
    }
    magePickRow_->setVisible(false);
    prepareLayout->addWidget(magePickRow_);

    skillDetail_ = new QTextBrowser(prepareCard);
    skillDetail_->setOpenExternalLinks(false);
    skillDetail_->setOpenLinks(false);
    skillDetail_->setMinimumHeight(140);
    skillDetail_->setMaximumHeight(200);
    skillDetail_->setStyleSheet("QTextBrowser { background: transparent; border: none; color: #cbbfae; }");
    prepareLayout->addWidget(skillDetail_, 1);

    auto* go = new QPushButton("进入", prepareCard);
    auto* back = new QPushButton("返回", prepareCard);
    prepareLayout->addWidget(go);
    prepareLayout->addWidget(back);
    prepareOuter->addWidget(prepareCard, 0, Qt::AlignHCenter | Qt::AlignVCenter);
    layoutPrepareArt();
    updatePrepareArtHighlight();

    game_ = new GameWidget(stack_);

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

    auto* settingsBack = new QPushButton("返回", settingsCard);
    settingsLayout->addWidget(settingsBack);
    settingsOuter->addWidget(settingsCard, 0, Qt::AlignHCenter);

    stack_->addWidget(menu_);
    stack_->addWidget(prepare_);
    stack_->addWidget(settings_);
    stack_->addWidget(game_);

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
            Audio::instance().startBgmLoop();
        }
    });
    connect(bgmSlider_, &QSlider::valueChanged, this, [this](int value) {
        bgmVolumeLabel_->setText(QString("%1%").arg(value));
        saveSettingsUi();
    });
    connect(quit, &QPushButton::clicked, this, [] { QApplication::quit(); });
    connect(game_, &GameWidget::returnedToMenu, this, [this] { showMenu(); });
    connect(classGroup_, &QButtonGroup::idClicked, this, [this](int) {
        refreshPrepareSkills();
        updatePrepareArtHighlight();
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

void MainWindow::refreshPrepareSkills() {
    auto line = [](const QString& key, const SkillText& text) {
        return QString("<p style='margin:4px 0;'><span style='color:#c45c48;'>%1</span>　<b>%2</b><br>%3</p>")
            .arg(key, text.name, text.detail);
    };
    QString html;
    html += line("Q", guardSkillText());
    html += line("E", healSkillText());
    const HeroClass hero = HeroClass(classGroup_->checkedId());
    const bool mage = hero == HeroClass::Mage;
    warriorPickRow_->setVisible(!mage);
    magePickRow_->setVisible(mage);
    if (mage) {
        skillHint_->setText("Q 防御　E 恢复　法师技能栏：R / F / C / V（自选，不可重复）");
        for (int i = 0; i < 4; ++i) {
            html += line(kSlotKeys[i], skillText(mageSkillBoxes_[i]->currentData().toInt()));
        }
        html += "<p style='margin:8px 0 0 0; color:#a89888;'>可选技能共六个，带走其中四个。</p>";
    } else {
        const char* role = hero == HeroClass::Sword ? "剑客" : "战士";
        skillHint_->setText(QString("Q 防御　E 恢复　%1技能栏：R / F / C（自选，不可重复）").arg(role));
        for (int i = 0; i < 3; ++i) {
            html += line(kSlotKeys[i], skillText(warriorSkillBoxes_[i]->currentData().toInt()));
        }
        html += "<p style='margin:8px 0 0 0; color:#a89888;'>回旋斩 / 剑气 / 突刺 / 狂化，四选三分配到三个键位。</p>";
    }
    skillDetail_->setText(html);
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

    // 布局图：女剑士左下；战士右上角、法师右下角缩小贴边；菜单居中。
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
}

void MainWindow::updatePrepareArtHighlight() {
    const HeroClass hero = classGroup_ ? HeroClass(classGroup_->checkedId()) : HeroClass::Warrior;
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
    if (!classGroup_) {
        return;
    }
    if (QAbstractButton* button = classGroup_->button(int(hero))) {
        button->setChecked(true);
    }
    refreshPrepareSkills();
    updatePrepareArtHighlight();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
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
    sfxCheck_->setChecked(settings.sfxEnabled);
    sfxSlider_->setValue(settings.sfxVolume);
    sfxSlider_->setEnabled(settings.sfxEnabled);
    sfxVolumeLabel_->setText(QString("%1%").arg(settings.sfxVolume));
    bgmCheck_->setChecked(settings.bgmEnabled);
    bgmSlider_->setValue(settings.bgmVolume);
    bgmSlider_->setEnabled(settings.bgmEnabled);
    bgmVolumeLabel_->setText(QString("%1%").arg(settings.bgmVolume));
    Audio::instance().setSfxEnabled(settings.sfxEnabled);
    Audio::instance().setSfxVolume(settings.sfxVolume);
    Audio::instance().setBgmEnabled(settings.bgmEnabled);
    Audio::instance().setBgmVolume(settings.bgmVolume);
}

void MainWindow::saveSettingsUi() {
    AppSettings settings;
    settings.sfxEnabled = sfxCheck_->isChecked();
    settings.sfxVolume = sfxSlider_->value();
    settings.bgmEnabled = bgmCheck_->isChecked();
    settings.bgmVolume = bgmSlider_->value();
    Audio::instance().setSfxEnabled(settings.sfxEnabled);
    Audio::instance().setSfxVolume(settings.sfxVolume);
    Audio::instance().setBgmEnabled(settings.bgmEnabled);
    Audio::instance().setBgmVolume(settings.bgmVolume);
    Storage::saveSettings(settings);
}

void MainWindow::startPrepared() {
    const HeroClass hero = HeroClass(classGroup_->checkedId());
    if (hero == HeroClass::Mage) {
        game_->startNew(hero,
            mageSkillBoxes_[0]->currentData().toInt(),
            mageSkillBoxes_[1]->currentData().toInt(),
            mageSkillBoxes_[2]->currentData().toInt(),
            mageSkillBoxes_[3]->currentData().toInt());
    } else {
        game_->startNew(hero,
            warriorSkillBoxes_[0]->currentData().toInt(),
            warriorSkillBoxes_[1]->currentData().toInt(),
            warriorSkillBoxes_[2]->currentData().toInt());
    }
    stack_->setCurrentWidget(game_);
}

void MainWindow::continueGame() {
    QJsonObject game;
    if (!Storage::loadContinue(game)) {
        QMessageBox::warning(this, "继续", "没有可用存档。");
        return;
    }
    game_->startContinue(game);
    stack_->setCurrentWidget(game_);
}

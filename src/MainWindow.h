#pragma once

#include "Session.h"

#include <QPixmap>
#include <QWidget>
#include <array>

class GameWidget;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGraphicsOpacityEffect;
class QGridLayout;
class QLabel;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTextBrowser;
class QWidget;

class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void showMenu();
    void showPrepare();
    void showSettings();
    void startPrepared();
    void continueGame();
    void refreshMenu();
    void refreshPrepareSkills();
    void loadSettingsUi();
    void saveSettingsUi();
    void fillSkillBox(QComboBox* box, const int* pool, int poolSize, int selected);
    QPushButton* makeSkillPickButton(QComboBox* box, QWidget* parent);
    void openSkillPicker(QComboBox* box);
    void onWarriorSkillPicked(int slot);
    void onMageSkillPicked(int slot);
    void layoutMenuBackground();
    void layoutMenuTitle();
    void layoutPrepareArt();
    void updatePrepareArtHighlight();
    void selectHeroClass(HeroClass hero);

    QStackedWidget* stack_ = nullptr;
    QWidget* menu_ = nullptr;
    QLabel* menuBg_ = nullptr;
    QLabel* menuTitle_ = nullptr;
    QWidget* menuContent_ = nullptr;
    QPixmap menuBgPix_;
    QWidget* prepare_ = nullptr;
    QLabel* prepareWarriorArt_ = nullptr;
    QLabel* prepareMageArt_ = nullptr;
    QLabel* prepareSwordArt_ = nullptr;
    QWidget* prepareContent_ = nullptr;
    QPixmap prepareWarriorPix_;
    QPixmap prepareMagePix_;
    QPixmap prepareSwordPix_;
    QGraphicsOpacityEffect* prepareWarriorOpacity_ = nullptr;
    QGraphicsOpacityEffect* prepareMageOpacity_ = nullptr;
    QGraphicsOpacityEffect* prepareSwordOpacity_ = nullptr;
    QWidget* settings_ = nullptr;
    GameWidget* game_ = nullptr;
    QLabel* recordLabel_ = nullptr;
    QPushButton* continueButton_ = nullptr;
    QCheckBox* sfxCheck_ = nullptr;
    QSlider* sfxSlider_ = nullptr;
    QLabel* sfxVolumeLabel_ = nullptr;
    QCheckBox* bgmCheck_ = nullptr;
    QSlider* bgmSlider_ = nullptr;
    QLabel* bgmVolumeLabel_ = nullptr;
    QButtonGroup* classGroup_ = nullptr;
    QTextBrowser* skillDetail_ = nullptr;
    QWidget* warriorPickRow_ = nullptr;
    QWidget* magePickRow_ = nullptr;
    QLabel* skillHint_ = nullptr;
    QLabel* glossary_ = nullptr;
    std::array<QPushButton*, 3> warriorSkillButtons_{};
    std::array<QPushButton*, 4> mageSkillButtons_{};
    QWidget* skillPicker_ = nullptr;
    QLabel* skillPickerTitle_ = nullptr;
    QGridLayout* skillPickerGrid_ = nullptr;
    std::array<QComboBox*, 3> warriorSkillBoxes_{};
    std::array<int, 3> warriorSkillPrev_{
        kSkillSpin, kSkillSwordQi, kSkillThrust};
    std::array<QComboBox*, 4> mageSkillBoxes_{};
    std::array<int, 4> mageSkillPrev_{
        kSkillMageBolt, kSkillNova, kSkillFlight, kSkillBurial};
};

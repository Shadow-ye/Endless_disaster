#pragma once

#include "Session.h"

#include <QPixmap>
#include <QStringList>
#include <QWidget>
#include <array>
#include <functional>

class GameWidget;
class LoadingWidget;
class QCheckBox;
class QComboBox;
class QGraphicsOpacityEffect;
class QGridLayout;
class QLabel;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTextBrowser;
class QTimer;
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
    // 开局加载页：把准备步骤摊到多帧执行，参数先记在这里
    void startLoadingNewRun(HeroClass hero, const std::array<int, 4>& skills);
    void startLoadingContinue(const QJsonObject& game);
    void beginLoading();
    void advanceLoading();
    void runLoadingStep(int step);
    void finishLoading();
    int loadingStepCount() const;
    void refreshMenu();
    void refreshPrepareSkills();
    void loadSettingsUi();
    void saveSettingsUi();
    void fillSkillBox(QComboBox* box, const int* pool, int poolSize, int selected);
    QPushButton* makeSkillPickButton(QComboBox* box, QWidget* parent);
    void openPicker(const QString& title, const QStringList& options, int current, int columns,
        const std::function<void(int)>& onPick);
    void openSkillPicker(QComboBox* box);
    void openHeroPicker();
    void onWarriorSkillPicked(int slot);
    void onMageSkillPicked(int slot);
    void onRobotSkillPicked(int slot);
    void layoutMenuBackground();
    void layoutMenuTitle();
    void layoutPrepareArt();
    void updatePrepareArtHighlight();
    void selectHeroClass(HeroClass hero);

    // 进入本局前的加载页：新开一局与继续存档都要先过它
    enum class LoadingKind { NewRun, Continue };
    LoadingWidget* loading_ = nullptr;
    QTimer* loadingTimeout_ = nullptr;
    LoadingKind loadingKind_ = LoadingKind::NewRun;
    HeroClass loadingHero_ = HeroClass::Warrior;
    std::array<int, 4> loadingSkills_{0, 1, 2, -1};
    QJsonObject loadingSave_;
    int loadingStep_ = 0;

    QStackedWidget* stack_ = nullptr;
    QWidget* menu_ = nullptr;
    QLabel* menuBg_ = nullptr;
    QLabel* menuTitle_ = nullptr;
    QWidget* menuContent_ = nullptr;
    QWidget* menuLinks_ = nullptr;
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
    QCheckBox* guideCheck_ = nullptr;
    HeroClass hero_ = HeroClass::Warrior;
    QPushButton* heroButton_ = nullptr;
    QLabel* heroStats_ = nullptr;
    QTextBrowser* skillDetail_ = nullptr;
    QWidget* warriorPickRow_ = nullptr;
    QWidget* magePickRow_ = nullptr;
    QWidget* robotPickRow_ = nullptr;
    QLabel* skillHint_ = nullptr;
    QLabel* glossary_ = nullptr;
    std::array<QPushButton*, 3> warriorSkillButtons_{};
    std::array<QPushButton*, 4> mageSkillButtons_{};
    std::array<QPushButton*, 3> robotSkillButtons_{};
    QWidget* picker_ = nullptr;
    QLabel* pickerTitle_ = nullptr;
    QGridLayout* pickerGrid_ = nullptr;
    std::array<QComboBox*, 3> warriorSkillBoxes_{};
    std::array<int, 3> warriorSkillPrev_{
        kSkillSpin, kSkillSwordQi, kSkillThrust};
    std::array<QComboBox*, 4> mageSkillBoxes_{};
    std::array<int, 4> mageSkillPrev_{
        kSkillMageBolt, kSkillNova, kSkillFlight, kSkillBurial};
    std::array<QComboBox*, 3> robotSkillBoxes_{};
    std::array<int, 3> robotSkillPrev_{
        kSkillScatter, kSkillMissile, kSkillBoost};
};

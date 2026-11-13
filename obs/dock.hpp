#pragma once
#include "camera_worker.hpp"
#include "scene_rules.hpp"
#include "scene_matching.hpp"
#include <QWidget>
#include <QTimer>
#include <QJsonObject>
#include <atomic>
#include <memory>
#include <map>
class QComboBox;
class QDoubleSpinBox;
class QSlider;
class QCheckBox;
class QLabel;
class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QGridLayout;
class QLineEdit;
namespace tinyobs {
struct Scene { std::string uuid; QString name; };
struct Host {
    std::function<std::vector<Scene>()> scenes;
    std::function<Scene()> currentScene;
    std::function<std::string()> collection;
};
class Dock : public QWidget {
  public:
    Dock(std::filesystem::path config, std::filesystem::path bundledPresets,
         Host host, bool discover = true, bool standalone = false);
    ~Dock() override;
    void frontendReady();
    void sceneChanged();
    void refreshScenes();
    void collectionChanging();
    void shutdown();
    // Worker results are presented only on the UI thread. Also used by offline UI tests.
    void presentSettings(std::vector<ptz::Setting> settings);
    void presentPosition(ptz::Position position, std::array<ptz::Range, 3> ranges);
    void setPreviewEnabled(bool enabled, HWND owner, int width, int height);
    void resizePreview(int width, int height);
    void setThemePreference(const QString &theme);
    QString themePreference() const { return theme_; }
    std::wstring selectedDevice() const { return device(); }
    void setPreviewStatus(const QString &message, bool error = false) { status(message, error); }
    std::function<void(bool, const QString &)> connectionChanged;
    std::function<void(bool, const QString &)> previewChanged;
  private:
    struct Numeric {
        QSlider *slider = nullptr;
        QDoubleSpinBox *number = nullptr;
        double low = 0, step = 1;
        int ticks = 0;
        bool numberEdited = false;
    };
    struct Edit { ptz::SettingValue value; bool final = false; qint64 changed = 0, sent = -1000; bool dirty = true; };
    Host host_;
    std::filesystem::path config_;
    std::unique_ptr<ptz::PresetStore> store_;
    SceneRules rules_;
    std::unique_ptr<CameraWorker> worker_;
    std::shared_ptr<std::atomic<unsigned long long>> sceneGeneration_;
    std::map<std::string, Edit> edits_;
    std::vector<ptz::Setting> settings_;
    QTimer liveTimer_;
    QTimer positionTimer_, reconnectTimer_;
    QComboBox *devices_, *choices_, *scenes_, *scenePresets_;
    QCheckBox *automatic_, *includeSettings_, *sceneEnabled_;
    QLabel *status_, *details_, *sceneStatus_;
    QListWidget *presets_;
    QTreeWidget *settingList_;
    QPushButton *apply_, *default_;
    QCheckBox *autoConnect_ = nullptr;
    QLineEdit *search_ = nullptr;
    bool standalone_ = false, connected_ = false, connecting_ = false, pollPending_ = false;
    std::wstring lastDevice_;
    QString theme_ = "system";
    std::array<ptz::Range, 3> positionRanges_{};
    unsigned long long positionRevision_ = 0;
    Numeric axes_[3], numeric_;
    bool updating_ = false, ready_ = false, stopped_ = false;
    unsigned long long deviceRevision_ = 0;
    std::wstring device() const;
    std::optional<ptz::Preset> selectedPreset() const;
    void post(std::function<void()> work);
    void status(const QString &text, bool error = false);
    void run(std::string key, CameraWorker::Work work);
    void discover();
    void readCamera();
    void pollPosition();
    void filterPresets();
    void readSettings();
    Numeric addNumeric(QGridLayout *layout, int row, const QString &name,
                       const QString &objectName, QWidget *parent);
    void configure(Numeric &numeric, double low, double high, double step, double value,
                   bool enabled);
    void selectSetting();
    void stageSetting(bool final);
    void flushLive(bool force = false);
    void stageAxis(int index, bool final);
    void refreshPresets();
    void transferPresets(bool importing);
    void transferConfiguration(bool importing);
    void assignPresetCamera();
    void capturePreset(bool replace);
    void recall(const ptz::Preset &preset, bool scene = false);
    void saveConfig();
    void loadConfig(const std::filesystem::path &bundledPresets);
    void selectScene();
    void assignScene();
    SceneCatalog sceneCatalog() const;
};
} // namespace tinyobs

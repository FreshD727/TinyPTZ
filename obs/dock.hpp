#pragma once
#include "camera_worker.hpp"
#include "scene_rules.hpp"
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
         Host host, bool discover = true);
    ~Dock() override;
    void frontendReady();
    void sceneChanged();
    void refreshScenes();
    void collectionChanging();
    void shutdown();
    // Worker results are presented only on the UI thread. Also used by offline UI tests.
    void presentSettings(std::vector<ptz::Setting> settings);
    void presentPosition(ptz::Position position, std::array<ptz::Range, 3> ranges);
  private:
    struct Numeric {
        QSlider *slider = nullptr;
        QDoubleSpinBox *number = nullptr;
        double low = 0, step = 1;
        int ticks = 0;
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
    QComboBox *devices_, *choices_, *scenes_, *scenePresets_;
    QCheckBox *automatic_, *includeSettings_, *sceneEnabled_;
    QLabel *status_, *details_, *path_, *sceneStatus_;
    QListWidget *presets_;
    QTreeWidget *settingList_;
    QPushButton *apply_, *default_;
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
    void openPresets();
    void capturePreset(bool replace);
    void recall(const ptz::Preset &preset, bool scene = false);
    void saveConfig();
    void loadConfig(const std::filesystem::path &bundledPresets);
    void selectScene();
    void assignScene();
};
} // namespace tinyobs

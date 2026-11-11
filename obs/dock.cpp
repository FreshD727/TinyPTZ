#include "dock.hpp"
#include "preset_transfer.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>

namespace tinyobs {
static QString qs(const std::wstring &text) { return QString::fromStdWString(text); }
static QString qs(const std::string &text) { return QString::fromUtf8(text.c_str()); }
static std::string bytes(const QString &text) { return text.toUtf8().toStdString(); }
static QString fileName(const std::filesystem::path &path) { return qs(path.wstring()); }
static PresetId id(const ptz::Preset &p) { return {p.device, p.name}; }
static constexpr long properties[] = {CameraControl_Pan, CameraControl_Tilt, CameraControl_Zoom};

Dock::Dock(std::filesystem::path config, std::filesystem::path bundledPresets,
           Host host, bool startDiscovery)
    : host_(std::move(host)), config_(std::move(config)),
      sceneGeneration_(std::make_shared<std::atomic<unsigned long long>>(0)) {
    setObjectName("tinyPtzDock");
    setMinimumSize(280, 320);
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    auto connection = new QGridLayout;
    devices_ = new QComboBox(this);
    devices_->setObjectName("cameraDevice");
    devices_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    connection->addWidget(devices_, 0, 0, 1, 2);
    auto rescan = new QPushButton("Rescan", this);
    connection->addWidget(rescan, 0, 2);
    auto read = new QPushButton("Connect / read", this);
    read->setObjectName("connectCamera");
    auto wake = new QPushButton("Wake", this);
    auto sleep = new QPushButton("Sleep", this);
    connection->addWidget(read, 1, 0);
    connection->addWidget(wake, 1, 1);
    connection->addWidget(sleep, 1, 2);
    outer->addLayout(connection);
    auto tabs = new QTabWidget(this);
    tabs->setObjectName("controlTabs");
    outer->addWidget(tabs, 1);
    auto ptzPage = new QWidget;
    auto ptzLayout = new QVBoxLayout(ptzPage);
    auto grid = new QGridLayout;
    for (int axis = 0; axis < 3; ++axis) {
        const QString names[] = {"Pan", "Tilt", "Zoom"};
        axes_[axis] = addNumeric(grid, axis, names[axis], names[axis].toLower(), ptzPage);
        configure(axes_[axis], 0, 0, 1, 0, false);
        QObject::connect(axes_[axis].slider, &QSlider::valueChanged, this, [this, axis](int value) {
            if (updating_) return;
            QSignalBlocker block(axes_[axis].number);
            axes_[axis].number->setValue(axes_[axis].low + value * axes_[axis].step);
            stageAxis(axis, false);
        });
        QObject::connect(axes_[axis].slider, &QSlider::sliderReleased, this,
                         [this, axis] { stageAxis(axis, true); flushLive(); });
        QObject::connect(axes_[axis].number, &QDoubleSpinBox::valueChanged, this,
                         [this, axis](double value) {
            QSignalBlocker block(axes_[axis].slider);
            axes_[axis].slider->setValue((int)std::lround((value - axes_[axis].low) / axes_[axis].step));
        });
        QObject::connect(axes_[axis].number, &QDoubleSpinBox::editingFinished, this,
                         [this, axis] { if (!updating_) { stageAxis(axis, true); flushLive(); } });
    }
    ptzLayout->addLayout(grid);
    auto readPosition = new QPushButton("Read position", ptzPage);
    ptzLayout->addWidget(readPosition);
    presets_ = new QListWidget(ptzPage);
    presets_->setObjectName("presets");
    ptzLayout->addWidget(presets_, 1);
    includeSettings_ = new QCheckBox("Save camera settings with presets", ptzPage);
    includeSettings_->setToolTip("Include tracking, image and device settings when capturing a preset.");
    includeSettings_->setChecked(true);
    ptzLayout->addWidget(includeSettings_);
    auto presetButtons = new QGridLayout;
    auto recallButton = new QPushButton("Load preset", ptzPage);
    auto capture = new QPushButton("Save new", ptzPage);
    auto replace = new QPushButton("Update selected", ptzPage);
    auto rename = new QPushButton("Rename", ptzPage);
    auto remove = new QPushButton("Delete", ptzPage);
    auto boot = new QPushButton("Set boot position", ptzPage);
    auto open = new QPushButton("Open preset file...", ptzPage);
    auto reload = new QPushButton("Reload file", ptzPage);
    auto importButton = new QPushButton("Import presets...", ptzPage);
    importButton->setObjectName("importPresets");
    importButton->setToolTip("Merge a presets.ptz file into this library. Duplicate names get a numbered suffix.");
    auto exportButton = new QPushButton("Export presets...", ptzPage);
    exportButton->setObjectName("exportPresets");
    exportButton->setToolTip("Save a copy of all presets, including their camera settings, to a .ptz file.");
    presetButtons->addWidget(recallButton, 0, 0);
    presetButtons->addWidget(capture, 0, 1);
    presetButtons->addWidget(replace, 1, 0);
    presetButtons->addWidget(rename, 1, 1);
    presetButtons->addWidget(remove, 2, 0);
    presetButtons->addWidget(boot, 2, 1);
    presetButtons->addWidget(open, 3, 0);
    presetButtons->addWidget(reload, 3, 1);
    presetButtons->addWidget(importButton, 4, 0);
    presetButtons->addWidget(exportButton, 4, 1);
    ptzLayout->addLayout(presetButtons);
    path_ = new QLabel(ptzPage);
    path_->setWordWrap(true);
    path_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    path_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ptzLayout->addWidget(path_);
    auto ptzScroll = new QScrollArea;
    ptzScroll->setWidgetResizable(true);
    ptzScroll->setWidget(ptzPage);
    tabs->addTab(ptzScroll, "PTZ / presets");

    auto settingsPage = new QWidget;
    auto settingsLayout = new QVBoxLayout(settingsPage);
    auto readSettingsButton = new QPushButton("Read camera settings", settingsPage);
    settingsLayout->addWidget(readSettingsButton);
    settingList_ = new QTreeWidget(settingsPage);
    settingList_->setObjectName("cameraSettings");
    settingList_->setHeaderLabels({"Setting", "Value"});
    settingList_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    settingList_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    settingList_->setRootIsDecorated(true);
    settingsLayout->addWidget(settingList_, 1);
    details_ = new QLabel("Connect and read the camera to edit its settings.", settingsPage);
    details_->setWordWrap(true);
    settingsLayout->addWidget(details_);
    auto editGrid = new QGridLayout;
    numeric_ = addNumeric(editGrid, 0, "Value", "setting", settingsPage);
    configure(numeric_, 0, 0, 1, 0, false);
    settingsLayout->addLayout(editGrid);
    choices_ = new QComboBox(settingsPage);
    choices_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    choices_->setObjectName("settingChoice");
    settingsLayout->addWidget(choices_);
    automatic_ = new QCheckBox("Automatic", settingsPage);
    automatic_->setObjectName("settingAutomatic");
    settingsLayout->addWidget(automatic_);
    auto actions = new QGridLayout;
    apply_ = new QPushButton("Apply selected setting", settingsPage);
    default_ = new QPushButton("Default selected", settingsPage);
    auto defaults = new QPushButton("Restore image defaults", settingsPage);
    actions->addWidget(apply_, 0, 0);
    actions->addWidget(default_, 0, 1);
    actions->addWidget(defaults, 1, 0, 1, 2);
    settingsLayout->addLayout(actions);
    auto settingsScroll = new QScrollArea;
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setWidget(settingsPage);
    tabs->addTab(settingsScroll, "Settings");

    auto scenePage = new QWidget;
    auto sceneLayout = new QVBoxLayout(scenePage);
    sceneEnabled_ = new QCheckBox("Enable scene preset automation", scenePage);
    sceneEnabled_->setToolTip("Load assigned presets when the active Program scene changes.");
    sceneEnabled_->setObjectName("sceneAutomation");
    sceneLayout->addWidget(sceneEnabled_);
    auto explanation = new QLabel("Assignments apply to this scene collection. Preview selection in Studio Mode does not move the camera. Unassigned scenes leave it alone.", scenePage);
    explanation->setWordWrap(true);
    sceneLayout->addWidget(explanation);
    sceneLayout->addWidget(new QLabel("Scene", scenePage));
    scenes_ = new QComboBox(scenePage);
    scenes_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    scenes_->setObjectName("sceneSelection");
    sceneLayout->addWidget(scenes_);
    sceneLayout->addWidget(new QLabel("Preset", scenePage));
    scenePresets_ = new QComboBox(scenePage);
    scenePresets_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    scenePresets_->setObjectName("scenePreset");
    sceneLayout->addWidget(scenePresets_);
    auto assign = new QPushButton("Save scene assignment", scenePage);
    assign->setObjectName("saveSceneAssignment");
    sceneLayout->addWidget(assign);
    sceneStatus_ = new QLabel(scenePage);
    sceneStatus_->setWordWrap(true);
    sceneLayout->addWidget(sceneStatus_);
    sceneLayout->addStretch();
    tabs->addTab(scenePage, "Scenes");
    status_ = new QLabel("Ready", this);
    status_->setObjectName("cameraStatus");
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outer->addWidget(status_);

    worker_ = std::make_unique<CameraWorker>([this](const std::string &message) {
        post([this, message] {
            edits_.clear(); liveTimer_.stop(); worker_->cancel("live:");
            status(qs(message), true);
        });
    });
    liveTimer_.setInterval(25);
    QObject::connect(&liveTimer_, &QTimer::timeout, this, [this] { flushLive(); });
    QObject::connect(rescan, &QPushButton::clicked, this, [this] { discover(); });
    QObject::connect(read, &QPushButton::clicked, this, [this] { readCamera(); });
    QObject::connect(readPosition, &QPushButton::clicked, this, [this] { readCamera(); });
    QObject::connect(readSettingsButton, &QPushButton::clicked, this, [this] { readSettings(); });
    QObject::connect(devices_, &QComboBox::currentIndexChanged, this, [this] {
        ++deviceRevision_;
        edits_.clear(); liveTimer_.stop();
        // Device-specific pending edits must not spill into a newly selected camera.
        worker_->cancel("live:");
        presentSettings({});
        for (auto &axis : axes_) configure(axis, 0, 0, 1, 0, false);
    });
    auto power = [this](bool awake) {
        flushLive(true);
        auto camera = device();
        run("power", [this, camera, awake](ptz::Camera &c) {
            tinyobs::connect(c, camera); c.wake(awake, true);
            post([this, awake] { status(awake ? "Camera awake." : "Camera sleeping."); });
        });
    };
    QObject::connect(wake, &QPushButton::clicked, this, [power] { power(true); });
    QObject::connect(sleep, &QPushButton::clicked, this, [power] { power(false); });
    QObject::connect(recallButton, &QPushButton::clicked, this, [this] {
        if (auto p = selectedPreset()) recall(*p);
    });
    QObject::connect(presets_, &QListWidget::itemDoubleClicked, this, [this] {
        if (auto p = selectedPreset()) recall(*p);
    });
    QObject::connect(capture, &QPushButton::clicked, this, [this] { capturePreset(false); });
    QObject::connect(replace, &QPushButton::clicked, this, [this] { capturePreset(true); });
    QObject::connect(open, &QPushButton::clicked, this, [this] { openPresets(); });
    QObject::connect(importButton, &QPushButton::clicked, this, [this] { transferPresets(true); });
    QObject::connect(exportButton, &QPushButton::clicked, this, [this] { transferPresets(false); });
    QObject::connect(reload, &QPushButton::clicked, this, [this] {
        try { store_->load(); refreshPresets(); status("Preset file reloaded."); }
        catch (const std::exception &e) { status(qs(std::string(e.what())), true); }
    });
    QObject::connect(rename, &QPushButton::clicked, this, [this] {
        auto p = selectedPreset(); if (!p) return;
        bool ok = false;
        auto name = QInputDialog::getText(this, "Rename preset", "Name", QLineEdit::Normal, qs(p->name), &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        auto replacement = *p; replacement.name = name.toStdWString();
        if (std::any_of(store_->items.begin(), store_->items.end(), [&](auto &v) {
            return id(v) == id(replacement) && !(id(v) == id(*p));
        })) { status("This camera already has a preset with that name.", true); return; }
        try {
            ptz::PresetStore next(store_->path()); next.load();
            auto found = std::find_if(next.items.begin(), next.items.end(), [&](auto &v) { return id(v) == id(*p); });
            if (found == next.items.end()) throw std::runtime_error("Preset changed externally. Reload the file.");
            found->name = replacement.name; next.save(); store_->items = next.items;
            rules_.rename(id(*p), id(replacement)); saveConfig(); refreshPresets();
        } catch (const std::exception &e) { status(qs(std::string(e.what())), true); }
    });
    QObject::connect(remove, &QPushButton::clicked, this, [this] {
        auto p = selectedPreset(); if (!p) return;
        if (QMessageBox::question(this, "Delete preset", "Delete " + qs(p->name) + " and its scene assignments?") != QMessageBox::Yes) return;
        try {
            ptz::PresetStore next(store_->path()); next.load();
            next.items.erase(std::remove_if(next.items.begin(), next.items.end(), [&](auto &v) { return id(v) == id(*p); }), next.items.end());
            next.save(); store_->items = next.items;
            rules_.remove(id(*p)); saveConfig(); refreshPresets();
        } catch (const std::exception &e) { status(qs(std::string(e.what())), true); }
    });
    QObject::connect(boot, &QPushButton::clicked, this, [this] {
        auto p = selectedPreset(); if (!p) return;
        flushLive(true);
        run("boot", [this, p = *p](ptz::Camera &c) {
            tinyobs::connect(c, p.device); c.setBootPreset(p);
            post([this] { status("Boot PTZ position stored in the camera and verified."); });
        });
    });
    QObject::connect(settingList_, &QTreeWidget::currentItemChanged, this, [this] { selectSetting(); });
    QObject::connect(numeric_.slider, &QSlider::valueChanged, this, [this](int index) {
        if (updating_) return;
        QSignalBlocker block(numeric_.number);
        numeric_.number->setValue(numeric_.low + index * numeric_.step);
        stageSetting(false);
    });
    QObject::connect(numeric_.slider, &QSlider::sliderReleased, this, [this] { stageSetting(true); flushLive(); });
    QObject::connect(numeric_.number, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        QSignalBlocker block(numeric_.slider);
        numeric_.slider->setValue((int)std::lround((value - numeric_.low) / numeric_.step));
    });
    QObject::connect(apply_, &QPushButton::clicked, this, [this] { stageSetting(true); flushLive(true); });
    QObject::connect(automatic_, &QCheckBox::toggled, this, [this](bool autoValue) {
        if (updating_) return;
        auto item = settingList_->currentItem();
        int index = item ? item->data(0, Qt::UserRole).toInt() : -1;
        if (index < 0 || index >= (int)settings_.size()) return;
        auto &s = settings_[index];
        numeric_.slider->setEnabled(s.writable && !autoValue && numeric_.ticks > 0);
        numeric_.number->setEnabled(s.writable && !autoValue);
        choices_->setEnabled(s.writable && !autoValue);
    });
    QObject::connect(default_, &QPushButton::clicked, this, [this] {
        auto item = settingList_->currentItem();
        int index = item ? item->data(0, Qt::UserRole).toInt() : -1;
        if (index < 0 || index >= (int)settings_.size() || !settings_[index].defaultAvailable) return;
        auto value = settings_[index].defaultValue;
        edits_[value.key] = {value, true, QDateTime::currentMSecsSinceEpoch(), -1000};
        flushLive(true);
        readSettings();
    });
    QObject::connect(defaults, &QPushButton::clicked, this, [this] {
        flushLive(true); auto camera = device(); auto revision = deviceRevision_;
        run("defaults", [this, camera, revision](ptz::Camera &c) {
            tinyobs::connect(c, camera); c.restoreImageDefaults(); auto values = c.settings();
            post([this, values, revision] {
                if (revision != deviceRevision_) return;
                presentSettings(values); status("Image defaults restored.");
            });
        });
    });
    QObject::connect(sceneEnabled_, &QCheckBox::toggled, this, [this](bool checked) {
        rules_.enabled = checked; ++*sceneGeneration_; worker_->cancel("recall");
        saveConfig(); if (checked) sceneChanged();
    });
    QObject::connect(scenes_, &QComboBox::currentIndexChanged, this, [this] { selectScene(); });
    QObject::connect(assign, &QPushButton::clicked, this, [this] { assignScene(); });
    loadConfig(bundledPresets);
    refreshPresets(); refreshScenes();
    if (startDiscovery) discover();
}

Dock::~Dock() { shutdown(); }
void Dock::post(std::function<void()> work) {
    QMetaObject::invokeMethod(this, [this, work = std::move(work)] { if (!stopped_) work(); }, Qt::QueuedConnection);
}
void Dock::status(const QString &text, bool error) {
    status_->setText((error ? "Error: " : "") + text);
    status_->setStyleSheet(error ? "color: #e87878;" : "");
}
void Dock::run(std::string key, CameraWorker::Work work) {
    if (stopped_) return;
    status("Working...");
    worker_->submit(std::move(key), std::move(work));
}
std::wstring Dock::device() const { return devices_->currentData().toString().toStdWString(); }
Dock::Numeric Dock::addNumeric(QGridLayout *layout, int row, const QString &name,
                               const QString &objectName, QWidget *parent) {
    Numeric n;
    n.slider = new QSlider(Qt::Horizontal, parent);
    n.slider->setObjectName(objectName + "Slider");
    n.slider->setMinimumWidth(60);
    n.number = new QDoubleSpinBox(parent);
    n.number->setObjectName(objectName + "Number");
    n.number->setKeyboardTracking(false);
    n.number->setMinimumWidth(72);
    layout->addWidget(new QLabel(name, parent), row, 0);
    layout->addWidget(n.slider, row, 1);
    layout->addWidget(n.number, row, 2);
    layout->setColumnStretch(1, 1);
    return n;
}
void Dock::configure(Numeric &n, double low, double high, double step, double value, bool enabled) {
    QSignalBlocker a(n.slider), b(n.number);
    double count = (high - low) / step;
    bool valid = std::isfinite(count) && step > 0 && count >= 0 && count < INT_MAX;
    n.low = low; n.step = valid ? step : 1;
    n.ticks = valid ? (int)std::floor(count + 1e-7) : 0;
    n.slider->setRange(0, n.ticks);
    n.slider->setPageStep(std::max(1, n.ticks / 10));
    n.slider->setValue(valid ? (int)std::lround(std::clamp((value - low) / step, 0., (double)n.ticks)) : 0);
    n.slider->setEnabled(enabled && valid && n.ticks > 0);
    int decimals = 0;
    while (decimals < 6 && std::abs(step * std::pow(10., decimals) - std::round(step * std::pow(10., decimals))) > 1e-7) ++decimals;
    n.number->setDecimals(decimals);
    n.number->setRange(low, high);
    n.number->setSingleStep(n.step);
    n.number->setValue(value);
    n.number->setEnabled(enabled && valid);
}
void Dock::discover() {
    auto previous = device();
    run("devices", [this, previous](ptz::Camera &) {
        auto list = ptz::devices();
        post([this, list, previous] {
            devices_->clear();
            for (auto &d : list) devices_->addItem(qs(d.name), qs(d.id));
            int index = devices_->findData(qs(previous));
            if (index >= 0) devices_->setCurrentIndex(index);
            status(list.empty() ? "No camera found. Connect the Tiny SE, then rescan." : "Select a camera and click Connect / read.");
        });
    });
}
void Dock::readCamera() {
    flushLive(true); auto camera = device(); auto revision = deviceRevision_;
    run("read", [this, camera, revision](ptz::Camera &c) {
        tinyobs::connect(c, camera);
        auto p = c.read(); std::array<ptz::Range, 3> ranges;
        for (int i = 0; i < 3; ++i) ranges[i] = c.range(properties[i]);
        auto settings = c.settings();
        post([this, p, ranges, settings, revision] {
            if (revision != deviceRevision_) return;
            presentPosition(p, ranges); presentSettings(settings); status("Camera connected. Sliders apply live.");
        });
    });
}
void Dock::presentPosition(ptz::Position p, std::array<ptz::Range, 3> ranges) {
    long values[] = {p.pan, p.tilt, p.zoom};
    for (int i = 0; i < 3; ++i)
        configure(axes_[i], ranges[i].min, ranges[i].max, ranges[i].step,
                  values[i], ranges[i].supported);
}
void Dock::readSettings() {
    flushLive(true); auto camera = device(); auto revision = deviceRevision_;
    run("settings", [this, camera, revision](ptz::Camera &c) {
        tinyobs::connect(c, camera); auto values = c.settings();
        post([this, values, revision] {
            if (revision != deviceRevision_) return;
            presentSettings(values); status("Settings read from camera.");
        });
    });
}
void Dock::presentSettings(std::vector<ptz::Setting> values) {
    QString selected;
    if (auto item = settingList_->currentItem()) selected = item->data(1, Qt::UserRole).toString();
    settings_ = std::move(values);
    QSignalBlocker block(settingList_);
    settingList_->clear();
    std::map<std::wstring, QTreeWidgetItem *> groups;
    QTreeWidgetItem *chosen = nullptr;
    for (int i = 0; i < (int)settings_.size(); ++i) {
        auto &s = settings_[i];
        if (!groups.count(s.group)) {
            auto group = new QTreeWidgetItem(settingList_, {qs(s.group)});
            group->setData(0, Qt::UserRole, -1);
            group->setExpanded(true); groups[s.group] = group;
        }
        QString current = s.readable ? (s.current.automatic ? "Auto" : QString::number(s.current.value, 'g', 9)) : "Set only";
        if (!s.writable && !s.readable) current = "Unavailable";
        auto item = new QTreeWidgetItem(groups[s.group], {qs(s.name), current});
        item->setData(0, Qt::UserRole, i);
        item->setData(1, Qt::UserRole, qs(s.current.key));
        if (selected == qs(s.current.key)) chosen = item;
        if (!chosen && i == 0) chosen = item;
    }
    settingList_->setCurrentItem(chosen);
    selectSetting();
}
void Dock::selectSetting() {
    updating_ = true;
    auto item = settingList_->currentItem();
    int index = item ? item->data(0, Qt::UserRole).toInt() : -1;
    bool valid = index >= 0 && index < (int)settings_.size();
    choices_->clear();
    if (!valid) {
        configure(numeric_, 0, 0, 1, 0, false);
        choices_->setEnabled(false); automatic_->setEnabled(false);
        apply_->setEnabled(false); default_->setEnabled(false);
        details_->setText("Select a supported setting.");
        updating_ = false; return;
    }
    auto &s = settings_[index];
    auto pending = edits_.find(s.current.key);
    auto value = pending == edits_.end() ? s.current : pending->second.value;
    bool numeric = s.choices.empty();
    configure(numeric_, s.minimum, s.maximum, s.step, value.value,
              numeric && s.writable && !value.automatic);
    numeric_.slider->setVisible(numeric); numeric_.number->setVisible(numeric);
    choices_->setVisible(!numeric);
    for (auto &c : s.choices) choices_->addItem(qs(c.second), c.first);
    int choice = choices_->findData(value.value);
    if (choice >= 0) choices_->setCurrentIndex(choice);
    choices_->setEnabled(s.writable && !value.automatic);
    automatic_->setVisible(s.autoSupported);
    automatic_->setChecked(value.automatic);
    automatic_->setEnabled(s.writable);
    apply_->setEnabled(s.writable);
    default_->setEnabled(s.writable && s.defaultAvailable);
    details_->setText(qs(s.name) + "\n" + qs(s.details) +
                      (!s.writable ? "\nRead only / unavailable on this camera." : ""));
    updating_ = false;
}
void Dock::stageSetting(bool final) {
    if (updating_) return;
    auto item = settingList_->currentItem();
    int index = item ? item->data(0, Qt::UserRole).toInt() : -1;
    if (index < 0 || index >= (int)settings_.size()) return;
    auto &s = settings_[index]; if (!s.writable) return;
    auto &edit = edits_[s.current.key];
    edit.value = {s.current.key, s.choices.empty() ? numeric_.number->value() : choices_->currentData().toDouble(), automatic_->isChecked()};
    edit.final = final; edit.changed = QDateTime::currentMSecsSinceEpoch(); edit.dirty = true;
    if (!liveTimer_.isActive()) liveTimer_.start();
}
void Dock::stageAxis(int index, bool final) {
    if (updating_ || !axes_[index].number->isEnabled()) return;
    auto key = "axis:" + std::to_string(index);
    auto &edit = edits_[key];
    edit.value = {key, axes_[index].number->value(), false};
    edit.final = final; edit.changed = QDateTime::currentMSecsSinceEpoch(); edit.dirty = true;
    if (!liveTimer_.isActive()) liveTimer_.start();
}
void Dock::flushLive(bool force) {
    auto now = QDateTime::currentMSecsSinceEpoch();
    for (auto i = edits_.begin(); i != edits_.end();) {
        auto &e = i->second;
        bool captured = numeric_.slider->isSliderDown();
        for (auto &axis : axes_) captured = captured || axis.slider->isSliderDown();
        bool final = force || e.final || (!captured && now - e.changed >= 150);
        if (!final && (!e.dirty || now - e.sent < 80)) { ++i; continue; }
        auto camera = device(); auto value = e.value; auto revision = deviceRevision_;
        worker_->submit("live:" + i->first, [this, camera, value, final, revision](ptz::Camera &c) {
            tinyobs::connect(c, camera);
            if (value.key.rfind("axis:", 0) == 0) {
                int axis = std::stoi(value.key.substr(5));
                c.setAxis(properties[axis], (long)std::lround(value.value));
            } else c.applySetting(value, final);
            if (final) post([this, value, revision] {
                if (revision != deviceRevision_) return;
                for (auto &s : settings_) if (s.current.key == value.key) s.current = value;
                auto items = settingList_->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
                for (auto item : items) if (item->data(1, Qt::UserRole).toString() == qs(value.key))
                    item->setText(1, value.automatic ? "Auto" : QString::number(value.value, 'g', 9));
                status(value.key.rfind("axis:", 0) == 0 ? "Position sent. Allow the camera to settle before saving." : "Setting applied and verified where readable.");
            });
        });
        if (final) i = edits_.erase(i);
        else { e.sent = now; e.dirty = false; ++i; }
    }
    if (edits_.empty()) liveTimer_.stop();
}

std::optional<ptz::Preset> Dock::selectedPreset() const {
    auto item = presets_->currentItem();
    int index = item ? item->data(Qt::UserRole).toInt() : -1;
    if (!store_ || index < 0 || index >= (int)store_->items.size()) return {};
    return store_->items[index];
}
void Dock::refreshPresets() {
    presets_->clear(); scenePresets_->clear();
    scenePresets_->addItem("No preset (leave camera unchanged)", -1);
    if (store_) for (int i = 0; i < (int)store_->items.size(); ++i) {
        auto &p = store_->items[i];
        auto label = qs(p.name) + QString("  [%1 / %2 / %3]").arg(p.position.pan).arg(p.position.tilt).arg(p.position.zoom);
        auto item = new QListWidgetItem(label, presets_);
        item->setData(Qt::UserRole, i);
        item->setToolTip(qs(p.device) + QString("\n%1 saved settings").arg(p.settings.size()));
        scenePresets_->addItem(qs(p.name) + "  (" + qs(p.device) + ")", i);
    }
    if (presets_->count()) presets_->setCurrentRow(0);
    path_->setText(store_ ? "Preset file: " + fileName(store_->path()) : "No preset file");
    selectScene();
}
void Dock::openPresets() {
    auto path = QFileDialog::getOpenFileName(this, "Open Tiny PTZ preset file", store_ ? fileName(store_->path()) : QString(), "Tiny PTZ presets (*.ptz)");
    if (path.isEmpty()) return;
    try {
        auto next = std::make_unique<ptz::PresetStore>(std::filesystem::path(path.toStdWString()));
        next->load(); store_ = std::move(next); saveConfig(); refreshPresets();
        status("Preset file opened. Changes made here will be saved to that file.");
    } catch (const std::exception &e) { status(qs(std::string(e.what())), true); }
}
void Dock::transferPresets(bool importing) {
    if (!store_) return;
    QFileDialog dialog(this, importing ? "Import Tiny PTZ presets" : "Export Tiny PTZ presets");
    dialog.setNameFilter("Tiny PTZ presets (*.ptz)");
    dialog.setDefaultSuffix("ptz");
    dialog.setAcceptMode(importing ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave);
    dialog.setFileMode(importing ? QFileDialog::ExistingFile : QFileDialog::AnyFile);
    dialog.setDirectory(importing ? QFileInfo(fileName(store_->path())).absolutePath()
                                  : QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    if (!importing) dialog.selectFile("presets.ptz");
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return;
    try {
        auto path = std::filesystem::path(dialog.selectedFiles().first().toStdWString());
        if (importing) {
            auto count = importPresetFile(*store_, path);
            refreshPresets();
            status(QString("Imported %1 presets. Existing presets and scene assignments were preserved.").arg(count));
        } else {
            auto count = exportPresetFile(*store_, path);
            status(QString("Exported %1 presets to %2").arg(count).arg(fileName(path)));
        }
    } catch (const std::exception &e) { status(qs(std::string(e.what())), true); }
}
void Dock::capturePreset(bool replace) {
    auto camera = device();
    if (camera.empty()) { status("Select and connect a camera first.", true); return; }
    auto selected = selectedPreset();
    if (replace && (!selected || selected->device != camera)) { status("Select a preset for this camera.", true); return; }
    bool ok = false;
    QString name = replace ? qs(selected->name) : QInputDialog::getText(this, "Save preset", "Name", QLineEdit::Normal, {}, &ok).trimmed();
    if ((!replace && !ok) || name.isEmpty()) return;
    if (!replace && std::any_of(store_->items.begin(), store_->items.end(), [&](auto &p) { return p.name == name.toStdWString() && p.device == camera; })) {
        status("That preset already exists. Use Update selected.", true); return;
    }
    flushLive(true); bool include = includeSettings_->isChecked(); auto path = store_->path();
    run("capture", [this, camera, name, include, replace, path](ptz::Camera &c) {
        tinyobs::connect(c, camera);
        auto position = c.read(); c.waitForPosition(position); position = c.read();
        ptz::Preset preset{name.toStdWString(), camera, position, include ? c.snapshotSettings() : std::vector<ptz::SettingValue>{}};
        post([this, preset, replace, path] {
            if (store_->path() != path) { status("Preset file changed during capture. Please save again.", true); return; }
            try {
                ptz::PresetStore next(path); next.load();
                auto found = std::find_if(next.items.begin(), next.items.end(), [&](auto &p) { return id(p) == id(preset); });
                if (replace && found == next.items.end()) throw std::runtime_error("Preset changed externally. Reload the file.");
                if (!replace && found != next.items.end()) throw std::runtime_error("Preset already exists. Reload the file.");
                if (replace) *found = preset; else next.items.push_back(preset);
                next.save(); store_->items = next.items; refreshPresets(); status("Preset saved.");
            } catch (const std::exception &e) { status(qs(std::string(e.what())), true); }
        });
    });
}
void Dock::recall(const ptz::Preset &preset, bool scene) {
    if (!scene) { ++*sceneGeneration_; flushLive(true); }
    else { edits_.clear(); liveTimer_.stop(); worker_->cancel("live:"); }
    auto token = sceneGeneration_->load(); auto generation = sceneGeneration_;
    auto label = qs(preset.name);
    run("recall", [this, preset, token, generation, scene, label](ptz::Camera &c) {
        if (generation->load() != token) return;
        tinyobs::connect(c, preset.device);
        if (generation->load() != token) return;
        c.recall(preset);
        auto position = c.read(); std::array<ptz::Range, 3> ranges;
        for (int i = 0; i < 3; ++i) ranges[i] = c.range(properties[i]);
        auto values = c.settings();
        post([this, preset, position, ranges, values, generation, token, scene, label] {
            if (generation->load() != token) return;
            int index = devices_->findData(qs(preset.device));
            if (index < 0) { devices_->addItem("Preset camera", qs(preset.device)); index = devices_->count() - 1; }
            devices_->setCurrentIndex(index);
            presentPosition(position, ranges); presentSettings(values);
            status((scene ? "Scene preset loaded: " : "Preset loaded: ") + label);
        });
    });
}

void Dock::loadConfig(const std::filesystem::path &bundledPresets) {
    std::filesystem::path presetPath = config_.parent_path() / L"presets.ptz";
    try {
        std::filesystem::create_directories(config_.parent_path());
        QFile f(fileName(config_));
        if (f.exists()) {
            if (!f.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read dock configuration.");
            QJsonParseError error;
            auto document = QJsonDocument::fromJson(f.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) throw std::runtime_error("Invalid dock configuration; original preserved.");
            auto json = document.object();
            rules_.enabled = json["sceneAutomation"].toBool();
            auto path = json["presetFile"].toString();
            if (!path.isEmpty()) presetPath = path.toStdWString();
            for (auto entry : json["assignments"].toArray()) {
                auto o = entry.toObject();
                if (o["scene"].toString().isEmpty() || o["preset"].toString().isEmpty()) continue;
                rules_.collections[bytes(o["collection"].toString())][bytes(o["scene"].toString())] =
                    {o["device"].toString().toStdWString(), o["preset"].toString().toStdWString()};
            }
        }
        if (!std::filesystem::exists(presetPath) && presetPath == config_.parent_path() / L"presets.ptz" && std::filesystem::exists(bundledPresets))
            std::filesystem::copy_file(bundledPresets, presetPath);
        auto next = std::make_unique<ptz::PresetStore>(presetPath); next->load(); store_ = std::move(next);
    } catch (const std::exception &e) {
        status(qs(std::string(e.what())), true);
        store_ = std::make_unique<ptz::PresetStore>(presetPath);
        // Missing/corrupt external libraries are never silently replaced or written.
    }
    QSignalBlocker block(sceneEnabled_); sceneEnabled_->setChecked(rules_.enabled);
}
void Dock::saveConfig() {
    try {
        QJsonObject json;
        json["version"] = 1;
        json["presetFile"] = fileName(store_->path());
        json["sceneAutomation"] = rules_.enabled;
        QJsonArray assignments;
        for (auto &collection : rules_.collections) for (auto &scene : collection.second) {
            QJsonObject o;
            o["collection"] = qs(collection.first); o["scene"] = qs(scene.first);
            o["device"] = qs(scene.second.device); o["preset"] = qs(scene.second.name);
            assignments.append(o);
        }
        json["assignments"] = assignments;
        QSaveFile file(fileName(config_));
        if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(json).toJson()) < 0 || !file.commit())
            throw std::runtime_error("Cannot save dock configuration.");
    } catch (const std::exception &e) { status(qs(std::string(e.what())), true); }
}
void Dock::refreshScenes() {
    if (stopped_) return;
    auto previous = scenes_->currentData().toString();
    QSignalBlocker block(scenes_); scenes_->clear();
    for (auto &scene : host_.scenes()) scenes_->addItem(scene.name, qs(scene.uuid));
    int index = scenes_->findData(previous);
    if (index < 0) index = scenes_->findData(qs(host_.currentScene().uuid));
    if (index >= 0) scenes_->setCurrentIndex(index);
    selectScene();
}
void Dock::selectScene() {
    auto collection = host_.collection(); auto scene = bytes(scenes_->currentData().toString());
    scenePresets_->setCurrentIndex(0);
    auto c = rules_.collections.find(collection);
    if (c != rules_.collections.end()) {
        auto s = c->second.find(scene);
        if (s != c->second.end()) {
            int index = -1;
            for (int i = 0; store_ && i < (int)store_->items.size(); ++i) if (id(store_->items[i]) == s->second) index = i;
            scenePresets_->setCurrentIndex(index >= 0 ? index + 1 : 0);
            if (index < 0) { sceneStatus_->setText("Assigned preset is missing: " + qs(s->second.name) + ". Choose a replacement or clear the assignment."); return; }
        }
    }
    sceneStatus_->setText("Collection: " + qs(collection) + "\nActive Program scene: " + host_.currentScene().name);
}
void Dock::assignScene() {
    auto collection = host_.collection(); auto scene = bytes(scenes_->currentData().toString());
    if (scene.empty()) return;
    int preset = scenePresets_->currentData().toInt();
    if (preset < 0 || preset >= (int)store_->items.size()) rules_.collections[collection].erase(scene);
    else rules_.collections[collection][scene] = id(store_->items[preset]);
    saveConfig(); selectScene();
    if (scene == host_.currentScene().uuid) sceneChanged();
}
void Dock::frontendReady() { if (!stopped_) { ready_ = true; refreshScenes(); sceneChanged(); } }
void Dock::collectionChanging() {
    ready_ = false; ++*sceneGeneration_; worker_->cancel("recall");
}
void Dock::sceneChanged() {
    if (stopped_ || !ready_) return;
    ++*sceneGeneration_; worker_->cancel("recall");
    auto scene = host_.currentScene(); auto collection = host_.collection();
    selectScene();
    auto target = rules_.lookup(collection, scene.uuid);
    if (!target) {
        if (rules_.enabled) status("No preset assigned to the active Program scene. Camera unchanged.");
        return;
    }
    // Re-read the library so standalone edits are visible before automatic recall.
    try {
        store_->load();
        auto found = std::find_if(store_->items.begin(), store_->items.end(), [&](auto &p) { return id(p) == *target; });
        if (found == store_->items.end()) { status("Scene preset is missing: " + qs(target->name) + ". Check its assignment.", true); return; }
        auto preset = *found; refreshPresets(); recall(preset, true);
    } catch (const std::exception &e) { status(qs(std::string(e.what())), true); }
}
void Dock::shutdown() {
    if (stopped_) return;
    stopped_ = true; ++*sceneGeneration_; liveTimer_.stop(); edits_.clear();
    if (worker_) worker_->stop();
}
} // namespace tinyobs

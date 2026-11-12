#include "qt_window.hpp"
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSplitter>
#include <QStyleHints>
#include <QVBoxLayout>
#include <QPainter>
#ifdef _WIN32
#include <dwmapi.h>
#else
#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoSink>
#include <QVideoFrame>
#include <QDebug>
#include "linux_camera_match.hpp"
#endif
#include <algorithm>
namespace tinyapp {
#ifndef _WIN32
// A plain sink requests CPU frames instead of importing camera textures into
// Qt's RHI/Wayland renderer. The resulting image uses normal QWidget painting.
class SoftwareVideoSurface : public QWidget {
  public:
    explicit SoftwareVideoSurface(QWidget *parent) : QWidget(parent), sink_(this) {
        setAttribute(Qt::WA_OpaquePaintEvent);
        QObject::connect(&sink_, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
            if (!frame.isValid()) { image_ = {}; update(); return; }
            auto image = frame.toImage();
            if (image.isNull()) {
                if (failed) failed(QString("Cannot decode preview frame (pixel format %1).").arg(int(frame.pixelFormat())));
                return;
            }
            image_ = std::move(image); update();
            if (ready) ready();
        });
    }
    QVideoSink *sink() { return &sink_; }
    std::function<void()> ready;
    std::function<void(const QString &)> failed;
  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this); painter.fillRect(rect(), QColor("#070c14"));
        if (image_.isNull()) return;
        QSize fit = image_.size().scaled(size(), Qt::KeepAspectRatio);
        QRect target(QPoint((width() - fit.width()) / 2, (height() - fit.height()) / 2), fit);
        painter.drawImage(target, image_);
    }
  private:
    QVideoSink sink_;
    QImage image_;
};
#endif
static bool systemDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    static const bool dark = qApp->palette().color(QPalette::Window).lightness() < 128;
    return dark;
#endif
}
void applyTheme(const QString &theme) {
    const bool nativeDark = systemDark();
    bool dark = theme == "dark" || (theme == "system" && nativeDark);
    const QString background = dark ? "#101720" : "#f2f5f9";
    const QString panel = dark ? "#182332" : "#ffffff";
    const QString secondary = dark ? "#223247" : "#edf2f8";
    const QString border = dark ? "#35465d" : "#d6dfeb";
    const QString text = dark ? "#edf3fc" : "#16243a";
    const QString muted = dark ? "#a3b4cc" : "#546780";
    const QString accent = dark ? "#70a9ff" : "#2563eb";
    QPalette p;
    p.setColor(QPalette::Window, QColor(background));
    p.setColor(QPalette::WindowText, QColor(text));
    p.setColor(QPalette::Base, QColor(panel));
    p.setColor(QPalette::AlternateBase, QColor(secondary));
    p.setColor(QPalette::Text, QColor(text));
    p.setColor(QPalette::Button, QColor(secondary));
    p.setColor(QPalette::ButtonText, QColor(text));
    p.setColor(QPalette::Highlight, QColor(accent));
    p.setColor(QPalette::HighlightedText, dark ? QColor("#101720") : QColor(Qt::white));
    p.setColor(QPalette::ToolTipBase, QColor(panel));
    p.setColor(QPalette::ToolTipText, QColor(text));
    p.setColor(QPalette::PlaceholderText, QColor(muted));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(muted));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(muted));
    p.setColor(QPalette::Mid, QColor(border));
    p.setColor(QPalette::Dark, QColor(border));
    p.setColor(QPalette::Light, QColor(secondary));
    p.setColor(QPalette::Midlight, QColor(secondary));
    p.setColor(QPalette::Shadow, QColor(background));
    qApp->setPalette(p);
    qApp->setStyleSheet(QString(R"(
        QFrame[card="true"] { background: %1; border: 1px solid %2; border-radius: 12px; }
        QLabel { background: transparent; }
        QLabel[muted="true"] { color: %3; }
        QLabel#appTitle { font-size: 24px; font-weight: 650; }
        QLabel#connectionBadge { background: %4; border-radius: 12px; padding: 7px 12px; }
        QWidget#tinyPtzDock { background: %1; }
        QScrollArea { border: none; background: %1; }
        QTabWidget::pane { border: 1px solid %2; border-radius: 7px; background: %1; }
        QTabBar::tab { background: transparent; border: none; border-bottom: 2px solid transparent; padding: 9px 12px; margin-right: 3px; color: %3; }
        QTabBar::tab:selected { color: %5; border-bottom: 2px solid %5; font-weight: 600; }
        QPushButton { background: %4; border: 1px solid %2; border-radius: 6px; padding: 7px 9px; }
        QPushButton:hover { border-color: %5; }
        QPushButton:pressed { background: %2; }
        QPushButton#previewToggle, QPushButton#connectCamera { background: %5; color: %6; border-color: %5; font-weight: 600; }
        QPushButton#previewToggle:disabled, QPushButton#connectCamera:disabled { background: %4; color: %3; border-color: %2; }
        QPushButton:disabled { color: %3; }
        QComboBox, QDoubleSpinBox, QLineEdit { background: %1; border: 1px solid %2; border-radius: 5px; padding: 5px; min-height: 22px; }
        QComboBox:focus, QDoubleSpinBox:focus, QLineEdit:focus { border-color: %5; }
        QListWidget, QTreeWidget { background: %1; border: 1px solid %2; border-radius: 5px; padding: 3px; }
        QListWidget::item { padding: 9px 6px; }
        QListWidget::item:selected, QTreeWidget::item:selected { background: %4; color: %5; }
        QHeaderView::section { background: %4; border: none; padding: 5px; color: %3; }
        QSlider::groove:horizontal { height: 4px; background: %2; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: %5; border-radius: 2px; }
        QSlider::handle:horizontal { background: %1; border: 2px solid %5; width: 12px; margin: -6px 0; border-radius: 7px; }
        QSlider::sub-page:horizontal:disabled { background: %2; }
        QCheckBox { spacing: 7px; padding: 3px 0; }
        QSplitter::handle { background: transparent; }
        QToolTip { border: 1px solid %2; padding: 5px; }
    )").arg(panel, border, muted, secondary, accent, dark ? "#101720" : "#ffffff"));
}
PreviewCanvas::PreviewCanvas(QWidget *parent) : QWidget(parent) {
    setObjectName("previewCanvas");
    setMinimumSize(280, 180);
#ifdef _WIN32
    surface_ = new QWidget(this);
#else
    auto software = new SoftwareVideoSurface(this);
    software->ready = [this] { if (frameReady) frameReady(); };
    software->failed = [this](const QString &message) { if (frameError) frameError(message); };
    surface_ = software;
#endif
    surface_->setObjectName("videoSurface");
#ifdef _WIN32
    surface_->setAttribute(Qt::WA_NativeWindow);
    surface_->winId();
#endif
    surface_->setStyleSheet("background: #070c14; border-radius: 8px;");
    placeholder_ = new QLabel(surface_);
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setWordWrap(true);
    placeholder_->setStyleSheet("color: #aabbd0; background: transparent; padding: 20px;");
    setPlaying(false);
}
HWND PreviewCanvas::owner() const {
#ifdef _WIN32
    return reinterpret_cast<HWND>(surface_->winId());
#else
    return nullptr;
#endif
}
QSize PreviewCanvas::videoSize() const { return surface_->size(); }
#ifndef _WIN32
QVideoSink *PreviewCanvas::videoSink() const { return static_cast<SoftwareVideoSurface *>(surface_)->sink(); }
#endif
void PreviewCanvas::setPlaying(bool playing, const QString &message) {
#ifndef _WIN32
    if (!playing) videoSink()->setVideoFrame({});
#endif
    placeholder_->setVisible(!playing);
    placeholder_->setText(message.isEmpty() ? "Preview is off\n\nConnect your camera, then start the preview." : message);
}
void PreviewCanvas::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    QSize fit(16, 9); fit.scale(size(), Qt::KeepAspectRatio);
    surface_->setGeometry((width() - fit.width()) / 2, (height() - fit.height()) / 2, fit.width(), fit.height());
    placeholder_->setGeometry(surface_->rect());
    if (resized) resized(fit.width(), fit.height());
}
Window::Window(std::filesystem::path data, bool discover) {
    setWindowTitle("Tiny PTZ");
    setObjectName("tinyPtzWindow");
    resize(1180, 790); setMinimumSize(900, 620);
    auto central = new QWidget(this);
    auto layout = new QVBoxLayout(central); layout->setContentsMargins(20, 18, 20, 18); layout->setSpacing(16);
    auto header = new QHBoxLayout;
    auto titles = new QVBoxLayout; titles->setSpacing(2);
    auto title = new QLabel("Tiny PTZ", central); title->setObjectName("appTitle"); titles->addWidget(title);
    auto subtitle = new QLabel("OBSBOT camera controller", central); subtitle->setProperty("muted", true); titles->addWidget(subtitle);
    header->addLayout(titles); header->addStretch();
    connection_ = new QLabel("Not connected", central); connection_->setObjectName("connectionBadge"); header->addWidget(connection_);
    auto themeLabel = new QLabel("Theme", central); themeLabel->setProperty("muted", true); header->addWidget(themeLabel);
    theme_ = new QComboBox(central); theme_->setObjectName("appTheme");
    theme_->addItem("System", "system"); theme_->addItem("Light", "light"); theme_->addItem("Dark", "dark"); header->addWidget(theme_);
    layout->addLayout(header);
    auto splitter = new QSplitter(Qt::Horizontal, central); splitter->setChildrenCollapsible(false); splitter->setHandleWidth(14);
    auto previewCard = new QFrame(splitter); previewCard->setProperty("card", true);
    auto previewLayout = new QVBoxLayout(previewCard); previewLayout->setContentsMargins(16, 14, 16, 14);
    auto previewHeader = new QHBoxLayout;
    auto label = new QLabel("Camera preview", previewCard); QFont font = label->font(); font.setPointSize(12); font.setBold(true); label->setFont(font);
    previewHeader->addWidget(label); previewHeader->addStretch();
    previewButton_ = new QPushButton("Start preview", previewCard); previewButton_->setObjectName("previewToggle"); previewButton_->setEnabled(false);
    previewHeader->addWidget(previewButton_); previewLayout->addLayout(previewHeader);
    preview_ = new PreviewCanvas(previewCard); previewLayout->addWidget(preview_, 1);
#ifndef _WIN32
    capture_ = new QMediaCaptureSession(this);
    capture_->setVideoSink(preview_->videoSink());
    previewWatchdog_.setSingleShot(true); previewWatchdog_.setInterval(7000);
    QObject::connect(&previewWatchdog_, &QTimer::timeout, this, [this] {
        stopLinuxPreview(playing_ ? "Preview stopped receiving frames. Close other camera apps and try again."
                                  : "The camera opened but supplied no video frames. Close other camera apps and try again.");
    });
#endif
    auto caption = new QLabel("Preview is optional. Stop it when OBS is using the camera.", previewCard);
    caption->setProperty("muted", true); caption->setWordWrap(true); previewLayout->addWidget(caption);
    auto panel = new QFrame(splitter); panel->setProperty("card", true); panel->setMinimumWidth(410);
    auto panelLayout = new QVBoxLayout(panel); panelLayout->setContentsMargins(8, 8, 8, 8);
    tinyobs::Host host{[] { return std::vector<tinyobs::Scene>{}; }, [] { return tinyobs::Scene{}; }, [] { return std::string{}; }};
    controls_ = new tinyobs::Dock(data / L"app.json", data / L"presets.ptz", host, discover, true);
    panelLayout->addWidget(controls_);
    splitter->addWidget(previewCard); splitter->addWidget(panel); splitter->setStretchFactor(0, 3); splitter->setStretchFactor(1, 2);
    splitter->setSizes({660, 460}); layout->addWidget(splitter, 1); setCentralWidget(central);
    theme_->setCurrentIndex(std::max(0, theme_->findData(controls_->themePreference())));
#ifndef _WIN32
    preview_->frameReady = [this] {
        if (!previewRequested_ || !camera_) return;
        previewWatchdog_.start();
        if (playing_) return;
        playing_ = true; preview_->setPlaying(true);
        previewButton_->setText("Stop preview"); previewButton_->setEnabled(true);
        controls_->setPreviewStatus("Preview is live.");
        const auto frame = preview_->videoSink()->videoFrame();
        qInfo() << "TinyPTZ preview: first decoded frame" << frame.size() << "format" << frame.pixelFormat();
    };
    preview_->frameError = [this](const QString &message) {
        auto failed = camera_;
        QTimer::singleShot(0, this, [this, failed, message] { if (failed && camera_ == failed) stopLinuxPreview(message); });
    };
#endif
    controls_->connectionChanged = [this](bool connected, const QString &text) {
        connection_->setText(connected ? "Connected" : "Not connected"); connection_->setToolTip(text);
        previewButton_->setEnabled(connected);
#ifndef _WIN32
        if (!connected) stopLinuxPreview();
#endif
    };
    controls_->previewChanged = [this](bool playing, const QString &error) {
#ifndef _WIN32
        if (playing) startLinuxPreview();
        else stopLinuxPreview(error);
#else
        playing_ = playing; preview_->setPlaying(playing, error);
        previewButton_->setText(playing ? "Stop preview" : "Start preview");
        previewButton_->setEnabled(connection_->text() == "Connected");
#endif
    };
    preview_->resized = [this](int width, int height) { if (!closing_) controls_->resizePreview(width, height); };
    QObject::connect(previewButton_, &QPushButton::clicked, this, [this] {
#ifndef _WIN32
        if (previewRequested_) { stopLinuxPreview(); return; }
#endif
        previewButton_->setEnabled(false);
        auto size = preview_->videoSize();
        controls_->setPreviewEnabled(!playing_, preview_->owner(), size.width(), size.height());
    });
    QObject::connect(theme_, &QComboBox::currentIndexChanged, this, [this] {
        controls_->setThemePreference(theme_->currentData().toString()); updateTheme();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { if (theme_->currentData() == "system") updateTheme(); });
#endif
    updateTheme();
}
void Window::updateTheme() {
    auto theme = theme_->currentData().toString(); applyTheme(theme);
#ifdef _WIN32
    BOOL dark = theme == "dark" || (theme == "system" && systemDark());
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 20, &dark, sizeof(dark));
#endif
}
#ifndef _WIN32
void Window::stopLinuxPreview(const QString &message) {
    previewWatchdog_.stop(); previewRequested_ = false;
    if (camera_) {
        auto old = camera_; camera_ = nullptr;
        QObject::disconnect(old, nullptr, this, nullptr);
        old->stop(); capture_->setCamera(nullptr); old->deleteLater();
    }
    playing_ = false; preview_->setPlaying(false, message);
    previewButton_->setText("Start preview");
    previewButton_->setEnabled(!closing_ && connection_->text() == "Connected");
    if (controls_) controls_->setPreviewStatus(message.isEmpty() ? "Preview stopped." : message, !message.isEmpty());
    if (!message.isEmpty()) qWarning() << "TinyPTZ preview:" << message;
}
void Window::startLinuxPreview() {
    stopLinuxPreview();
    const auto selected = ptz::filePath(controls_->selectedDevice());
    const auto devices = QMediaDevices::videoInputs();
    std::vector<PreviewDeviceIdentity> identities;
    qInfo() << "TinyPTZ preview: selected" << QString::fromStdWString(controls_->selectedDevice()) << controls_->selectedDeviceName();
    for (const auto &candidate : devices) {
        identities.push_back({candidate.id(), candidate.description()});
        qInfo() << "TinyPTZ preview: video device" << candidate.id() << candidate.description();
    }
    const auto resolved = matchPreviewDevice(selected, controls_->selectedDeviceName(), identities);
    if (resolved.index < 0) {
        stopLinuxPreview(resolved.error);
        return;
    }
    const auto match = devices[resolved.index];
    camera_ = new QCamera(match, this);
    capture_->setCamera(camera_);
    QObject::connect(camera_, &QCamera::activeChanged, this, [this](bool active) {
        // Camera activation alone does not mean it is delivering video.
        if (!active && playing_) stopLinuxPreview("The camera stopped its video stream.");
    });
    QObject::connect(camera_, &QCamera::errorOccurred, this, [this](QCamera::Error error, const QString &message) {
        if (error != QCamera::NoError) stopLinuxPreview("Preview failed: " + message);
    });
    previewRequested_ = true;
    preview_->setPlaying(false, "Starting preview… Waiting for the first video frame.");
    previewButton_->setText("Stop preview"); previewButton_->setEnabled(true);
    controls_->setPreviewStatus("Starting preview… Waiting for video frames.");
    qInfo() << "TinyPTZ preview: starting" << match.id() << "backend" << qgetenv("QT_MEDIA_BACKEND");
    previewWatchdog_.start();
    camera_->start();
}
#endif
Window::~Window() {
#ifndef _WIN32
    stopLinuxPreview();
#endif
    controls_->shutdown();
}
void Window::closeEvent(QCloseEvent *event) {
    if (closing_) { event->ignore(); return; }
    closing_ = true;
#ifndef _WIN32
    stopLinuxPreview();
#endif
    controls_->shutdown(); event->accept();
}
} // namespace tinyapp

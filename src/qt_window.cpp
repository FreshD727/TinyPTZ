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
#include <dwmapi.h>
#include <algorithm>
namespace tinyapp {
void applyTheme(const QString &theme) {
    bool dark = theme == "dark" || (theme == "system" && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
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
    surface_ = new QWidget(this);
    surface_->setObjectName("videoSurface");
    surface_->setAttribute(Qt::WA_NativeWindow);
    surface_->setStyleSheet("background: #070c14; border-radius: 8px;");
    surface_->winId();
    placeholder_ = new QLabel(surface_);
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setWordWrap(true);
    placeholder_->setStyleSheet("color: #aabbd0; background: transparent; padding: 20px;");
    setPlaying(false);
}
HWND PreviewCanvas::owner() const { return reinterpret_cast<HWND>(surface_->winId()); }
QSize PreviewCanvas::videoSize() const { return surface_->size(); }
void PreviewCanvas::setPlaying(bool playing, const QString &message) {
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
    controls_->connectionChanged = [this](bool connected, const QString &text) {
        connection_->setText(connected ? "Connected" : "Not connected"); connection_->setToolTip(text);
        previewButton_->setEnabled(connected);
    };
    controls_->previewChanged = [this](bool playing, const QString &error) {
        playing_ = playing; preview_->setPlaying(playing, error);
        previewButton_->setText(playing ? "Stop preview" : "Start preview");
        previewButton_->setEnabled(connection_->text() == "Connected");
    };
    preview_->resized = [this](int width, int height) { if (!closing_) controls_->resizePreview(width, height); };
    QObject::connect(previewButton_, &QPushButton::clicked, this, [this] {
        previewButton_->setEnabled(false);
        auto size = preview_->videoSize();
        controls_->setPreviewEnabled(!playing_, preview_->owner(), size.width(), size.height());
    });
    QObject::connect(theme_, &QComboBox::currentIndexChanged, this, [this] {
        controls_->setThemePreference(theme_->currentData().toString()); updateTheme();
    });
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { if (theme_->currentData() == "system") updateTheme(); });
    updateTheme();
}
void Window::updateTheme() {
    auto theme = theme_->currentData().toString(); applyTheme(theme);
    BOOL dark = theme == "dark" || (theme == "system" && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), 20, &dark, sizeof(dark));
}
Window::~Window() { controls_->shutdown(); }
void Window::closeEvent(QCloseEvent *event) {
    if (closing_) { event->ignore(); return; }
    closing_ = true; controls_->shutdown(); event->accept();
}
} // namespace tinyapp

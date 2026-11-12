#pragma once
#include "dock.hpp"
#include <QMainWindow>
#include <QWidget>
class QLabel;
class QPushButton;
class QComboBox;
#ifndef _WIN32
#include <QTimer>
class QVideoSink;
#endif
namespace tinyapp {
#ifndef _WIN32
class LinuxPreviewCapture;
#endif
void applyTheme(const QString &theme);
class PreviewCanvas : public QWidget {
  public:
    explicit PreviewCanvas(QWidget *parent = nullptr);
    HWND owner() const;
    QSize videoSize() const;
    QWidget *videoOutput() const { return surface_; }
#ifndef _WIN32
    QVideoSink *videoSink() const;
    void presentImage(const QImage &image);
    std::function<void()> frameReady;
    std::function<void(const QString &)> frameError;
#endif
    void setPlaying(bool playing, const QString &message = {});
    std::function<void(int, int)> resized;
  protected:
    void resizeEvent(QResizeEvent *event) override;
  private:
    QWidget *surface_;
    QLabel *placeholder_;
};
class Window : public QMainWindow {
  public:
    explicit Window(std::filesystem::path data, bool discover = true);
    ~Window() override;
    tinyobs::Dock *controls() const { return controls_; }
  protected:
    void closeEvent(QCloseEvent *event) override;
  private:
    tinyobs::Dock *controls_;
    PreviewCanvas *preview_;
    QPushButton *previewButton_;
    QLabel *connection_;
    QComboBox *theme_;
    bool playing_ = false, closing_ = false;
    void updateTheme();
#ifndef _WIN32
    LinuxPreviewCapture *capture_ = nullptr;
    QTimer previewWatchdog_;
    bool previewRequested_ = false;
    void startLinuxPreview();
    void stopLinuxPreview(const QString &message = {});
#endif
};
} // namespace tinyapp

#pragma once
#include "dock.hpp"
#include <QMainWindow>
#include <QWidget>
class QLabel;
class QPushButton;
class QComboBox;
namespace tinyapp {
void applyTheme(const QString &theme);
class PreviewCanvas : public QWidget {
  public:
    explicit PreviewCanvas(QWidget *parent = nullptr);
    HWND owner() const;
    QSize videoSize() const;
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
};
} // namespace tinyapp

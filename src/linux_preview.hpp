#pragma once
#include <QObject>
#include <QImage>
#include <QString>
#include <functional>
#include <memory>

namespace tinyapp {
class LinuxPreviewCapture : public QObject {
  public:
    explicit LinuxPreviewCapture(QObject *parent = nullptr);
    ~LinuxPreviewCapture() override;
    QString startDevice(const QString &path);
    // Exercises the same decode/conversion/appsink path without camera hardware.
    QString startTestPattern();
    void stop();
    bool running() const;
    std::function<void(const QImage &)> frame;
    std::function<void(const QString &)> failed;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace tinyapp

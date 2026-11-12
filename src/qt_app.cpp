#include "qt_window.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QIcon>
#ifndef _WIN32
#include "linux_preview.hpp"
#include "desktop_entry.hpp"
#include <QElapsedTimer>
#include <QThread>
#include <QDebug>
#endif
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("TinyPTZ"); app.setApplicationDisplayName("Tiny PTZ"); app.setApplicationVersion("0.7.5");
    app.setWindowIcon(QIcon(":/tinyptz/icon.png"));
    app.setDesktopFileName("tinyptz");
    app.setStyle("Fusion");
#ifdef _WIN32
    app.setFont(QFont("Segoe UI", 10));
#endif
    QCommandLineParser parser;
    parser.setApplicationDescription("OBSBOT camera controller");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"check-runtime", "Check the UI/runtime without connecting to a camera."});
#ifndef _WIN32
    parser.addOption({"check-preview-runtime", "Decode and display a test video without opening a camera."});
    parser.addOption({"install-desktop-entry", "Add Tiny PTZ and its icon to your applications menu."});
#endif
    parser.addOption({"data-dir", "Store presets and preferences in this directory.", "directory"});
    parser.process(app);
    try {
#ifndef _WIN32
        if (parser.isSet("install-desktop-entry")) {
            auto error = tinyapp::installDesktopEntry();
            if (!error.isEmpty()) { qCritical().noquote() << error; return 1; }
            qInfo() << "Tiny PTZ launcher and icon installed for the current user."; return 0;
        }
#endif
        bool runtimeCheck = parser.isSet("check-runtime");
#ifndef _WIN32
        const bool previewCheck = parser.isSet("check-preview-runtime");
        runtimeCheck = runtimeCheck || previewCheck;
        if (!runtimeCheck) {
            auto error = tinyapp::installDesktopEntry();
            if (!error.isEmpty()) qWarning().noquote() << "Application-menu registration failed:" << error;
        }
#endif
#ifdef _WIN32
        auto data = ptz::portableData().parent_path();
#else
        auto data = ptz::filePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdWString());
#endif
        if (parser.isSet("data-dir")) data = ptz::filePath(parser.value("data-dir").toStdWString());
        tinyapp::Window window(data, !runtimeCheck);
#ifndef _WIN32
        if (previewCheck) {
            auto preview = static_cast<tinyapp::PreviewCanvas *>(window.findChild<QWidget *>("previewCanvas"));
            tinyapp::LinuxPreviewCapture capture;
            int frames = 0; QString error;
            capture.frame = [&](const QImage &image) { preview->presentImage(image); preview->setPlaying(true); ++frames; };
            capture.failed = [&](const QString &message) { error = message; };
            error = capture.startTestPattern();
            QElapsedTimer timer; timer.start();
            while (frames < 3 && error.isEmpty() && timer.elapsed() < 5000) {
                app.processEvents(); QThread::msleep(5);
            }
            capture.stop();
            if (!error.isEmpty() || frames < 3) {
                qCritical() << "Preview runtime check failed:" << error; return 1;
            }
            qInfo() << "PASS: MJPEG camera-preview decode and display path"; return 0;
        }
#endif
        if (runtimeCheck) return app.windowIcon().isNull() ? 1 : 0;
        window.show();
        return app.exec();
    } catch (const std::exception &e) {
        QMessageBox::critical(nullptr, "Tiny PTZ", QString::fromUtf8(e.what())); return 1;
    }
}

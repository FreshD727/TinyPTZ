#include "qt_window.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QCommandLineParser>
#include <QStandardPaths>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("TinyPTZ"); app.setApplicationDisplayName("Tiny PTZ"); app.setApplicationVersion("0.7.0");
    app.setStyle("Fusion");
#ifdef _WIN32
    app.setFont(QFont("Segoe UI", 10));
#endif
    QCommandLineParser parser;
    parser.setApplicationDescription("OBSBOT camera controller");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"check-runtime", "Check the UI/runtime without connecting to a camera."});
    parser.addOption({"data-dir", "Store presets and preferences in this directory.", "directory"});
    parser.process(app);
    try {
        bool runtimeCheck = parser.isSet("check-runtime");
#ifdef _WIN32
        auto data = ptz::portableData().parent_path();
#else
        auto data = ptz::filePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdWString());
#endif
        if (parser.isSet("data-dir")) data = ptz::filePath(parser.value("data-dir").toStdWString());
        tinyapp::Window window(data, !runtimeCheck);
        if (runtimeCheck) return 0;
        window.show();
        return app.exec();
    } catch (const std::exception &e) {
        QMessageBox::critical(nullptr, "Tiny PTZ", QString::fromUtf8(e.what())); return 1;
    }
}

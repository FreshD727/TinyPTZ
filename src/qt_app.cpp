#include "qt_window.hpp"
#include <QApplication>
#include <QMessageBox>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Tiny PTZ"); app.setApplicationVersion("0.5.0");
    app.setStyle("Fusion"); app.setFont(QFont("Segoe UI", 10));
    try {
        bool runtimeCheck = app.arguments().contains("--check-runtime");
        tinyapp::Window window(ptz::portableData().parent_path(), !runtimeCheck);
        if (runtimeCheck) return 0;
        window.show();
        return app.exec();
    } catch (const std::exception &e) {
        QMessageBox::critical(nullptr, "Tiny PTZ", QString::fromUtf8(e.what())); return 1;
    }
}

#pragma once
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QCoreApplication>

namespace tinyapp {
inline QString installDesktopEntry() {
    auto executable = qEnvironmentVariable("APPIMAGE");
    if (executable.isEmpty()) executable = QCoreApplication::applicationFilePath();
    executable = QFileInfo(executable).absoluteFilePath();
    if (executable.contains('\n') || executable.contains('\r')) return "The executable path contains a line break.";
    QString argument = "\"";
    for (auto character : executable) {
        if (character == '%') { argument += "%%"; continue; }
        if (character == '\\' || character == '"' || character == '$' || character == '`') argument += '\\';
        argument += character;
    }
    argument += '"';
    // Desktop-entry string escaping is applied after Exec argument escaping.
    argument.replace("\\", "\\\\");
    auto base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    auto iconPath = base + "/icons/hicolor/256x256/apps/tinyptz.png";
    auto entryPath = base + "/applications/tinyptz.desktop";
    auto write = [](const QString &path, const QByteArray &bytes) {
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes) return true;
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
        QSaveFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
    };
    QFile icon(":/tinyptz/icon.png");
    if (!icon.open(QIODevice::ReadOnly) || !write(iconPath, icon.readAll())) return "Cannot install the application icon.";
    auto entry = QString("[Desktop Entry]\nType=Application\nName=Tiny PTZ\nComment=OBSBOT camera controls, presets and preview\nExec=%1\nIcon=tinyptz\nTerminal=false\nCategories=AudioVideo;Video;\nStartupWMClass=TinyPTZ\n").arg(argument);
    if (!write(entryPath, entry.toUtf8())) return "Cannot install the application launcher.";
    return {};
}
} // namespace tinyapp

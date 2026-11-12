#pragma once
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <filesystem>
#include <vector>

namespace tinyapp {
struct PreviewDeviceIdentity { QByteArray id; QString name; };
struct PreviewDeviceMatch { int index = -1; QString error; };
inline PreviewDeviceMatch matchPreviewDevice(const std::filesystem::path &selected,
                                            const QString &selectedName,
                                            const std::vector<PreviewDeviceIdentity> &devices) {
    int named = -1, count = 0;
    bool namedHasPath = false;
    for (int i = 0; i < int(devices.size()); ++i) {
        auto text = QString::fromUtf8(devices[i].id);
        QUrl url(text);
        if (url.isLocalFile()) text = url.toLocalFile();
        bool hasPath = text.startsWith('/');
        if (hasPath) {
            std::error_code error;
            if (std::filesystem::equivalent(selected, std::filesystem::u8path(text.toUtf8().toStdString()), error) && !error)
                return {i, {}};
        }
        if (!selectedName.isEmpty() && devices[i].name.simplified().compare(selectedName.simplified(), Qt::CaseInsensitive) == 0) {
            named = i; ++count; namedHasPath = hasPath;
        }
    }
    // Qt 6.4 GStreamer uses generated numeric IDs, not V4L2 paths. Only a
    // unique name can identify such an entry; list order is not an identity.
    if (count == 1 && !namedHasPath) return {named, {}};
    if (count > 1) return {-1, "Multiple video devices have the selected camera's name. Preview cannot identify it unambiguously."};
    return {-1, devices.empty() ? "The video backend did not report any cameras. Reconnect the camera and try again."
                               : "The selected camera was not found in the video backend's device list. Rescan or reconnect the camera."};
}
} // namespace tinyapp

#include "linux_preview.hpp"
#include <QTimer>
#include <QFileInfo>
#include <QDebug>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

namespace tinyapp {
struct LinuxPreviewCapture::Impl {
    LinuxPreviewCapture *owner;
    GstElement *pipeline = nullptr;
    GstAppSink *sink = nullptr;
    GstBus *bus = nullptr;
    QTimer timer;
    explicit Impl(LinuxPreviewCapture *value) : owner(value) {
        gst_init(nullptr, nullptr);
        timer.setInterval(15);
        QObject::connect(&timer, &QTimer::timeout, owner, [this] { poll(); });
    }
    ~Impl() { stop(); }
    void stop() {
        timer.stop();
        if (pipeline) gst_element_set_state(pipeline, GST_STATE_NULL);
        if (bus) gst_object_unref(bus);
        if (sink) gst_object_unref(sink);
        if (pipeline) gst_object_unref(pipeline);
        pipeline = nullptr; bus = nullptr; sink = nullptr;
    }
    QString takeError() {
        auto message = bus ? gst_bus_pop_filtered(bus, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)) : nullptr;
        if (!message) return {};
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
            gst_message_unref(message); return "The camera video stream ended.";
        }
        GError *error = nullptr; gchar *debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        auto result = QString::fromUtf8(error ? error->message : "Camera stream failed.");
        qWarning() << "TinyPTZ capture:" << result << (debug ? debug : "");
        if (error) g_error_free(error);
        g_free(debug); gst_message_unref(message);
        return result;
    }
    QString fail(const QString &message) { stop(); return message; }
    QString create(const char *description) {
        stop(); GError *error = nullptr;
        pipeline = gst_parse_launch(description, &error);
        if (error || !pipeline) {
            auto message = QString::fromUtf8(error ? error->message : "Cannot create the camera pipeline.");
            if (error) g_error_free(error);
            return fail(message);
        }
        auto output = gst_bin_get_by_name(GST_BIN(pipeline), "frames");
        if (!output || !GST_IS_APP_SINK(output)) {
            if (output) gst_object_unref(output);
            return fail("The camera pipeline has no video frame output.");
        }
        sink = GST_APP_SINK(output);
        bus = gst_element_get_bus(pipeline);
        return {};
    }
    QString play() {
        if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            auto error = takeError();
            return fail(error.isEmpty() ? "Cannot start the camera video stream." : error);
        }
        timer.start(); return {};
    }
    void poll() {
        if (!pipeline || !sink) return;
        auto error = takeError();
        if (!error.isEmpty()) {
            stop(); if (owner->failed) owner->failed(error); return;
        }
        auto sample = gst_app_sink_try_pull_sample(sink, 0);
        // No sample during asynchronous startup is normal; only actual bus
        // ERROR/EOS messages above terminate the stream.
        if (!sample) return;
        GstVideoInfo info; gst_video_info_init(&info);
        GstVideoFrame mapped{}; QImage image;
        if (gst_video_info_from_caps(&info, gst_sample_get_caps(sample)) &&
            GST_VIDEO_INFO_FORMAT(&info) == GST_VIDEO_FORMAT_RGBA &&
            gst_video_frame_map(&mapped, &info, gst_sample_get_buffer(sample), GST_MAP_READ)) {
            image = QImage(static_cast<const uchar *>(GST_VIDEO_FRAME_PLANE_DATA(&mapped, 0)),
                           int(info.width), int(info.height), GST_VIDEO_FRAME_PLANE_STRIDE(&mapped, 0),
                           QImage::Format_RGBA8888).copy();
            gst_video_frame_unmap(&mapped);
        }
        gst_sample_unref(sample);
        if (image.isNull()) {
            stop(); if (owner->failed) owner->failed("Cannot decode the camera's video frame."); return;
        }
        if (owner->frame) owner->frame(image);
    }
};
LinuxPreviewCapture::LinuxPreviewCapture(QObject *parent) : QObject(parent), impl_(std::make_unique<Impl>(this)) {}
LinuxPreviewCapture::~LinuxPreviewCapture() = default;
void LinuxPreviewCapture::stop() { impl_->stop(); }
bool LinuxPreviewCapture::running() const { return impl_->pipeline != nullptr; }
QString LinuxPreviewCapture::startDevice(const QString &path) {
    impl_->stop();
    auto canonical = QFileInfo(path).canonicalFilePath();
    if (!canonical.startsWith("/dev/video")) return "The selected video device is unavailable. Reconnect the camera and rescan.";
    auto error = impl_->create("v4l2src name=camera io-mode=mmap ! capsfilter name=input ! decodebin ! videoconvert ! video/x-raw,format=RGBA ! appsink name=frames sync=false max-buffers=1 drop=true");
    if (!error.isEmpty()) return error;
    auto source = gst_bin_get_by_name(GST_BIN(impl_->pipeline), "camera");
    auto filter = gst_bin_get_by_name(GST_BIN(impl_->pipeline), "input");
    auto device = canonical.toUtf8();
    g_object_set(source, "device", device.constData(), nullptr);
    if (gst_element_set_state(impl_->pipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) {
        error = impl_->takeError();
        gst_object_unref(source); gst_object_unref(filter);
        return impl_->fail(error.isEmpty() ? "Cannot open camera video device " + canonical : error);
    }
    auto pad = gst_element_get_static_pad(source, "src");
    auto available = gst_pad_query_caps(pad, nullptr);
    gst_object_unref(pad);
    if (available && !gst_caps_is_any(available)) {
        for (auto preference : {"image/jpeg,width=1280,height=720,framerate=30/1",
                                "video/x-raw,width=1280,height=720,framerate=30/1",
                                "video/x-raw,width=640,height=480,framerate=30/1"}) {
            auto wanted = gst_caps_from_string(preference);
            auto compatible = gst_caps_intersect(available, wanted);
            gst_caps_unref(wanted);
            if (!gst_caps_is_empty(compatible)) {
                compatible = gst_caps_fixate(compatible);
                g_object_set(filter, "caps", compatible, nullptr);
                auto text = gst_caps_to_string(compatible);
                qInfo() << "TinyPTZ capture:" << canonical << "format" << text;
                g_free(text); gst_caps_unref(compatible); break;
            }
            gst_caps_unref(compatible);
        }
    }
    if (available) gst_caps_unref(available);
    gst_object_unref(source); gst_object_unref(filter);
    qInfo() << "TinyPTZ capture: opening exact device" << canonical;
    return impl_->play();
}
QString LinuxPreviewCapture::startTestPattern() {
    auto error = impl_->create("videotestsrc is-live=true num-buffers=30 pattern=smpte ! video/x-raw,width=320,height=180,framerate=30/1 ! jpegenc ! decodebin ! videoconvert ! video/x-raw,format=RGBA ! appsink name=frames sync=false max-buffers=1 drop=true");
    return error.isEmpty() ? impl_->play() : error;
}
} // namespace tinyapp

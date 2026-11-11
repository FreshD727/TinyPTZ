#include "core.hpp"
#include "protocol.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vidcap.h>
namespace ptz {
static void check(HRESULT hr, const char *what) {
    if (FAILED(hr)) {
        std::ostringstream s;
        s << what << " (0x" << std::hex << static_cast<unsigned long>(hr) << ")";
        if (hr == E_ACCESSDENIED)
            s << ". Allow camera access in Windows privacy settings and close other camera apps.";
        throw std::runtime_error(s.str());
    }
}
std::string utf8(const std::wstring &s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0,
                                nullptr, nullptr);
    if (!n)
        throw std::runtime_error("Invalid Unicode text");
    std::string r(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n, nullptr, nullptr);
    return r;
}
std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
    if (!n)
        throw std::runtime_error("Invalid UTF-8 text");
    std::wstring r(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n);
    return r;
}
static ComPtr<IEnumMoniker> enumerate() {
    ComPtr<ICreateDevEnum> d;
    check(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)),
          "Enumerate cameras");
    ComPtr<IEnumMoniker> e;
    HRESULT hr = d->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &e, 0);
    check(hr, "Enumerate cameras");
    return e;
}
static std::wstring identity(IMoniker *m) {
    LPOLESTR p = nullptr;
    check(m->GetDisplayName(nullptr, nullptr, &p), "Read camera identity");
    std::wstring id(p);
    CoTaskMemFree(p);
    return id;
}
std::vector<Device> devices() {
    std::vector<Device> out;
    auto e = enumerate();
    if (!e)
        return out;
    ComPtr<IMoniker> m;
    while (e->Next(1, &m, nullptr) == S_OK) {
        ComPtr<IPropertyBag> b;
        std::wstring name = L"Camera";
        if (SUCCEEDED(m->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&b)))) {
            VARIANT v;
            VariantInit(&v);
            if (SUCCEEDED(b->Read(L"FriendlyName", &v, nullptr)) && v.vt == VT_BSTR)
                name = v.bstrVal;
            VariantClear(&v);
        }
        out.push_back({name, identity(m.Get())});
        m.Reset();
    }
    return out;
}
void Camera::connect(const std::wstring &id) {
    disconnect();
    auto e = enumerate();
    ComPtr<IMoniker> m;
    while (e && e->Next(1, &m, nullptr) == S_OK) {
        if (identity(m.Get()) == id) {
            check(m->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&filter_)), "Open camera");
            HRESULT hr = filter_.As(&control_);
            if (FAILED(hr)) {
                disconnect();
                check(hr, "Camera has no standard PTZ interface");
            }
            id_ = id;
            filter_.As(&procamp_);
            return;
        }
        m.Reset();
    }
    throw std::runtime_error("Selected camera was disconnected. Refresh the device list.");
}
void Camera::disconnect() {
    stopPreview();
    std::exception_ptr failure;
    if (sleepGuard_) {
        try {
            holdAwake(false);
        } catch (...) {
            failure = std::current_exception();
        }
        sleepGuard_ = false;
    }
    extension_.Reset();
    control_.Reset();
    procamp_.Reset();
    appliedSettings_.clear();
    filter_.Reset();
    id_.clear();
    if (failure)
        std::rethrow_exception(failure);
}
Range Camera::range(long property) const {
    Range r;
    long flags = 0;
    if (control_ &&
        SUCCEEDED(control_->GetRange(property, &r.min, &r.max, &r.step, &r.normal, &flags)) &&
        (flags & CameraControl_Flags_Manual) && r.min <= r.max) {
        r.supported = true;
        r.step = std::max(1L, r.step);
    }
    return r;
}
Position Camera::read() const {
    if (!control_)
        throw std::runtime_error("Connect a camera first.");
    Position p;
    long flags;
    check(control_->Get(CameraControl_Pan, &p.pan, &flags), "Read pan");
    check(control_->Get(CameraControl_Tilt, &p.tilt, &flags), "Read tilt");
    check(control_->Get(CameraControl_Zoom, &p.zoom, &flags), "Read zoom");
    return p;
}
static const GUID obsbotXu = {
    0x9a1e7291, 0x6843, 0x4683, {0x6d, 0x92, 0x39, 0xbc, 0x79, 0x06, 0xee, 0x49}};
void Camera::discoverExtension() {
    if (extension_)
        return;
    if (!filter_)
        throw std::runtime_error("Connect a camera first.");
    ComPtr<IKsTopologyInfo> topology;
    check(filter_.As(&topology), "Camera extension topology");
    DWORD count = 0;
    check(topology->get_NumNodes(&count), "Camera extension nodes");
    for (DWORD i = 0; i < count; ++i) {
        ComPtr<IKsControl> candidate;
        if (FAILED(topology->CreateNodeInstance(i, __uuidof(IKsControl),
                                                (void **)candidate.GetAddressOf())))
            continue;
        KSP_NODE prop{};
        prop.Property.Set = obsbotXu;
        prop.Property.Id = 8;
        prop.Property.Flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY;
        prop.NodeId = i;
        unsigned char product[60]{};
        ULONG received = 0;
        if (SUCCEEDED(candidate->KsProperty((PKSPROPERTY)&prop, sizeof(prop), product,
                                            sizeof(product), &received)) &&
            received == 60) {
            extension_ = candidate;
            node_ = i;
            return;
        }
    }
    throw std::runtime_error("OBSBOT extension unit is unavailable on this camera.");
}
bool Camera::sleeping() {
    discoverExtension();
    KSP_NODE prop{};
    prop.Property.Set = obsbotXu;
    prop.Property.Id = 6;
    prop.Property.Flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY;
    prop.NodeId = node_;
    unsigned char status[60]{};
    ULONG bytes = 0;
    check(extension_->KsProperty((PKSPROPERTY)&prop, sizeof(prop), status, 60, &bytes),
          "Read OBSBOT sleep status");
    if (bytes != 60)
        throw std::runtime_error("Incomplete OBSBOT status");
    return status[9] != 1;
}
void Camera::holdAwake(bool hold) {
    discoverExtension();
    KSP_NODE prop{};
    prop.Property.Set = obsbotXu;
    prop.Property.Id = 6;
    prop.Property.Flags = KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY;
    prop.NodeId = node_;
    unsigned char data[60]{};
    data[0] = 0x12;
    data[1] = 1;
    data[2] = hold ? 1 : 0;
    ULONG bytes = 0;
    check(extension_->KsProperty((PKSPROPERTY)&prop, sizeof(prop), data, 60, &bytes),
          "Set no-stream sleep guard");
    sleepGuard_ = hold;
}
void Camera::wake(bool awake) {
    if (awake && !sleepGuard_)
        holdAwake(true);
    if (!awake && sleepGuard_)
        holdAwake(false);
    if (sleeping() == !awake) {
        if (!awake && sleepGuard_)
            holdAwake(false);
        return;
    }
    // Recovered from libdev.dll's legacy encoder and cameraSetDevRunStatusR.
    // Tiny SE requires legacy wake; V3 settings work after the device is awake.
    auto frame = wakePacket(awake, ++sequence_);
    KSP_NODE prop{};
    prop.Property.Set = obsbotXu;
    prop.Property.Id = 2;
    prop.Property.Flags = KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY;
    prop.NodeId = node_;
    ULONG received = 0;
    check(extension_->KsProperty((PKSPROPERTY)&prop, sizeof(prop), frame.data(), 60, &received),
          "Send OBSBOT power command");
    for (int attempt = 0; attempt < 30; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (sleeping() == !awake) {
            if (awake)
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            else if (sleepGuard_)
                holdAwake(false);
            return;
        }
    }
    throw std::runtime_error("Camera did not confirm the requested sleep/wake state.");
}
std::string Camera::diagnostics() {
    discoverExtension();
    std::ostringstream out;
    out << "XU node=" << node_ << "\n";
    for (ULONG id : {2UL, 6UL, 8UL}) {
        KSP_NODE prop{};
        prop.Property.Set = obsbotXu;
        prop.Property.Id = id;
        prop.Property.Flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY;
        prop.NodeId = node_;
        unsigned char data[60]{};
        ULONG n = 0;
        HRESULT hr = extension_->KsProperty((PKSPROPERTY)&prop, sizeof(prop), data, 60, &n);
        out << "selector=" << id << " hr=" << std::hex << (unsigned long)hr << " bytes=" << std::dec
            << n << " ";
        for (auto b : data)
            out << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        out << "\n";
    }
    return out.str();
}
void Camera::setAxis(long property, long value) {
    auto r = range(property);
    if (!r.supported)
        throw std::runtime_error("Camera does not support this PTZ axis.");
    if (value < r.min || value > r.max)
        throw std::runtime_error("Position is outside the camera's reported range.");
    wake(true);
    check(control_->Set(property, quantize(value, r), CameraControl_Flags_Manual), "Move camera");
}
void Camera::move(Position p) {
    auto a = range(CameraControl_Pan), b = range(CameraControl_Tilt), c = range(CameraControl_Zoom);
    if (!a.supported || !b.supported || !c.supported || p.pan < a.min || p.pan > a.max ||
        p.tilt < b.min || p.tilt > b.max || p.zoom < c.min || p.zoom > c.max)
        throw std::runtime_error("Preset is outside this camera's supported PTZ range.");
    wake(true);
    setAxis(CameraControl_Pan, p.pan);
    setAxis(CameraControl_Tilt, p.tilt);
    setAxis(CameraControl_Zoom, p.zoom);
}
void Camera::preview(HWND owner, int width, int height) {
    stopPreview();
    try {
        if (!filter_)
            throw std::runtime_error("Connect a camera first.");
        check(CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                               IID_PPV_ARGS(&graph_)),
              "Create preview");
        ComPtr<ICaptureGraphBuilder2> builder;
        check(CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER,
                               IID_PPV_ARGS(&builder)),
              "Create preview builder");
        check(builder->SetFiltergraph(graph_.Get()), "Set preview graph");
        check(graph_->AddFilter(filter_.Get(), L"Tiny PTZ camera"), "Add camera");
        check(builder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, filter_.Get(), nullptr,
                                    nullptr),
              "Start preview (camera may be in use by OBS)");
        check(graph_.As(&video_), "Preview window");
        check(video_->put_Owner((OAHWND)owner), "Embed preview");
        check(video_->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN),
              "Preview style");
        resizePreview(width, height);
        graph_.As(&media_);
        check(media_->Run(), "Run preview");
        video_->put_Visible(OATRUE);
    } catch (...) {
        stopPreview();
        throw;
    }
}
void Camera::resizePreview(int w, int h) {
    if (video_)
        video_->SetWindowPosition(0, 0, w, h);
}
void Camera::stopPreview() {
    if (media_)
        media_->Stop();
    if (video_) {
        video_->put_Visible(OAFALSE);
        video_->put_Owner(0);
    }
    video_.Reset();
    media_.Reset();
    graph_.Reset();
}
std::filesystem::path portableData() {
    std::wstring p(32768, 0);
    DWORD n = GetModuleFileNameW(nullptr, p.data(), (DWORD)p.size());
    if (!n || n == p.size())
        throw std::runtime_error("Cannot locate portable directory");
    p.resize(n);
    return std::filesystem::path(p).parent_path() / L"data" / L"presets.ptz";
}
} // namespace ptz

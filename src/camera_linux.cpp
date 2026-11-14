#include "core.hpp"
#include "protocol.hpp"
#include "linux_v4l2.hpp"
#include <linux/uvcvideo.h>
#include <linux/usb/video.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <codecvt>
#include <locale>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>
namespace ptz {
std::string utf8(const std::wstring &value) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(value);
}
std::wstring wide(const std::string &value) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(value);
}
static void checkIoctl(int fd, unsigned long operation, void *data, const char *message) {
    int result; do { result=::ioctl(fd,operation,data); } while (result<0 && errno==EINTR);
    if (result<0) throw std::runtime_error(std::string(message)+": "+std::strerror(errno));
}
std::vector<Device> devices() {
    std::vector<Device> result;
    std::vector<std::filesystem::path> paths;
    for (auto folder : {"/dev/v4l/by-id","/dev/v4l/by-path"}) {
        std::error_code error;
        if (std::filesystem::is_directory(folder,error))
            for (auto &entry : std::filesystem::directory_iterator(folder,error)) paths.push_back(entry.path());
    }
    for (int i=0;i<256;++i) {
        auto path=std::filesystem::path("/dev/video"+std::to_string(i));
        if (std::filesystem::exists(path)) paths.push_back(path);
    }
    std::set<std::filesystem::path> visited;
    for (auto &path : paths) {
        std::error_code error; auto real=std::filesystem::canonical(path,error);
        if (error || visited.count(real)) continue;
        int fd=::open(path.c_str(),O_RDWR|O_NONBLOCK|O_CLOEXEC);
        if (fd<0) continue;
        v4l2_capability caps{};
        bool valid=::ioctl(fd,VIDIOC_QUERYCAP,&caps)==0;
        auto capabilities=(caps.capabilities & V4L2_CAP_DEVICE_CAPS)?caps.device_caps:caps.capabilities;
        valid=valid && (capabilities & (V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_VIDEO_CAPTURE_MPLANE));
        // Ignore metadata nodes and ordinary fixed webcams.
        v4l2_queryctrl pan{}; pan.id=V4L2_CID_PAN_ABSOLUTE;
        valid=valid && ::ioctl(fd,VIDIOC_QUERYCTRL,&pan)==0 && !(pan.flags & V4L2_CTRL_FLAG_DISABLED);
        ::close(fd);
        if (valid) {
            visited.insert(real);
            std::string name(reinterpret_cast<const char *>(caps.card),strnlen(reinterpret_cast<const char *>(caps.card),sizeof(caps.card)));
            result.push_back({wide(name),pathText(path)});
        }
    }
    return result;
}
void Camera::connect(const std::wstring &id) {
    disconnect(); auto path=filePath(id);
    if (path.native().rfind("/dev/",0)!=0) throw std::runtime_error("Select a Linux camera, then use Assign presets to camera for imported Windows presets.");
    fd_=::open(path.c_str(),O_RDWR|O_NONBLOCK|O_CLOEXEC);
    if (fd_<0) throw std::runtime_error("Cannot open camera "+path.string()+": "+std::strerror(errno)+". Check video-device permissions and sandbox access.");
    try {
        v4l2_capability caps{}; checkIoctl(fd_,VIDIOC_QUERYCAP,&caps,"Not a V4L2 video device");
        auto capabilities=(caps.capabilities & V4L2_CAP_DEVICE_CAPS)?caps.device_caps:caps.capabilities;
        if (!(capabilities & (V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_VIDEO_CAPTURE_MPLANE))) throw std::runtime_error("Select a video capture node, not its metadata node.");
        id_=id; discoverExtension();
        auto product=extension(8,false); (void)product;
    } catch (...) { disconnect(); throw; }
}
void Camera::disconnect() {
    if (fd_>=0) ::close(fd_);
    fd_=-1; unit_=0; awakeConfirmed_=false; appliedSettings_.clear(); id_.clear();
}
void Camera::discoverExtension() {
    if (unit_) return;
    if (fd_<0) throw std::runtime_error("Connect a camera first.");
    auto node=std::filesystem::canonical(filePath(id_)).filename();
    std::error_code error;
    auto parent=std::filesystem::canonical(std::filesystem::path("/sys/class/video4linux")/node/"device",error);
    if (error) throw std::runtime_error("Cannot locate this camera's USB descriptors in sysfs.");
    while (!parent.empty() && parent!=parent.root_path()) {
        auto descriptor=parent/"descriptors";
        if (std::filesystem::exists(descriptor)) {
            std::ifstream file(descriptor,std::ios::binary);
            if (!file) throw std::runtime_error("Cannot read USB descriptors for OBSBOT extension discovery.");
            std::vector<unsigned char> bytes(std::istreambuf_iterator<char>(file),{});
            unit_=linuxuvc::extensionUnit(bytes); return;
        }
        parent=parent.parent_path();
    }
    throw std::runtime_error("No USB descriptors found. The OBSBOT camera must be exposed through uvcvideo, not a virtual camera.");
}
std::vector<unsigned char> Camera::extension(ULONG selector, bool write, std::vector<unsigned char> data) {
    discoverExtension();
    if (data.empty()) data.resize(60);
    if (data.size()!=60 || selector>255) throw std::runtime_error("Invalid UVC extension request.");
    unsigned char length[2]{};
    uvc_xu_control_query query{}; query.unit=unit_; query.selector=selector; query.query=UVC_GET_LEN; query.size=2; query.data=length;
    checkIoctl(fd_,UVCIOC_CTRL_QUERY,&query,"Read OBSBOT UVC control length");
    if ((length[0]|(length[1]<<8))!=60) throw std::runtime_error("Unsupported OBSBOT UVC control size (expected 60 bytes).");
    query.query=write?UVC_SET_CUR:UVC_GET_CUR; query.size=data.size(); query.data=data.data();
    checkIoctl(fd_,UVCIOC_CTRL_QUERY,&query,"OBSBOT UVC extension transfer");
    return data;
}
Range Camera::standardRange(bool image,int property,long &support) const { return linuxuvc::Controls(fd_).range(image,property,support); }
long Camera::standardRead(bool image,int property,long &flags) const { return linuxuvc::Controls(fd_).read(image,property,flags); }
void Camera::standardWrite(bool image,int property,long value,bool automatic) { linuxuvc::Controls(fd_).write(image,property,value,automatic); }
void Camera::setAntiFlicker(long value) { linuxuvc::Controls(fd_).set(V4L2_CID_POWER_LINE_FREQUENCY,value); }
Range Camera::range(long property) const {
    try { long support=0; auto r=standardRange(false,property,support); r.supported=(support&2)!=0; return r; }
    catch (...) { return {}; }
}
Position Camera::read() {
    if (!connected()) throw std::runtime_error("Connect a camera first.");
    Position p; long flags=0; auto status=extension(6,false);
    if (status[9]==1) { auto angles=decodeMotorAngles(request(0x6604,false)); p.pan=roundedAngle(angles.pan); p.tilt=roundedAngle(angles.tilt); }
    else { p.pan=standardRead(false,CameraControl_Pan,flags); p.tilt=standardRead(false,CameraControl_Tilt,flags); }
    p.zoom=standardRead(false,CameraControl_Zoom,flags); return p;
}
void Camera::waitForPosition(Position target) {
    int stable=0;
    for (int i=0;i<60;++i) {
        auto p=read(); stable=(p.pan==target.pan && p.tilt==target.tilt && p.zoom==target.zoom)?stable+1:0;
        if (stable>=3) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    throw std::runtime_error("Camera did not settle at the requested preset position.");
}
bool Camera::sleeping() { return extension(6,false)[9]!=1; }
void Camera::wake(bool awake,bool explicitRequest) {
    auto status=extension(6,false);
    if (!needsPowerCommand(awake,status[9],awakeConfirmed_,explicitRequest)) return;
    request(0xa0c2,true,{static_cast<unsigned char>(awake?0:1),0,0,0});
    int stable=0;
    for (int i=0;i<50;++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stable=extension(6,false)[9]==(awake?1:3)?stable+1:0;
        if(stable>=2) { awakeConfirmed_=awake; if(awake)std::this_thread::sleep_for(std::chrono::milliseconds(1500)); return; }
    }
    throw std::runtime_error("Camera did not confirm the requested sleep/wake state.");
}
std::string Camera::diagnostics() { return "Linux V4L2/UVC: "+utf8(id_)+"; extension unit="+std::to_string(unit_); }
std::filesystem::path portableData() { return std::filesystem::current_path()/"data"/"presets.ptz"; }
void Camera::preview(HWND,int,int) { throw std::runtime_error("Use OBS Video Capture Device (V4L2) for video on Linux."); }
void Camera::resizePreview(int,int) {}
void Camera::stopPreview() {}
} // namespace ptz

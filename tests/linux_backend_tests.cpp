#include "linux_v4l2.hpp"
#include <iostream>
#include <map>
static void require(bool value,const char *message) { if(!value) throw std::runtime_error(message); }
template<class F> static void rejects(F f,const char *message) { bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad,message); }
int main() {
 try {
    using namespace ptz;
    std::vector<unsigned char> descriptors(18,0); descriptors[0]=18; descriptors[1]=1;
    std::vector<unsigned char> xu{26,0x24,6,7,0x91,0x72,0x1e,0x9a,0x43,0x68,0x83,0x46,0x6d,0x92,0x39,0xbc,0x79,0x06,0xee,0x49,3,1,1,1,0xff,0};
    descriptors.insert(descriptors.end(),xu.begin(),xu.end());
    require(linuxuvc::extensionUnit(descriptors)==7,"discover vendor XU from GUID instead of guessing unit IDs");
    auto invalid=descriptors; invalid.push_back(0);
    rejects([&]{linuxuvc::extensionUnit(invalid);},"reject truncated descriptors");
    xu[3]=9; invalid=descriptors; invalid.insert(invalid.end(),xu.begin(),xu.end());
    rejects([&]{linuxuvc::extensionUnit(invalid);},"reject ambiguous extension IDs");
    invalid=descriptors; invalid[22]^=1;
    rejects([&]{linuxuvc::extensionUnit(invalid);},"reject non-vendor extension GUIDs");
    std::map<unsigned,v4l2_queryctrl> queries;
    std::map<unsigned,long> state;
    std::vector<std::pair<unsigned,long>> writes;
    auto define=[&](unsigned id,int lo,int hi,int step,int normal,unsigned type=V4L2_CTRL_TYPE_INTEGER,unsigned flags=0) {
        v4l2_queryctrl q{};q.id=id;q.minimum=lo;q.maximum=hi;q.step=step;q.default_value=normal;q.type=type;q.flags=flags;
        queries[id]=q;state[id]=normal;
    };
    define(V4L2_CID_PAN_ABSOLUTE,-130*3600,130*3600,3600,0);
    define(V4L2_CID_TILT_ABSOLUTE,-90*3600,90*3600,3600,0);
    define(V4L2_CID_ZOOM_ABSOLUTE,0,12,1,0);
    define(V4L2_CID_EXPOSURE_ABSOLUTE,1,10000,1,156);
    define(V4L2_CID_EXPOSURE_AUTO,0,3,1,3,V4L2_CTRL_TYPE_MENU);
    define(V4L2_CID_WHITE_BALANCE_TEMPERATURE,2800,6500,10,4500);
    define(V4L2_CID_AUTO_WHITE_BALANCE,0,1,1,1,V4L2_CTRL_TYPE_BOOLEAN);
    define(V4L2_CID_GAIN,0,100,1,50,V4L2_CTRL_TYPE_INTEGER,V4L2_CTRL_FLAG_READ_ONLY);
    auto fake=[&](int,unsigned long request,void *data) {
        if(request==VIDIOC_QUERYCTRL) {auto q=static_cast<v4l2_queryctrl*>(data);if(!queries.count(q->id)){errno=EINVAL;return -1;}*q=queries.at(q->id);return 0;}
        if(request==VIDIOC_QUERYMENU) {auto q=static_cast<v4l2_querymenu*>(data);if(q->id==V4L2_CID_EXPOSURE_AUTO&&(q->index==1||q->index==3))return 0;errno=EINVAL;return -1;}
        auto c=static_cast<v4l2_control*>(data);
        if(!state.count(c->id)){errno=EINVAL;return -1;}
        if(request==VIDIOC_G_CTRL){c->value=state.at(c->id);return 0;}
        if(request==VIDIOC_S_CTRL){state[c->id]=c->value;writes.push_back({c->id,c->value});return 0;}
        errno=EINVAL;return -1;
    };
    linuxuvc::Controls controls(-1,fake);
    linuxuvc::Controls denied(-1,[](int,unsigned long,void*) {errno=EACCES;return -1;});
    rejects([&]{long caps=0;denied.range(false,CameraControl_Pan,caps);},"propagate camera permission failures without issuing writes");
    long support=0,flags=0;
    auto pan=controls.range(false,CameraControl_Pan,support);
    require(pan.min==-130&&pan.max==130&&pan.step==1&&support==2,"V4L2 arcseconds normalize to UI degrees");
    controls.write(false,CameraControl_Pan,17,false);
    controls.write(false,CameraControl_Tilt,-4,false);
    require(state[V4L2_CID_PAN_ABSOLUTE]==61200&&state[V4L2_CID_TILT_ABSOLUTE]==-14400,"signed PTZ commands map to arcseconds");
    require(controls.read(false,CameraControl_Tilt,flags)==-4,"negative tilt roundtrips");
    auto exposure=controls.range(false,CameraControl_Exposure,support);
    require(exposure.min==-13&&exposure.max==0&&support==3,"exposure units normalize for Windows preset compatibility");
    controls.write(false,CameraControl_Exposure,-5,false);
    require(state[V4L2_CID_EXPOSURE_AUTO]==V4L2_EXPOSURE_MANUAL&&state[V4L2_CID_EXPOSURE_ABSOLUTE]==313,"manual exposure disables auto before writing 100us units");
    require(controls.read(false,CameraControl_Exposure,flags)==-5&&flags==2,"canonical exposure readback");
    controls.write(false,CameraControl_Exposure,-5,true);
    require(state[V4L2_CID_EXPOSURE_AUTO]==V4L2_EXPOSURE_APERTURE_PRIORITY,"automatic exposure chooses a supported V4L2 menu mode");
    controls.write(true,VideoProcAmp_WhiteBalance,5000,false);
    require(state[V4L2_CID_AUTO_WHITE_BALANCE]==0&&state[V4L2_CID_WHITE_BALANCE_TEMPERATURE]==5000,"manual white balance mode and value");
    controls.write(true,VideoProcAmp_WhiteBalance,0,true);
    require(state[V4L2_CID_AUTO_WHITE_BALANCE]==1&&state[V4L2_CID_WHITE_BALANCE_TEMPERATURE]==5000,"auto mode does not overwrite manual temperature");
    rejects([&]{controls.write(true,VideoProcAmp_WhiteBalance,5001,false);},"reject values off driver step");
    rejects([&]{controls.write(true,VideoProcAmp_Gain,60,false);},"respect read-only controls");
    queries[V4L2_CID_EXPOSURE_ABSOLUTE].minimum=50;queries[V4L2_CID_EXPOSURE_ABSOLUTE].maximum=60;
    rejects([&]{controls.range(false,CameraControl_Exposure,support);},"reject ranges with no representable exposure stop");
    auto root=std::filesystem::temp_directory_path()/("tiny-ptz-linux-"+std::to_string(processId()));
    PresetStore store(root/filePath(L"相機-ä.ptz"));
    store.items={{L"Küche 🍣",L"/dev/v4l/by-id/test-camera",{17,-4,3},{{"exposure",-5,true}}}};
    store.save();store.load();
    require(store.items.size()==1&&store.items[0].name==L"Küche 🍣"&&store.items[0].position.tilt==-4,"UTF-8 filesystem and preset persistence on Linux");
    std::filesystem::remove_all(root);
    Camera camera;
    rejects([&]{camera.connect(L"@device:pnp:windows-camera");},"Windows camera identities require explicit rebind");
    require(!camera.connected(),"failed connect leaves no camera handle");
    std::cout<<"PASS: Linux descriptor GUID discovery, V4L2 mappings/units/auto modes, range/step/read-only guards, Unicode atomic persistence and foreign-device rejection\n";
    return 0;
 } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

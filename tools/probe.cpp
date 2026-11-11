#include <windows.h>
#include <dshow.h>
#include <wrl/client.h>
#include <iostream>
using Microsoft::WRL::ComPtr;
int main(){
 CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 ComPtr<ICreateDevEnum> dev; CoCreateInstance(CLSID_SystemDeviceEnum,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dev));
 ComPtr<IEnumMoniker> en; if(!dev || dev->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,&en,0)!=S_OK) return 2;
 ComPtr<IMoniker> m;
 while(en->Next(1,&m,nullptr)==S_OK){
  ComPtr<IPropertyBag> bag; m->BindToStorage(nullptr,nullptr,IID_PPV_ARGS(&bag));
  VARIANT v; VariantInit(&v); bag->Read(L"FriendlyName",&v,nullptr); std::wcout<<v.bstrVal<<L"\n"; VariantClear(&v);
  ComPtr<IBaseFilter> f; auto hr=m->BindToObject(nullptr,nullptr,IID_PPV_ARGS(&f));
  std::cout<<" bind_hr="<<std::hex<<hr<<std::dec<<"\n";
  ComPtr<IAMCameraControl> c; if(f) f.As(&c);
  for(long p:{long(CameraControl_Pan),long(CameraControl_Tilt),long(CameraControl_Zoom)}) {
   long lo=0,hi=0,step=0,def=0,flags=0,cur=0; hr=c?c->GetRange(p,&lo,&hi,&step,&def,&flags):E_NOINTERFACE;
   std::cout<<" property="<<p<<" hr="<<std::hex<<hr<<std::dec<<" range="<<lo<<","<<hi<<" step="<<step<<" default="<<def<<" flags="<<flags;
   if(c) {hr=c->Get(p,&cur,&flags);std::cout<<" read_hr="<<std::hex<<hr<<std::dec<<" current="<<cur;} std::cout<<"\n";
  }
  m.Reset();
 }
}

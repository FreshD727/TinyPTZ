// Development-only, in-process observation of the hash-checked SDK encoder.
#include "../src/core.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <windows.h>
using Encode = void (*)(void *, void *, bool);
static Encode original;
static unsigned char *site, saved[5];
static void *stub;
static void observer(void *msg, void *out, bool flags) {
    original(msg, out, flags);
    auto *m = (unsigned char *)msg;
    if (*(unsigned short *)(m + 14) == 2 && *(unsigned short *)(m + 16) == 1) {
        FILE *f = nullptr;
        fopen_s(&f, "build/sdk-power-wire.txt", "a");
        if (f) {
            std::fputs("message ", f);
            for (int i = 0; i < 32; ++i)
                std::fprintf(f, "%02x", m[i]);
            std::fputs("\nencoded ", f);
            for (int i = 0; i < 60; ++i)
                std::fprintf(f, "%02x", ((unsigned char *)out)[i]);
            std::fputs("\n", f);
            std::fclose(f);
        }
    }
}
static ptz::ComPtr<IKsControl> held;
using Ks = HRESULT(STDMETHODCALLTYPE *)(IKsControl *, PKSPROPERTY, ULONG, void *, ULONG, ULONG *);
static Ks originalKs;
static void **ksTable;
static HRESULT STDMETHODCALLTYPE observeKs(IKsControl *self, PKSPROPERTY prop, ULONG propSize,
                                           void *data, ULONG dataSize, ULONG *returned) {
    auto *b = (unsigned char *)data;
    if ((prop->Flags & KSPROPERTY_TYPE_SET) && dataSize >= 12 && b[0] == 0xaa) {
        FILE *f = nullptr;
        fopen_s(&f, "build/sdk-power-wire.txt", "a");
        if (f) {
            std::fprintf(f, "KS selector=%lu flags=%lu length=%lu propertySize=%lu\n", prop->Id,
                         prop->Flags, dataSize, propSize);
            for (ULONG i = 0; i < propSize; ++i)
                std::fprintf(f, "%02x", ((unsigned char *)prop)[i]);
            std::fputs("\n", f);
            for (ULONG i = 0; i < dataSize; ++i)
                std::fprintf(f, "%02x", b[i]);
            std::fputs("\n", f);
            std::fclose(f);
        }
    }
    return originalKs(self, prop, propSize, data, dataSize, returned);
}
static bool installKs() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ptz::Camera camera;
    for (auto &d : ptz::devices())
        if (d.name.find(L"Tiny SE") != std::wstring::npos) {
            camera.connect(d.id);
            camera.diagnostics();
            // Development-only access to the known current Camera ComPtr layout.
            held = reinterpret_cast<IKsControl **>(&camera)[6];
            if (!held)
                return false;
            ksTable = *(void ***)held.Get();
            originalKs = (Ks)ksTable[3];
            DWORD protection, ignored;
            VirtualProtect(ksTable + 3, sizeof(void *), PAGE_READWRITE, &protection);
            ksTable[3] = (void *)&observeKs;
            VirtualProtect(ksTable + 3, sizeof(void *), protection, &ignored);
            return true;
        }
    return false;
}
extern "C" __declspec(dllexport) bool install(void *base) {
    if (!installKs())
        return false;
    site = (unsigned char *)base + 0x7b62;
    if (site[0] != 0xe8 || site + 5 + *(int *)(site + 1) != (unsigned char *)base + 0xee430)
        return false;
    original = (Encode)((unsigned char *)base + 0xee430);
    for (uintptr_t offset = 0x10000; offset < 0x70000000; offset += 0x10000) {
        stub = VirtualAlloc((void *)((uintptr_t)base + offset), 4096, MEM_COMMIT | MEM_RESERVE,
                            PAGE_EXECUTE_READWRITE);
        if (stub)
            break;
    }
    if (!stub)
        return false;
    unsigned char jump[] = {0xff, 0x25, 0, 0, 0, 0};
    std::memcpy(stub, jump, 6);
    *(void **)((char *)stub + 6) = (void *)&observer;
    DWORD protection;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &protection))
        return false;
    std::memcpy(saved, site, 5);
    *(int *)(site + 1) = (int)((unsigned char *)stub - (site + 5));
    DWORD ignored;
    VirtualProtect(site, 5, protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    return true;
}
extern "C" __declspec(dllexport) void uninstall() {
    if (!stub)
        return;
    DWORD protection, ignored;
    VirtualProtect(ksTable + 3, sizeof(void *), PAGE_READWRITE, &protection);
    ksTable[3] = (void *)originalKs;
    VirtualProtect(ksTable + 3, sizeof(void *), protection, &ignored);
    held.Reset();
    VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &protection);
    std::memcpy(site, saved, 5);
    VirtualProtect(site, 5, protection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    VirtualFree(stub, 0, MEM_RELEASE);
    stub = nullptr;
}

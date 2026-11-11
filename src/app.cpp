#include "core.hpp"
#include <algorithm>
#include <commctrl.h>
#include <commdlg.h>
#include <functional>
#include <shellapi.h>
namespace ptz {
void showSettings(HWND, Camera &);
}
using namespace ptz;
namespace {
enum {
    Devices = 100,
    Refresh,
    Connect,
    Preview,
    Apply,
    Left,
    Right,
    Up,
    Down,
    ZoomIn,
    ZoomOut,
    Home,
    Read,
    Save,
    Recall,
    Update,
    Rename,
    Remove,
    Import,
    Export,
    Wake,
    Sleep,
    Search,
    Name,
    Pan,
    Tilt,
    Zoom,
    List,
    SettingsButton,
    IncludeSettings,
    SetBoot
};
HINSTANCE instance;
HWND mainWindow, deviceBox, status, previewHost, list, searchBox, nameBox, panBox, tiltBox, zoomBox;
HFONT font, titleFont;
HBRUSH background;
Camera camera;
PresetStore store(portableData());
std::vector<Device> found;
std::vector<size_t> visible;
bool previewOn = false, storeHealthy = true;
std::wstring selectedId;
HWND ctl(const wchar_t *cls, const wchar_t *text, int id, int x, int y, int w, int h,
         DWORD extra = 0) {
    auto c = CreateWindowExW(
        (cls == std::wstring(L"EDIT") || cls == std::wstring(WC_LISTVIEWW)) ? WS_EX_CLIENTEDGE : 0,
        cls, text, WS_CHILD | WS_VISIBLE | extra, x, y, w, h, mainWindow, (HMENU)(INT_PTR)id,
        instance, nullptr);
    SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}
void label(const wchar_t *text, int x, int y, int w = 230) {
    ctl(L"STATIC", text, 0, x, y, w, 23);
}
void button(const wchar_t *text, int id, int x, int y, int w = 100, int h = 32) {
    ctl(L"BUTTON", text, id, x, y, w, h, BS_PUSHBUTTON | WS_TABSTOP);
}
std::wstring text(HWND w) {
    int n = GetWindowTextLengthW(w);
    std::wstring s(n + 1, 0);
    GetWindowTextW(w, s.data(), n + 1);
    s.resize(n);
    return s;
}
void message(const std::wstring &s) {
    SetWindowTextW(status, s.c_str());
}
void error(const std::exception &e) {
    auto s = wide(e.what());
    message(s);
    MessageBoxW(mainWindow, s.c_str(), L"Tiny PTZ", MB_OK | MB_ICONWARNING);
}
void showPosition() {
    auto p = camera.read();
    SetWindowTextW(panBox, std::to_wstring(p.pan).c_str());
    SetWindowTextW(tiltBox, std::to_wstring(p.tilt).c_str());
    SetWindowTextW(zoomBox, std::to_wstring(p.zoom).c_str());
}
long number(HWND w) {
    auto s = text(w);
    size_t end = 0;
    long n = std::stol(s, &end);
    if (end != s.size())
        throw std::runtime_error("Enter whole numbers for pan, tilt and zoom.");
    return n;
}
int selection() {
    int i = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    return i >= 0 && i < (int)visible.size() ? (int)visible[i] : -1;
}
void fillList() {
    ListView_DeleteAllItems(list);
    visible.clear();
    auto q = text(searchBox);
    std::transform(q.begin(), q.end(), q.begin(), towlower);
    for (size_t i = 0; i < store.items.size(); ++i) {
        auto &p = store.items[i];
        if (p.device != selectedId)
            continue;
        auto name = p.name;
        std::transform(name.begin(), name.end(), name.begin(), towlower);
        if (name.find(q) == std::wstring::npos)
            continue;
        visible.push_back(i);
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = (int)visible.size() - 1;
        item.pszText = const_cast<wchar_t *>(p.name.c_str());
        int row = ListView_InsertItem(list, &item);
        auto values = std::to_wstring(p.position.pan) + L" / " + std::to_wstring(p.position.tilt) +
                      L" / " + std::to_wstring(p.position.zoom) + L"  + " +
                      std::to_wstring(p.settings.size()) + L" settings";
        ListView_SetItemText(list, row, 1, values.data());
    }
}
void refresh() {
    camera.disconnect();
    previewOn = false;
    SetWindowTextW(GetDlgItem(mainWindow, Preview), L"Start preview");
    found = devices();
    SendMessage(deviceBox, CB_RESETCONTENT, 0, 0);
    int chosen = 0;
    for (size_t i = 0; i < found.size(); ++i) {
        SendMessage(deviceBox, CB_ADDSTRING, 0, (LPARAM)found[i].name.c_str());
        if (found[i].id == selectedId ||
            selectedId.empty() && found[i].name.find(L"OBSBOT Tiny SE") != std::wstring::npos)
            chosen = (int)i;
    }
    SendMessage(deviceBox, CB_SETCURSEL, chosen, 0);
    selectedId = found.empty() ? L"" : found[chosen].id;
    fillList();
    message(L"Choose your camera and connect. Preview is optional when using OBS.");
}
void connect() {
    int i = (int)SendMessage(deviceBox, CB_GETCURSEL, 0, 0);
    if (i < 0 || i >= (int)found.size())
        throw std::runtime_error("No camera selected.");
    selectedId = found[i].id;
    camera.connect(selectedId);
    camera.wake(true);
    previewOn = false;
    SetWindowTextW(GetDlgItem(mainWindow, Preview), L"Start preview");
    showPosition();
    fillList();
    auto p = camera.range(CameraControl_Pan), t = camera.range(CameraControl_Tilt),
         z = camera.range(CameraControl_Zoom);
    message(L"Connected  |  Pan " + std::to_wstring(p.min) + L".." + std::to_wstring(p.max) +
            L"   Tilt " + std::to_wstring(t.min) + L".." + std::to_wstring(t.max) + L"   Zoom " +
            std::to_wstring(z.min) + L".." + std::to_wstring(z.max));
}
void persist(const std::function<void()> &edit) {
    if (!storeHealthy)
        throw std::runtime_error("Preset file could not be loaded. Repair or back up "
                                 "data/presets.ptz and restart before saving.");
    auto old = store.items;
    try {
        edit();
        store.save();
    } catch (...) {
        store.items = std::move(old);
        throw;
    }
    fillList();
}
std::wstring presetName() {
    auto name = text(nameBox);
    auto first = name.find_first_not_of(L" \t");
    if (first == std::wstring::npos)
        throw std::runtime_error("Enter a preset name.");
    name = name.substr(first, name.find_last_not_of(L" \t") - first + 1);
    return name;
}
void uniqueName(const std::wstring &n, int except = -1) {
    for (size_t i = 0; i < store.items.size(); ++i)
        if ((int)i != except && store.items[i].device == selectedId && store.items[i].name == n)
            throw std::runtime_error("A preset already has that name.");
}
void fileTransfer(bool importing) {
    wchar_t path[32768] = L"presets.ptz";
    OPENFILENAMEW f{sizeof(f)};
    f.hwndOwner = mainWindow;
    f.lpstrFilter = L"Tiny PTZ presets (*.ptz)\0*.ptz\0All files\0*.*\0";
    f.lpstrFile = path;
    f.nMaxFile = 32768;
    f.lpstrDefExt = L"ptz";
    f.Flags =
        OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (importing ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);
    if (!(importing ? GetOpenFileNameW(&f) : GetSaveFileNameW(&f)))
        return;
    PresetStore other(path);
    if (importing) {
        other.load();
        int added = 0;
        persist([&] {
            for (auto p : other.items) {
                auto exists = [&](const std::wstring &name) {
                    return std::any_of(store.items.begin(), store.items.end(), [&](auto &x) {
                        return x.device == p.device && x.name == name;
                    });
                };
                auto base = p.name;
                int n = 2;
                while (exists(p.name))
                    p.name = base + L" (" + std::to_wstring(n++) + L")";
                store.items.push_back(std::move(p));
                ++added;
            }
        });
        message(L"Imported " + std::to_wstring(added) +
                L" presets. Presets retain their original camera identity.");
    } else {
        other.items = store.items;
        other.save();
        message(L"Exported all camera presets.");
    }
}
void command(int id) {
    if (id == SettingsButton) {
        showSettings(mainWindow, camera);
        return;
    }
    if (id == Refresh) {
        refresh();
        return;
    }
    if (id == Connect) {
        connect();
        return;
    }
    if (id == Import || id == Export) {
        fileTransfer(id == Import);
        return;
    }
    if (id == Preview) {
        if (previewOn) {
            camera.stopPreview();
            previewOn = false;
        } else {
            RECT r;
            GetClientRect(previewHost, &r);
            camera.preview(previewHost, r.right, r.bottom);
            previewOn = true;
        }
        SetWindowTextW(GetDlgItem(mainWindow, Preview),
                       previewOn ? L"Stop preview" : L"Start preview");
        return;
    }
    if (id == Wake || id == Sleep) {
        camera.wake(id == Wake);
        message(id == Wake ? L"Wake sent. Allow the gimbal to settle, then read position."
                           : L"Camera sleep sent.");
        return;
    }
    if (id == Read) {
        showPosition();
        return;
    }
    if (id == Apply) {
        camera.move({number(panBox), number(tiltBox), number(zoomBox)});
        message(L"Position sent. Allow the camera to settle before saving a preset.");
        return;
    }
    if (id >= Left && id <= Home) {
        camera.wake(true);
        auto p = camera.read();
        long prop = CameraControl_Pan, v = p.pan;
        switch (id) {
        case Left:
            v += 5;
            break;
        case Right:
            v -= 5;
            break;
        case Up:
            prop = CameraControl_Tilt;
            v = p.tilt + 5;
            break;
        case Down:
            prop = CameraControl_Tilt;
            v = p.tilt - 5;
            break;
        case ZoomIn:
            prop = CameraControl_Zoom;
            v = p.zoom + 1;
            break;
        case ZoomOut:
            prop = CameraControl_Zoom;
            v = p.zoom - 1;
            break;
        case Home:
            camera.move({camera.range(CameraControl_Pan).normal,
                         camera.range(CameraControl_Tilt).normal,
                         camera.range(CameraControl_Zoom).normal});
            message(L"Sent camera defaults.");
            return;
        }
        camera.setAxis(prop, quantize(v, camera.range(prop)));
        message(L"Move sent. Use Read position after the camera settles.");
        return;
    }
    if (id == Save) {
        if (!camera.connected())
            throw std::runtime_error("Connect your camera before saving.");
        if (camera.sleeping())
            throw std::runtime_error("Wake and position the camera before saving a preset.");
        auto name = presetName();
        uniqueName(name);
        auto p = camera.read();
        persist([&] {
            store.items.push_back({name, camera.id(), p,
                                   SendMessageW(GetDlgItem(mainWindow, IncludeSettings),
                                                BM_GETCHECK, 0, 0) == BST_CHECKED
                                       ? camera.snapshotSettings()
                                       : std::vector<SettingValue>{}});
        });
        message(L"Saved \"" + name + L"\" with " +
                std::to_wstring(store.items.back().settings.size()) + L" camera settings.");
        return;
    }
    int index = selection();
    if (index < 0)
        throw std::runtime_error("Select a preset first.");
    auto p = store.items[index];
    if (id == SetBoot) {
        camera.setBootPreset(p);
        message(L"Stored PTZ from \"" + p.name +
                L"\" in the camera as its boot position. No app is needed at startup.");
    } else if (id == Recall) {
        if (camera.id() != p.device)
            throw std::runtime_error("Connect the camera associated with this preset.");
        camera.recall(p);
        message(L"Recalled \"" + p.name + L"\".");
    } else if (id == Update) {
        if (camera.id() != p.device)
            throw std::runtime_error("Connect this preset's camera first.");
        if (camera.sleeping())
            throw std::runtime_error("Wake and position the camera before updating a preset.");
        auto position = camera.read();
        persist([&] {
            store.items[index].position = position;
            store.items[index].settings = SendMessageW(GetDlgItem(mainWindow, IncludeSettings),
                                                       BM_GETCHECK, 0, 0) == BST_CHECKED
                                              ? camera.snapshotSettings()
                                              : std::vector<SettingValue>{};
        });
        message(L"Updated selected preset from current position and selected settings option.");
    } else if (id == Rename) {
        auto name = presetName();
        uniqueName(name, index);
        persist([&] { store.items[index].name = name; });
    } else if (id == Remove) {
        if (MessageBoxW(mainWindow, (L"Delete preset \"" + p.name + L"\"?").c_str(),
                        L"Delete preset", MB_YESNO | MB_ICONQUESTION) == IDYES)
            persist([&] { store.items.erase(store.items.begin() + index); });
    }
}
LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    try {
        switch (msg) {
        case WM_CREATE: {
            mainWindow = w;
            background = CreateSolidBrush(RGB(245, 247, 250));
            font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                               CLEARTYPE_QUALITY, 0, L"Segoe UI");
            titleFont = CreateFontW(-30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            auto title = ctl(L"STATIC", L"Tiny PTZ", 0, 24, 18, 260, 42);
            SendMessage(title, WM_SETFONT, (WPARAM)titleFont, TRUE);
            label(L"Portable camera control  /  Unlimited named presets", 24, 64, 700);
            deviceBox = ctl(WC_COMBOBOXW, L"", Devices, 24, 105, 570, 280,
                            CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
            button(L"Refresh", Refresh, 606, 102, 90);
            button(L"Connect", Connect, 706, 102, 120);
            previewHost = ctl(L"STATIC",
                              L"Preview is off\n\nCamera control works without preview.\nLeave "
                              L"preview off when OBS uses the camera.",
                              0, 24, 158, 520, 290, SS_CENTER | SS_CENTERIMAGE | WS_CLIPCHILDREN);
            button(L"Start preview", Preview, 24, 461, 140);
            button(L"Settings...", SettingsButton, 574, 481, 248);
            ctl(L"BUTTON", L"Include tracking, image and device settings when saving presets",
                IncludeSettings, 24, 497, 530, 24, BS_AUTOCHECKBOX | WS_TABSTOP);
            SendMessageW(GetDlgItem(w, IncludeSettings), BM_SETCHECK, BST_CHECKED, 0);
            button(L"Read position", Read, 178, 461, 140);
            button(L"Wake", Wake, 332, 461, 100);
            button(L"Sleep", Sleep, 444, 461, 100);
            label(L"CAMERA POSITION", 574, 158);
            label(L"Pan", 574, 196, 50);
            label(L"Tilt", 660, 196, 50);
            label(L"Zoom", 746, 196, 60);
            panBox = ctl(L"EDIT", L"0", Pan, 574, 222, 76, 30, WS_TABSTOP | ES_AUTOHSCROLL);
            tiltBox = ctl(L"EDIT", L"0", Tilt, 660, 222, 76, 30, WS_TABSTOP | ES_AUTOHSCROLL);
            zoomBox = ctl(L"EDIT", L"0", Zoom, 746, 222, 76, 30, WS_TABSTOP | ES_AUTOHSCROLL);
            button(L"Apply position", Apply, 574, 264, 248);
            button(L"Up", Up, 660, 310, 76);
            button(L"Left", Left, 574, 350, 76);
            button(L"Center", Home, 660, 350, 76);
            button(L"Right", Right, 746, 350, 76);
            button(L"Down", Down, 660, 390, 76);
            button(L"Zoom -", ZoomOut, 574, 439, 118);
            button(L"Zoom +", ZoomIn, 704, 439, 118);
            label(L"PRESET LIBRARY", 24, 520, 250);
            searchBox = ctl(L"EDIT", L"", Search, 574, 513, 248, 30, WS_TABSTOP | ES_AUTOHSCROLL);
            SendMessageW(searchBox, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search presets...");
            list = ctl(WC_LISTVIEWW, L"", List, 24, 554, 798, 205,
                       LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP);
            ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            LVCOLUMNW col{};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.cx = 485;
            col.pszText = const_cast<wchar_t *>(L"Preset name");
            ListView_InsertColumn(list, 0, &col);
            col.cx = 280;
            col.pszText = const_cast<wchar_t *>(L"Pan / Tilt / Zoom (driver units)");
            ListView_InsertColumn(list, 1, &col);
            nameBox = ctl(L"EDIT", L"", Name, 24, 773, 310, 32, WS_TABSTOP | ES_AUTOHSCROLL);
            SendMessageW(nameBox, EM_SETCUEBANNER, TRUE, (LPARAM)L"Name for new preset or rename");
            button(L"Save current", Save, 346, 773, 144);
            button(L"Recall selected", Recall, 502, 773, 154);
            button(L"Update selected", Update, 668, 773, 154);
            button(L"Rename", Rename, 24, 819, 100);
            button(L"Delete", Remove, 136, 819, 100);
            button(L"Set as boot position", SetBoot, 248, 819, 210);
            button(L"Import...", Import, 574, 819, 118);
            button(L"Export...", Export, 704, 819, 118);
            status = ctl(L"STATIC", L"", 0, 24, 875, 798, 64);
            try {
                store.load();
            } catch (const std::exception &e) {
                storeHealthy = false;
                error(e);
            }
            refresh();
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == Search && HIWORD(wp) == EN_CHANGE) {
                fillList();
                return 0;
            }
            if (id == Devices && HIWORD(wp) == CBN_SELCHANGE) {
                camera.disconnect();
                previewOn = false;
                SetWindowTextW(GetDlgItem(w, Preview), L"Start preview");
                int i = (int)SendMessage(deviceBox, CB_GETCURSEL, 0, 0);
                selectedId = found.at(i).id;
                fillList();
                message(L"Camera changed. Click Connect.");
                return 0;
            }
            if (HIWORD(wp) == BN_CLICKED &&
                (id >= Refresh && id <= Sleep || id == SettingsButton || id == SetBoot)) {
                command(id);
                return 0;
            }
            break;
        }
        case WM_NOTIFY: {
            auto n = (NMHDR *)lp;
            if (n->idFrom == List && n->code == NM_DBLCLK) {
                command(Recall);
                return 0;
            }
            if (n->idFrom == List && n->code == LVN_ITEMCHANGED) {
                int i = selection();
                if (i >= 0)
                    SetWindowTextW(nameBox, store.items[i].name.c_str());
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            auto dc = (HDC)wp;
            SetTextColor(dc, RGB(28, 39, 56));
            SetBkColor(dc, RGB(245, 247, 250));
            return (LRESULT)background;
        }
        case WM_CLOSE:
            DestroyWindow(w);
            return 0;
        case WM_DESTROY:
            try {
                camera.disconnect();
            } catch (const std::exception &e) {
                error(e);
            }
            DeleteObject(font);
            DeleteObject(titleFont);
            DeleteObject(background);
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception &e) {
        error(e);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}
} // namespace
int WINAPI wWinMain(HINSTANCE h, HINSTANCE, PWSTR, int show) {
    instance = h;
    SetProcessDPIAware();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return 1;
    INITCOMMONCONTROLSEX ic{sizeof(ic), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&ic);
    WNDCLASSW c{};
    c.lpfnWndProc = proc;
    c.hInstance = h;
    c.lpszClassName = L"TinyPTZPortable";
    c.hCursor = LoadCursor(nullptr, IDC_ARROW);
    c.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&c);
    RECT r{0, 0, 850, 950};
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    auto w = CreateWindowW(c.lpszClassName, L"Tiny PTZ — Portable OBSBOT Camera Control",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                           CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, h,
                           nullptr);
    if (!w) {
        CoUninitialize();
        return 1;
    }
    ShowWindow(w, show);
    MSG m{};
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(w, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    CoUninitialize();
    return (int)m.wParam;
}

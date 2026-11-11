#include "core.hpp"
#include "live_slider.hpp"
#include "numeric_slider.hpp"
#include <algorithm>
#include <cmath>
#include <commctrl.h>
#include <functional>
#include <iomanip>
#include <sstream>
namespace ptz {
namespace {
struct Editor {
    Camera *camera;
    HWND owner, window, category, list, value, choices, automatic, range, details, apply, defaults,
        selectedDefault;
    NumericSlider numeric;
    LiveSliderEdits<std::string, double> live;
    std::function<void(const SettingValue &, bool)> liveApply;
    HFONT font;
    bool refreshing = false;
    std::vector<Setting> settings;
    std::vector<size_t> rows;
};
std::wstring format(double n) {
    std::wostringstream s;
    s << std::setprecision(8) << n;
    return s.str();
}
HWND control(Editor &e, const wchar_t *type, const wchar_t *label, int id, int x, int y, int w,
             int h, DWORD style = 0) {
    HWND c = CreateWindowExW(type == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0, type, label,
                             WS_CHILD | WS_VISIBLE | style, x, y, w, h, e.window,
                             (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)e.font, TRUE);
    return c;
}
Setting *selected(Editor &e) {
    int row = ListView_GetNextItem(e.list, -1, LVNI_SELECTED);
    return row >= 0 && row < (int)e.rows.size() ? &e.settings[e.rows[row]] : nullptr;
}
void fill(Editor &e) {
    e.refreshing = true;
    ListView_DeleteAllItems(e.list);
    e.rows.clear();
    int group = (int)SendMessageW(e.category, CB_GETCURSEL, 0, 0);
    const wchar_t *groups[] = {L"Tracking", L"Image", L"Device"};
    for (size_t i = 0; i < e.settings.size(); ++i) {
        auto &s = e.settings[i];
        if (s.group != groups[group])
            continue;
        e.rows.push_back(i);
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = (int)e.rows.size() - 1;
        item.pszText = s.name.data();
        int row = ListView_InsertItem(e.list, &item);
        std::wstring value = s.readable ? format(s.current.value) : L"Unavailable";
        if (!s.readable && s.writable)
            value = s.details.find(L"Last requested") == 0
                        ? format(s.current.value) + L" (requested)"
                        : L"Set only";
        for (auto &v : s.choices)
            if (s.readable && v.first == s.current.value)
                value = v.second;
        if (s.current.automatic)
            value = L"Auto (" + value + L")";
        ListView_SetItemText(e.list, row, 1, value.data());
        ListView_SetItemText(e.list, row, 2, s.details.data());
    }
    EnableWindow(e.apply, FALSE);
    EnableWindow(e.numeric.track, FALSE);
    EnableWindow(e.selectedDefault, FALSE);
    EnableWindow(e.defaults, group == 1);
    e.refreshing = false;
}
void updateManualControls(Editor &e) {
    auto *s = selected(e);
    bool enabled =
        s && s->writable &&
        !(s->autoSupported && SendMessageW(e.automatic, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(e.value, enabled);
    EnableWindow(e.numeric.track,
                 enabled && e.numeric.valid && e.numeric.ticks > 0 && s->choices.empty());
}
void showSelection(Editor &e);
void refreshSettings(Editor &e, std::vector<Setting> settings) {
    auto *old = selected(e);
    std::string key = old ? old->current.key : "";
    e.settings = std::move(settings);
    fill(e);
    for (size_t row = 0; row < e.rows.size(); ++row)
        if (e.settings[e.rows[row]].current.key == key) {
            ListView_SetItemState(e.list, (int)row, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(e.list, (int)row, FALSE);
            showSelection(e);
            break;
        }
}
constexpr UINT_PTR liveTimer = 7001;
void processLive(Editor &e, std::uint64_t now, bool force = false) {
    e.live.tick(
        now, GetCapture() == e.numeric.track,
        [&](const std::string &key, double value, bool verify) {
            SettingValue change{key, value, false};
            if (e.liveApply)
                e.liveApply(change, verify);
            else if (verify)
                e.camera->verifySetting(change);
            else
                e.camera->applySetting(change, false);
            auto i = std::find_if(e.settings.begin(), e.settings.end(),
                                  [&](auto &s) { return s.current.key == key; });
            if (i != e.settings.end()) {
                i->current = change;
                std::wstring text = format(value) + (verify && i->readable ? L"" : L" (sent)");
                for (size_t row = 0; row < e.rows.size(); ++row)
                    if (e.settings[e.rows[row]].current.key == key)
                        ListView_SetItemText(e.list, (int)row, 1, text.data());
                auto *current = selected(e);
                if (current && current->current.key == key)
                    SetWindowTextW(
                        e.details,
                        verify
                            ? L"Live setting confirmed by camera readback."
                            : L"Live update sent. Readback follows when you release the slider.");
            }
        },
        force);
    if (!e.live.active())
        KillTimer(e.window, liveTimer);
}
void flushLive(Editor &e) {
    try {
        processLive(e, GetTickCount64(), true);
    } catch (...) {
        e.live.clear();
        KillTimer(e.window, liveTimer);
        throw;
    }
}
void load(Editor &e) {
    flushLive(e);
    e.live.clear();
    refreshSettings(e, e.camera->settings());
}
void showSelection(Editor &e) {
    auto *s = selected(e);
    if (!s) {
        EnableWindow(e.apply, FALSE);
        EnableWindow(e.numeric.track, FALSE);
        EnableWindow(e.selectedDefault, FALSE);
        return;
    }
    SetWindowTextW(e.value, format(s->current.value).c_str());
    SendMessageW(e.choices, CB_RESETCONTENT, 0, 0);
    int current = 0;
    for (size_t i = 0; i < s->choices.size(); ++i) {
        SendMessageW(e.choices, CB_ADDSTRING, 0, (LPARAM)s->choices[i].second.c_str());
        if (s->choices[i].first == s->current.value)
            current = (int)i;
    }
    SendMessageW(e.choices, CB_SETCURSEL, current, 0);
    ShowWindow(e.choices, s->choices.empty() ? SW_HIDE : SW_SHOW);
    ShowWindow(e.value, s->choices.empty() ? SW_SHOW : SW_HIDE);
    ShowWindow(e.numeric.track, s->choices.empty() ? SW_SHOW : SW_HIDE);
    e.numeric.configure(s->minimum, s->maximum, s->step, s->current.value, s->writable);
    SendMessageW(e.automatic, BM_SETCHECK, s->current.automatic ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(e.automatic, s->autoSupported && s->writable);
    updateManualControls(e);
    EnableWindow(e.choices, s->writable);
    EnableWindow(e.apply, s->writable);
    EnableWindow(e.selectedDefault, s->writable && s->group == L"Image" && s->defaultAvailable);
    SetWindowTextW(e.range,
                   (L"Range: " + format(s->minimum) + L" to " + format(s->maximum) + L"   Step: " +
                    format(s->step) +
                    (s->defaultAvailable ? L"   Default: " + (s->defaultValue.automatic
                                                                  ? std::wstring(L"Auto")
                                                                  : format(s->defaultValue.value))
                                         : L""))
                       .c_str());
    SetWindowTextW(e.details, (s->name + L"\n" + s->details).c_str());
}
LRESULT CALLBACK procedure(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    auto *e = (Editor *)GetWindowLongPtrW(w, GWLP_USERDATA);
    try {
        if (msg == WM_CREATE) {
            e = (Editor *)((CREATESTRUCTW *)lp)->lpCreateParams;
            e->window = w;
            SetWindowLongPtrW(w, GWLP_USERDATA, (LONG_PTR)e);
            e->font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            control(*e, L"STATIC", L"Camera settings", 0, 20, 18, 720, 24);
            e->category =
                control(*e, WC_COMBOBOXW, L"", 1, 20, 50, 230, 200, CBS_DROPDOWNLIST | WS_TABSTOP);
            for (auto *g : {L"Tracking", L"Image", L"Device"})
                SendMessageW(e->category, CB_ADDSTRING, 0, (LPARAM)g);
            SendMessageW(e->category, CB_SETCURSEL, 0, 0);
            control(*e, L"BUTTON", L"Read from camera", 2, 590, 47, 170, 32, WS_TABSTOP);
            e->list = control(*e, WC_LISTVIEWW, L"", 3, 20, 95, 740, 350,
                              LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP);
            ListView_SetExtendedListViewStyle(e->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            LVCOLUMNW col{};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            const wchar_t *names[] = {L"Setting", L"Current value", L"Availability / readback"};
            int widths[] = {285, 125, 310};
            for (int i = 0; i < 3; ++i) {
                col.pszText = (wchar_t *)names[i];
                col.cx = widths[i];
                ListView_InsertColumn(e->list, i, &col);
            }
            e->details = control(
                *e, L"STATIC", L"Select a setting to edit. Unsupported controls cannot be applied.",
                0, 20, 455, 740, 44);
            e->value = control(*e, L"EDIT", L"", 4, 20, 505, 240, 30, ES_AUTOHSCROLL | WS_TABSTOP);
            e->choices =
                control(*e, WC_COMBOBOXW, L"", 5, 20, 505, 430, 250, CBS_DROPDOWNLIST | WS_TABSTOP);
            ShowWindow(e->choices, SW_HIDE);
            e->automatic = control(*e, L"BUTTON", L"Automatic", 6, 470, 505, 130, 30,
                                   BS_AUTOCHECKBOX | WS_TABSTOP);
            e->numeric.track = control(*e, TRACKBAR_CLASSW, L"", 11, 20, 540, 740, 32,
                                       TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP);
            e->numeric.edit = e->value;
            e->range = control(*e, L"STATIC", L"", 0, 20, 580, 740, 25);
            e->apply =
                control(*e, L"BUTTON", L"Apply selected setting", 7, 20, 615, 220, 36, WS_TABSTOP);
            e->defaults =
                control(*e, L"BUTTON", L"Restore image defaults", 9, 255, 615, 220, 36, WS_TABSTOP);
            e->selectedDefault =
                control(*e, L"BUTTON", L"Default selected", 10, 485, 615, 135, 36, WS_TABSTOP);
            control(*e, L"BUTTON", L"Close", 8, 630, 615, 130, 36, WS_TABSTOP);
            control(*e, L"STATIC",
                    L"Preset saving captures readable settings. Set-only values are included after "
                    L"you apply them.\nTracking can move the camera after recall. PC-side beauty "
                    L"and blur effects are not camera controls.",
                    0, 20, 675, 740, 55);
            load(*e);
            return 0;
        }
        if (!e)
            return DefWindowProcW(w, msg, wp, lp);
        if (msg == WM_HSCROLL && (HWND)lp == e->numeric.track) {
            auto *s = selected(*e);
            if (!s || !s->writable || !IsWindowEnabled(e->numeric.track))
                return 0;
            e->numeric.fromTrack();
            wchar_t text[128]{};
            GetWindowTextW(e->value, text, 128);
            int action = LOWORD(wp);
            bool running = e->live.active();
            e->live.stage(s->current.key, std::stod(text),
                          action == TB_ENDTRACK || action == TB_THUMBPOSITION, GetTickCount64());
            if (!running)
                SetTimer(w, liveTimer, 25, nullptr);
            return 0;
        }
        if (msg == WM_TIMER && wp == liveTimer) {
            try {
                processLive(*e, GetTickCount64());
            } catch (const std::exception &ex) {
                e->live.clear();
                KillTimer(w, liveTimer);
                SetWindowTextW(e->details, (L"Live update failed: " + wide(ex.what())).c_str());
            }
            return 0;
        }
        if (msg == WM_COMMAND) {
            int id = LOWORD(wp);
            if (id == 2 || id == 7 || id == 8 || id == 9 || id == 10)
                flushLive(*e);
            if (id == 4 && HIWORD(wp) == EN_CHANGE) {
                e->numeric.fromEdit();
                return 0;
            }
            if (id == 6 && HIWORD(wp) == BN_CLICKED) {
                updateManualControls(*e);
                return 0;
            }
            if (id == 1 && HIWORD(wp) == CBN_SELCHANGE) {
                fill(*e);
                return 0;
            }
            if (id == 2) {
                load(*e);
                return 0;
            }
            if (id == 10) {
                auto *s = selected(*e);
                if (s && s->writable && s->defaultAvailable) {
                    e->camera->applySetting(s->defaultValue);
                    load(*e);
                }
                return 0;
            }
            if (id == 9) {
                e->camera->restoreImageDefaults();
                load(*e);
                SetWindowTextW(e->details, L"Supported image settings restored to defaults.");
                return 0;
            }
            if (id == 8) {
                DestroyWindow(w);
                return 0;
            }
            if (id == 7) {
                auto *s = selected(*e);
                if (!s || !s->writable)
                    return 0;
                auto value = s->current;
                if (!s->choices.empty()) {
                    int i = (int)SendMessageW(e->choices, CB_GETCURSEL, 0, 0);
                    if (i < 0)
                        throw std::runtime_error("Select a value");
                    value.value = s->choices.at(i).first;
                } else {
                    wchar_t text[128]{};
                    GetWindowTextW(e->value, text, 128);
                    size_t end;
                    value.value = std::stod(text, &end);
                    if (end != std::wstring(text).size())
                        throw std::runtime_error("Enter a valid numeric value");
                }
                value.automatic = IsWindowEnabled(e->automatic) &&
                                  SendMessageW(e->automatic, BM_GETCHECK, 0, 0) == BST_CHECKED;
                e->camera->applySetting(value);
                load(*e);
                auto i = std::find_if(e->settings.begin(), e->settings.end(),
                                      [&](auto &s) { return s.current.key == value.key; });
                if (i != e->settings.end() && i->readable &&
                    (i->current.automatic != value.automatic ||
                     !value.automatic && std::abs(i->current.value - value.value) > .02))
                    throw std::runtime_error("Camera readback differs from the requested setting; "
                                             "see its current value.");
                SetWindowTextW(e->details,
                               i != e->settings.end() && i->readable
                                   ? L"Setting applied and verified by camera readback."
                                   : L"Setting request sent. The device does not expose readback; "
                                     L"the last request can be saved with presets.");
                return 0;
            }
        }
        if (msg == WM_NOTIFY && ((NMHDR *)lp)->idFrom == 3 &&
            ((NMHDR *)lp)->code == LVN_ITEMCHANGED) {
            if (!e->refreshing && (((NMLISTVIEW *)lp)->uChanged & LVIF_STATE))
                showSelection(*e);
            return 0;
        }
        if (msg == WM_DESTROY) {
            KillTimer(w, liveTimer);
            e->live.clear();
            return 0;
        }
        if (msg == WM_CLOSE) {
            flushLive(*e);
            DestroyWindow(w);
            return 0;
        }
    } catch (const std::exception &ex) {
        MessageBoxW(w, wide(ex.what()).c_str(), L"Camera settings", MB_OK | MB_ICONWARNING);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}
} // namespace
void showSettings(HWND owner, Camera &camera) {
    if (!camera.connected())
        throw std::runtime_error("Connect a camera first");
    camera.wake(true);
    Editor editor{};
    editor.camera = &camera;
    editor.owner = owner;
    WNDCLASSW c{};
    c.lpfnWndProc = procedure;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"TinyPTZSettings";
    c.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    c.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&c);
    RECT r{0, 0, 780, 750};
    AdjustWindowRect(&r, WS_CAPTION | WS_SYSMENU, FALSE);
    HWND w = CreateWindowExW(
        WS_EX_DLGMODALFRAME, c.lpszClassName, L"Tiny PTZ — Tracking, Image and Device Settings",
        WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        owner, nullptr, c.hInstance, &editor);
    if (!w)
        throw std::runtime_error("Could not create settings editor");
    EnableWindow(owner, FALSE);
    ShowWindow(w, SW_SHOW);
    MSG m;
    while (IsWindow(w) && GetMessageW(&m, nullptr, 0, 0) > 0)
        if (!IsDialogMessageW(w, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
}
} // namespace ptz

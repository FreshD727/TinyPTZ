// Exercise native controls under a hidden parent; no desktop automation/camera.
#include "../src/settings_ui.cpp"
#include <iostream>
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    using namespace ptz;
    try {
        INITCOMMONCONTROLSEX init{sizeof(init), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
        InitCommonControlsEx(&init);
        Editor e{};
        e.window = CreateWindowExW(0, L"STATIC", L"Hidden settings test", WS_OVERLAPPED, 0, 0, 780,
                                   750, nullptr, nullptr, nullptr, nullptr);
        require(e.window != nullptr, "hidden parent creation");
        e.font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        e.category = control(e, WC_COMBOBOXW, L"", 1, 0, 0, 100, 100, CBS_DROPDOWNLIST);
        for (auto *name : {L"Tracking", L"Image", L"Device"})
            SendMessageW(e.category, CB_ADDSTRING, 0, (LPARAM)name);
        SendMessageW(e.category, CB_SETCURSEL, 1, 0);
        e.list = control(e, WC_LISTVIEWW, L"", 3, 0, 0, 400, 300, LVS_REPORT | LVS_SINGLESEL);
        e.value = control(e, L"EDIT", L"", 4, 0, 0, 100, 30);
        e.numeric.track =
            control(e, TRACKBAR_CLASSW, L"", 11, 0, 0, 500, 32, TBS_HORZ | TBS_AUTOTICKS);
        e.numeric.edit = e.value;
        e.choices = control(e, WC_COMBOBOXW, L"", 5, 0, 0, 100, 100, CBS_DROPDOWNLIST);
        e.automatic = control(e, L"BUTTON", L"Auto", 6, 0, 0, 100, 30, BS_AUTOCHECKBOX);
        e.apply = control(e, L"BUTTON", L"Apply", 7, 0, 0, 100, 30);
        e.defaults = control(e, L"BUTTON", L"Defaults", 9, 0, 0, 100, 30);
        e.selectedDefault = control(e, L"BUTTON", L"Default", 10, 0, 0, 100, 30);
        e.range = control(e, L"STATIC", L"", 0, 0, 0, 100, 30);
        e.details = control(e, L"STATIC", L"", 0, 0, 0, 100, 30);
        SetWindowLongPtrW(e.window, GWLP_USERDATA, (LONG_PTR)&e);
        SetWindowLongPtrW(e.window, GWLP_WNDPROC, (LONG_PTR)procedure);
        Setting numeric;
        numeric.current = {"exposure_compensation", 0, false};
        numeric.group = L"Image";
        numeric.name = L"Exposure Compensation";
        numeric.minimum = -9;
        numeric.maximum = 9;
        numeric.readable = numeric.writable = numeric.defaultAvailable = true;
        numeric.defaultValue = numeric.current;
        Setting choice = numeric;
        choice.current.key = "hdr";
        choice.choices = {{0, L"Off"}, {1, L"On"}};
        refreshSettings(e, {numeric, choice});
        ListView_SetItemState(e.list, 0, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        showSelection(e);
        for (double value : {-9., 9., 0.}) {
            numeric.current.value = value;
            refreshSettings(e, {numeric, choice}); // Readback after Apply.
            require(selected(e) && selected(e)->current.key == numeric.current.key,
                    "numeric selection must survive apply/readback");
            require(IsWindowEnabled(e.apply),
                    "Apply must remain enabled for repeated numeric edits");
            SetWindowTextW(e.value, L"1");
            require(IsWindowEnabled(e.apply), "editing again must permit Apply");
        }
        auto fieldText = [&] {
            wchar_t text[128]{};
            GetWindowTextW(e.value, text, 128);
            return std::wstring(text);
        };
        auto drag = [&](int tick) {
            SendMessageW(e.numeric.track, TBM_SETPOS, TRUE, tick);
            SendMessageW(e.window, WM_HSCROLL, TB_THUMBTRACK, (LPARAM)e.numeric.track);
        };
        drag(0);
        require(fieldText() == L"-9", "slider minimum maps to negative numeric value");
        drag(18);
        require(fieldText() == L"9", "slider maximum maps to numeric value");
        SetWindowTextW(e.value, L"-4");
        require(SendMessageW(e.numeric.track, TBM_GETPOS, 0, 0) == 5,
                "typed negative value updates slider");
        SetWindowTextW(e.value, L"-");
        require(fieldText() == L"-" && SendMessageW(e.numeric.track, TBM_GETPOS, 0, 0) == 5,
                "partial numeric text must survive editing");
        auto fractional = numeric;
        fractional.current = {"composition_x", .12, false};
        fractional.minimum = -1;
        fractional.maximum = 1;
        fractional.step = .01;
        refreshSettings(e, {fractional, choice});
        ListView_SetItemState(e.list, 0, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        showSelection(e);
        require(SendMessageW(e.numeric.track, TBM_GETPOS, 0, 0) == 112,
                "fractional readback positions slider");
        drag(113);
        require(std::abs(std::stod(fieldText()) - .13) < 1e-12, "fractional slider step");
        SetWindowTextW(e.value, L"-0.25");
        require(SendMessageW(e.numeric.track, TBM_GETPOS, 0, 0) == 75,
                "typed fractional value updates slider");
        auto wide = numeric;
        wide.minimum = 0;
        wide.maximum = 100000;
        wide.step = 1;
        refreshSettings(e, {wide, choice});
        ListView_SetItemState(e.list, 0, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        showSelection(e);
        drag(90000);
        require(fieldText() == L"90000", "slider range is not truncated to 16 bits");
        auto automatic = numeric;
        automatic.autoSupported = true;
        automatic.current.automatic = true;
        refreshSettings(e, {automatic, choice});
        require(!IsWindowEnabled(e.numeric.track) && !IsWindowEnabled(e.value),
                "automatic mode disables manual slider and text");
        SendMessageW(e.automatic, BM_CLICK, 0, 0);
        require(IsWindowEnabled(e.numeric.track) && IsWindowEnabled(e.value),
                "disabling automatic mode enables manual input");
        require(IsWindowEnabled(e.apply), "slider/manual mode edits preserve Apply availability");
        ListView_SetItemState(e.list, 1, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        showSelection(e);
        for (double value : {1., 0., 1.}) {
            choice.current.value = value;
            refreshSettings(e, {numeric, choice});
            require(selected(e) && selected(e)->current.key == "hdr",
                    "dropdown selection survives refresh");
            require(IsWindowEnabled(e.apply),
                    "Apply must remain enabled for repeated dropdown edits");
            require(!(GetWindowLongPtrW(e.numeric.track, GWL_STYLE) & WS_VISIBLE),
                    "choice controls do not expose a numeric slider");
            require(SendMessageW(e.choices, CB_GETCURSEL, 0, 0) == (LRESULT)value,
                    "editor must display refreshed value");
        }
        numeric.writable = false;
        refreshSettings(e, {numeric, choice});
        ListView_SetItemState(e.list, 0, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        showSelection(e);
        require(!IsWindowEnabled(e.numeric.track), "read-only numeric settings disable slider");
        ListView_SetItemState(e.list, 1, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        showSelection(e);
        choice.writable = false;
        refreshSettings(e, {numeric, choice});
        require(!IsWindowEnabled(e.apply), "unsupported control must disable Apply");
        DestroyWindow(e.window);
        std::cout << "PASS: native numeric sliders, negative/fractional/large ranges, text "
                     "synchronization, auto mode, selection and repeated Apply; "
                     "unsupported controls disabled\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

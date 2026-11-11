#pragma once
#include <algorithm>
#include <cmath>
#include <commctrl.h>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <windows.h>
namespace ptz {
// Trackbar positions are step indices, so negative/fractional ranges work too.
struct NumericSlider {
    HWND track = nullptr, edit = nullptr;
    double minimum = 0, step = 1;
    int ticks = 0;
    bool valid = false, updating = false;
    void configure(double low, double high, double increment, double value, bool enabled) {
        minimum = low;
        step = increment;
        double count = (high - low) / increment;
        valid = std::isfinite(low) && std::isfinite(high) && std::isfinite(increment) &&
                increment > 0 && high >= low && std::isfinite(count) && count >= 0 &&
                count < std::numeric_limits<int>::max();
        ticks = valid ? (int)std::floor(count + 1e-7) : 0;
        SendMessageW(track, TBM_SETTICFREQ, std::max(1, ticks / 10), 0);
        SendMessageW(track, TBM_SETRANGEMIN, FALSE, 0);
        SendMessageW(track, TBM_SETRANGEMAX, TRUE, ticks);
        SendMessageW(track, TBM_SETLINESIZE, 0, 1);
        SendMessageW(track, TBM_SETPAGESIZE, 0, std::max(1, ticks / 10));
        EnableWindow(track, valid && ticks > 0 && enabled);
        reflect(value);
    }
    void reflect(double value) {
        if (!valid || !std::isfinite(value))
            return;
        double index = std::clamp((value - minimum) / step, 0.0, (double)ticks);
        SendMessageW(track, TBM_SETPOS, TRUE, (LPARAM)std::lround(index));
    }
    void fromEdit() {
        if (updating || !valid)
            return;
        wchar_t buffer[128]{};
        GetWindowTextW(edit, buffer, 128);
        try {
            size_t end = 0;
            double value = std::stod(buffer, &end);
            if (end == std::wstring(buffer).size())
                reflect(value);
        } catch (...) { /* Preserve partial text such as '-' while typing. */
        }
    }
    void fromTrack() {
        if (!valid || !IsWindowEnabled(track))
            return;
        int index = (int)SendMessageW(track, TBM_GETPOS, 0, 0);
        double value = minimum + std::clamp(index, 0, ticks) * step;
        if (std::abs(value) < step * 1e-9)
            value = 0;
        std::wostringstream text;
        text << std::setprecision(12) << value;
        updating = true;
        SetWindowTextW(edit, text.str().c_str());
        updating = false;
    }
};
} // namespace ptz

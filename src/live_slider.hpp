#pragma once
#include <cstdint>
#include <map>
namespace ptz {
// Throttle while dragging, retaining only the newest value for each control.
// Readback runs after input is idle, never while a slider owns mouse capture.
template <class Key, class Value> class LiveSliderEdits {
    struct Pending {
        Value value;
        bool final;
        std::uint64_t interaction;
    };
    struct Sent {
        Value value;
        std::uint64_t at, interaction;
        bool verify;
    };
    std::map<Key, Pending> pending_;
    std::map<Key, Sent> sent_;

  public:
    void stage(const Key &key, Value value, bool final, std::uint64_t now) {
        auto previous = sent_.find(key);
        if (previous != sent_.end() && previous->second.verify && previous->second.value == value) {
            pending_.erase(key);
            previous->second.interaction = now;
            return;
        }
        pending_[key] = {value, final, now};
    }
    template <class Apply>
    void tick(std::uint64_t now, bool captured, Apply apply, bool force = false) {
        for (auto i = pending_.begin(); i != pending_.end();) {
            auto previous = sent_.find(i->first);
            if (force || i->second.final || previous == sent_.end() ||
                now - previous->second.at >= 80) {
                apply(i->first, i->second.value, false);
                sent_.insert_or_assign(i->first,
                                       Sent{i->second.value, now, i->second.interaction, true});
                i = pending_.erase(i);
            } else
                ++i;
        }
        if (!captured || force)
            for (auto &entry : sent_)
                if (entry.second.verify && !pending_.count(entry.first) &&
                    (force || now - entry.second.interaction >= 150)) {
                    apply(entry.first, entry.second.value, true);
                    entry.second.verify = false;
                }
    }
    bool active() const {
        if (!pending_.empty())
            return true;
        for (auto &entry : sent_)
            if (entry.second.verify)
                return true;
        return false;
    }
    void clear() {
        pending_.clear();
        sent_.clear();
    }
};
} // namespace ptz

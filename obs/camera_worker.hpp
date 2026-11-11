#pragma once
#include "core.hpp"
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace tinyobs {
// All DirectShow/KS interfaces are created, used and destroyed in this STA.
// Named jobs replace older pending jobs, keeping slider/scene bursts bounded.
class CameraWorker {
  public:
    using Work = std::function<void(ptz::Camera &)>;
    using Error = std::function<void(const std::string &)>;
    explicit CameraWorker(Error error);
    ~CameraWorker();
    void submit(std::string key, Work work);
    void cancel(const std::string &prefix);
    void stop();
    CameraWorker(const CameraWorker &) = delete;
    CameraWorker &operator=(const CameraWorker &) = delete;
  private:
    struct Job { std::string key; Work work; };
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Job> jobs_;
    bool stopping_ = false;
    std::thread thread_;
};
inline void connect(ptz::Camera &camera, const std::wstring &device) {
    if (device.empty()) throw std::runtime_error("Select a camera first.");
    if (!camera.connected() || camera.id() != device) camera.connect(device);
}
} // namespace tinyobs

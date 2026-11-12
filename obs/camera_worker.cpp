#include "camera_worker.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
namespace tinyobs {
CameraWorker::CameraWorker(Error error) {
    thread_ = std::thread([this, error = std::move(error)] {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        {
            ptz::Camera camera;
            for (;;) {
                Job job;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    condition_.wait_for(lock, std::chrono::milliseconds(15), [this] { return stopping_ || !jobs_.empty(); });
                    if (stopping_) break;
                    if (!jobs_.empty()) {
                        job = std::move(jobs_.front());
                        jobs_.pop_front();
                    }
                }
                try {
                    if (FAILED(hr)) throw std::runtime_error("Cannot initialize camera COM worker.");
                    if (job.work) job.work(camera);
                } catch (const std::exception &e) {
                    error(e.what());
                } catch (...) {
                    error("Unexpected camera error.");
                }
                MSG message;
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
        } // Release camera interfaces before uninitializing COM.
        if (SUCCEEDED(hr)) CoUninitialize();
        finished_.store(true);
    });
}
CameraWorker::~CameraWorker() { stop(); }
void CameraWorker::submit(std::string key, Work work) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        if (!key.empty())
            jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
                [&](const Job &job) { return job.key == key; }), jobs_.end());
        jobs_.push_back({std::move(key), std::move(work)});
    }
    condition_.notify_one();
}
void CameraWorker::cancel(const std::string &prefix) {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(), [&](const Job &job) {
        return job.key.compare(0, prefix.size(), prefix) == 0;
    }), jobs_.end());
}
void CameraWorker::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        jobs_.clear();
    }
    condition_.notify_one();
    if (thread_.joinable()) {
        // Video renderer teardown may send window messages to the UI owner.
        // Keep its native message queue responsive until COM teardown finishes.
        while (!finished_.load()) {
            MsgWaitForMultipleObjectsEx(0, nullptr, 15, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) { PostQuitMessage((int)message.wParam); break; }
                TranslateMessage(&message); DispatchMessageW(&message);
            }
        }
        thread_.join();
    }
}
} // namespace tinyobs

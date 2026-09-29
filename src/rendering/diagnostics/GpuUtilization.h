#pragma once

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace rendering {
// Match conservatively: duplicate model names cannot identify the active adapter.
inline std::optional<std::size_t> utilizationDevice(const std::string& renderer,
                                                   const std::vector<std::string>& names) {
    const auto normalize = [](std::string name) {
        if (name.starts_with("NVIDIA ")) name.erase(0, 7);
        if (const auto suffix = name.find('/'); suffix != std::string::npos) name.erase(suffix);
        return name;
    };
    std::optional<std::size_t> match;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (!names[i].empty() && normalize(names[i]) == normalize(renderer)) {
            if (match) return std::nullopt;
            match = i;
        }
    }
    return match;
}

// NVML's stable C ABI subset. Load optionally: no CUDA SDK or hard driver dependency.
// https://docs.nvidia.com/deploy/nvml-api/api/group__nvmlDeviceQueries.html
class NvidiaUtilization {
public:
    explicit NvidiaUtilization(const std::string& vendor, const std::string& renderer) {
#if defined(__linux__)
        if (vendor != "NVIDIA Corporation") return;
        library_ = dlopen("libnvidia-ml.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (!library_) return;
        const auto init = symbol<int (*)()>("nvmlInit_v2");
        shutdown_ = symbol<int (*)()>("nvmlShutdown");
        const auto count = symbol<int (*)(unsigned*)>("nvmlDeviceGetCount_v2");
        const auto handle = symbol<int (*)(unsigned, Device*)>("nvmlDeviceGetHandleByIndex_v2");
        const auto name = symbol<int (*)(Device, char*, unsigned)>("nvmlDeviceGetName");
        utilization_ = symbol<int (*)(Device, Rates*)>("nvmlDeviceGetUtilizationRates");
        if (!init || !shutdown_ || !count || !handle || !name || !utilization_ || init() != 0) return;
        initialized_ = true;
        unsigned size = 0;
        if (count(&size) != 0 || size > 128) return;
        std::vector<Device> devices(size);
        std::vector<std::string> names(size);
        for (unsigned i = 0; i < size; ++i) {
            char label[256]{};
            // An inaccessible adapter makes identification ambiguous; do not guess.
            if (handle(i, &devices[i]) != 0 || name(devices[i], label, sizeof(label)) != 0) return;
            names[i] = label;
        }
        if (const auto index = utilizationDevice(renderer, names)) device_ = devices[*index];
#endif
    }
    ~NvidiaUtilization() {
#if defined(__linux__)
        if (initialized_) shutdown_();
        if (library_) dlclose(library_);
#endif
    }
    NvidiaUtilization(const NvidiaUtilization&) = delete;
    NvidiaUtilization& operator=(const NvidiaUtilization&) = delete;
    std::optional<unsigned> sample() const {
        Rates rates{};
        if (!device_ || !utilization_ || utilization_(device_, &rates) != 0 || rates.gpu > 100)
            return std::nullopt;
        return rates.gpu;
    }
private:
    using Device = struct nvmlDevice_st*;
    struct Rates { unsigned gpu, memory; };
    void* library_ = nullptr;
    bool initialized_ = false;
    Device device_ = nullptr;
    int (*shutdown_)() = nullptr;
    int (*utilization_)(Device, Rates*) = nullptr;
#if defined(__linux__)
    template<class T> T symbol(const char* name) { return reinterpret_cast<T>(dlsym(library_, name)); }
#endif
};

// Driver calls run off the rendering thread. Hidden panels schedule no new work.
class GpuUtilization {
public:
    GpuUtilization(std::string vendor, std::string renderer)
        : vendor_(std::move(vendor)), renderer_(std::move(renderer)) {}
    std::optional<unsigned> sample(bool visible) {
        using namespace std::chrono_literals;
        const auto now = std::chrono::steady_clock::now();
        if (pending_.valid() && pending_.wait_for(0ms) == std::future_status::ready) {
            value_ = pending_.get();
            lastSample_ = now;
        }
        if (!visible) { value_.reset(); return std::nullopt; }
        if (now - lastSample_ > 2s) value_.reset();
        if (!pending_.valid() && now >= nextPoll_) {
            nextPoll_ = now + 500ms;
            pending_ = std::async(std::launch::async, [this] {
                if (!reader_) reader_ = std::make_unique<NvidiaUtilization>(vendor_, renderer_);
                return reader_->sample();
            });
        }
        return value_;
    }
private:
    std::string vendor_, renderer_;
    std::unique_ptr<NvidiaUtilization> reader_;
    std::optional<unsigned> value_;
    std::chrono::steady_clock::time_point nextPoll_{}, lastSample_{};
    // Destroy first: wait for a pending driver call before destroying its reader.
    std::future<std::optional<unsigned>> pending_;
};
} // namespace rendering

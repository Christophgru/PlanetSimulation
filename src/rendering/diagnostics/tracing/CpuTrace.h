#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

namespace rendering {
// Optional Chrome/Perfetto complete-event stream. Bind each participating thread
// explicitly; the trace must outlive all bindings (including terrain futures).
class CpuTrace {
    using Clock = std::chrono::steady_clock;
public:
    explicit CpuTrace(const std::string& path = "");
    ~CpuTrace();
    CpuTrace(const CpuTrace&) = delete;
    CpuTrace& operator=(const CpuTrace&) = delete;
    class Scope;

    class Thread {
    public:
        Thread(CpuTrace* trace, const char* name);
        ~Thread();
        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;
    private:
        friend class Scope;
        CpuTrace* trace_;
        Thread* previous_;
        unsigned id_ = 0;
    };

    class Scope {
    public:
        explicit Scope(const char* name);
        ~Scope() { stop(); }
        void stop() noexcept;
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        Thread* thread_;
        const char* name_; // Scope labels must outlive the scope; use literals.
        Clock::time_point start_;
        std::int64_t cpuStart_ = -1;
    };

private:
    static thread_local Thread* current_;
    std::ofstream output_;
    std::mutex mutex_;
    Clock::time_point origin_ = Clock::now();
    unsigned nextThread_ = 0;
    bool first_ = true;
    void separator();
};
}

#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <time.h>

namespace rendering {
namespace {
std::int64_t threadCpuNs() {
#if defined(CLOCK_THREAD_CPUTIME_ID)
    timespec value{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) == 0)
        return std::int64_t(value.tv_sec) * 1000000000 + value.tv_nsec;
#endif
    return -1;
}
}
thread_local CpuTrace::Thread* CpuTrace::current_ = nullptr;

CpuTrace::CpuTrace(const std::string& path) {
    if (path.empty()) return;
    output_.open(path);
    if (!output_) throw std::runtime_error("Cannot open CPU trace: " + path);
    output_ << "{\"displayTimeUnit\":\"ms\",\"traceEvents\":[";
}

CpuTrace::~CpuTrace() {
    if (!output_.is_open()) return;
    output_ << "]}\n";
    output_.flush();
    if (!output_) std::cerr << "Failed to finish CPU trace\n";
}

void CpuTrace::separator() {
    if (!first_) output_ << ',';
    first_ = false;
}

CpuTrace::Thread::Thread(CpuTrace* trace, const char* name)
    : trace_(trace && trace->output_.is_open() ? trace : nullptr), previous_(current_) {
    if (trace_) {
        std::lock_guard lock(trace_->mutex_);
        id_ = trace_->nextThread_++;
        trace_->separator();
        trace_->output_ << nlohmann::json{{"ph", "M"}, {"pid", 1}, {"tid", id_},
            {"name", "thread_name"}, {"args", {{"name", name}}}};
    }
    current_ = this;
}
CpuTrace::Thread::~Thread() { current_ = previous_; }

CpuTrace::Scope::Scope(const char* name)
    : thread_(current_ && current_->trace_ ? current_ : nullptr), name_(name) {
    // Disabled tracing avoids clock reads, allocations, locks and file writes.
    if (!thread_) return;
    start_ = Clock::now();
    cpuStart_ = threadCpuNs();
}

void CpuTrace::Scope::stop() noexcept {
    if (!thread_) return;
    const auto end = Clock::now();
    const auto cpuEnd = threadCpuNs();
    auto* trace = thread_->trace_;
    // Timestamp before taking the output lock. Ancestor self time includes
    // tracing overhead; timings are deliberately not corrected estimates.
    try {
        nlohmann::json event{{"ph", "X"}, {"cat", "cpu"}, {"pid", 1},
            {"tid", thread_->id_}, {"name", name_},
            {"ts", std::chrono::duration<double, std::micro>(start_ - trace->origin_).count()},
            {"dur", std::chrono::duration<double, std::micro>(end - start_).count()}};
        if (cpuStart_ >= 0 && cpuEnd >= cpuStart_)
            event["args"]["thread_cpu_us"] = (cpuEnd - cpuStart_) / 1000.0;
        std::lock_guard lock(trace->mutex_);
        trace->separator();
        trace->output_ << event;
    } catch (...) {
        // Diagnostics must not throw from a scope destructor during unwinding.
        std::cerr << "Failed to write CPU trace event\n";
    }
    thread_ = nullptr;
}
}

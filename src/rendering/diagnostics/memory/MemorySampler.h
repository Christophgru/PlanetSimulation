#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace rendering {
struct NvxMemory {
    std::string status="unsupported";
    std::uint64_t observedNs=0,dedicated=0,totalAvailable=0,available=0;
};
struct MemoryContext {
    std::string vendor,renderer,uuid,status="uuid_unsupported";
    NvxMemory nvx;
};
struct PhysicalMemory {
    std::string status="pending";
    int driverError=0;
    std::uint64_t total=0,used=0,free=0;
};
enum class MemoryPhase { RendererReady,Steady,Loading,Minimized,Capture,
    ReloadRequested,ReloadPreparing,ReloadExchange,ReloadFailed,ReloadSuperseded,
    Retirement,Shutdown };
struct MemoryObservation {
    std::uint64_t observedNs=0,frame=0,epoch=0,serial=0,reloadAttempt=0,replacementEpoch=0;
    MemoryPhase phase=MemoryPhase::RendererReady;
    bool managedLedger=false;
    // Replacement reservation already includes the live external reservation.
    std::uint64_t liveReserved=0,replacementReserved=0,overlapReserved=0;
    std::uint64_t meshVectorBytes=0,readyVectorBytes=0;
    bool schedulerObserved=false;
    unsigned running=0,queued=0,ready=0;
    NvxMemory nvx;
};
std::uint64_t memoryClockNs();
const char* memoryPhaseName(MemoryPhase phase);
bool validDeviceUuid(const std::string& uuid);
using MemoryReader=std::function<PhysicalMemory()>;
MemoryReader nvmlMemoryReader(const MemoryContext& context); // Construct/destroy on sampling worker.
// One persistent worker, scalar latest observation, fixed 64-event ring. Render
// submission uses try_lock for latest state and an SPSC event ring. One context
// producer; stop only after submissions cease. No driver/filesystem/wait work there.
class MemorySampler {
public:
    using Factory=std::function<MemoryReader(const MemoryContext&)>;
    MemorySampler(const std::string& path,MemoryContext context,MemoryObservation initial,
        Factory factory=nvmlMemoryReader,
        std::chrono::milliseconds period=std::chrono::milliseconds(250));
    ~MemorySampler();
    MemorySampler(const MemorySampler&)=delete;
    MemorySampler& operator=(const MemorySampler&)=delete;
    bool observe(MemoryObservation observation,bool event=false);
    struct Cached {PhysicalMemory physical;std::uint64_t sampledNs=0,reserved=0;};
    std::optional<Cached> cached(); // Nonblocking scalar copy; no driver call.
    // Setup/shutdown only; joins the single reader before closing its output.
    void stop(MemoryObservation final);
    std::uint64_t dropped() const {return dropped_.load();}
private:
    friend struct MemorySamplerProbe;
    void run();
    std::string path_;
    MemoryContext context_;
    Factory factory_;
    std::chrono::milliseconds period_;
    std::mutex mutex_;
    std::condition_variable changed_;
    MemoryObservation initial_,latest_,final_;
    Cached cached_;
    std::array<MemoryObservation,64> events_;
    std::atomic<std::uint64_t> eventRead_{0},eventWrite_{0};
    std::atomic<bool> stopping_{false};
    std::atomic<std::uint64_t> dropped_{0},droppedEvents_{0},skippedLatest_{0};
    std::thread worker_;
};
}

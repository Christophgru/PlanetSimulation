#include "rendering/diagnostics/memory/MemorySampler.h"
#include "Csv.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <future>
namespace rendering {
struct MemorySamplerProbe {static auto& mutex(MemorySampler& s){return s.mutex_;}};
}
using namespace rendering;
using namespace std::chrono_literals;
namespace {
MemoryContext context() {MemoryContext c;c.status="uuid_verified";c.vendor="NVIDIA Corporation";c.renderer="GPU, quoted \"model\"";c.uuid="GPU-11111111-1111-1111-1111-111111111111";return c;}
MemoryObservation observation(MemoryPhase phase=MemoryPhase::Steady) {MemoryObservation o;o.observedNs=memoryClockNs();o.phase=phase;return o;}
std::string path(const std::string& name) {const auto p=std::filesystem::path(PLANET_TEST_OUTPUT)/name;std::filesystem::create_directories(p.parent_path());return p.string();}
struct Env {
    std::string key;explicit Env(const char* k,const char* v):key(k) {setenv(k,v,1);}
    ~Env(){unsetenv(key.c_str());}
};
struct Gate {
    std::mutex mutex;std::condition_variable changed;bool open=false;
    void wait(){std::unique_lock lock(mutex);changed.wait(lock,[&]{return open;});}
    void release(){{std::lock_guard lock(mutex);open=true;}changed.notify_all();}
};
struct Release {Gate& gate;~Release(){gate.release();}};
}
TEST(MemoryIdentity, RejectsAmbiguousUnverifiedAndMalformedIdentifiers) {
    for(const auto* status:{"uuid_unsupported","uuid_ambiguous","uuid_invalid"}) {
        auto c=context();c.status=status;EXPECT_EQ(nvmlMemoryReader(c)().status,"unverified_context");
    }
    for(const auto* uuid:{"Quadro M1000M","GPU-00000000-0000-0000-0000-000000000000","GPU-11111111-1111-1111-1111-11111111111g"}) {
        auto c=context();c.uuid=uuid;EXPECT_FALSE(validDeviceUuid(uuid));EXPECT_EQ(nvmlMemoryReader(c)().status,"unverified_context");
    }
    auto c=context();c.vendor="Mesa";EXPECT_EQ(nvmlMemoryReader(c)().status,"vendor_unsupported");
}
TEST(MemoryIdentity, RequiresReadbackUuidAndPreservesFailures) {
    for(const auto& [key,status]:std::vector<std::pair<const char*,const char*>>{
        {"MEMORY_INIT_ERROR","initialization_failed"},{"MEMORY_LOOKUP_ERROR","uuid_lookup_failed"},
        {"MEMORY_UUID_ERROR","uuid_read_failed"},{"MEMORY_READ_ERROR","read_failed"}}) {
        Env env(key,"3");const auto sample=nvmlMemoryReader(context())();
        EXPECT_EQ(sample.status,status);EXPECT_EQ(sample.driverError,3);EXPECT_EQ(sample.total,0u);
    }
    Env wrong("MEMORY_WRONG_UUID","1");EXPECT_EQ(nvmlMemoryReader(context())().status,"uuid_mismatch");
}
TEST(MemoryIdentity, AcceptsZeroFreeAndRejectsInvalidTotals) {
    EXPECT_EQ(nvmlMemoryReader(context())().total,1024u);
    {Env free("MEMORY_FREE","0"),used("MEMORY_USED","1024");const auto sample=nvmlMemoryReader(context())();
     EXPECT_EQ(sample.status,"ok");EXPECT_EQ(sample.free,0u);EXPECT_EQ(sample.used,1024u);}
    {Env free("MEMORY_FREE","257");EXPECT_EQ(nvmlMemoryReader(context())().status,"invalid_counters");}
}
TEST(MemorySampler, BoundsSlowReaderQueueWithoutWaitingAndJoinsAtShutdown) {
    Gate gate;std::promise<void> started;std::thread::id factoryThread,readThread,destroyThread;
    struct Owner {std::thread::id& destroyed;~Owner(){destroyed=std::this_thread::get_id();}};
    unsigned calls=0;
    MemorySampler sampler(path("slow.csv"),context(),observation(MemoryPhase::RendererReady),[&](const auto&) {
        factoryThread=std::this_thread::get_id();auto owner=std::shared_ptr<Owner>(new Owner{destroyThread});
        return MemoryReader([&,owner]{readThread=std::this_thread::get_id();if(!calls++){started.set_value();gate.wait();}return PhysicalMemory{"ok",0,1024,768,256};});
    },10s);
    Release release{gate};ASSERT_EQ(started.get_future().wait_for(5s),std::future_status::ready);
    auto submissions=std::async(std::launch::async,[&]{unsigned accepted=0;for(unsigned i=0;i<128;++i) {
        auto o=observation(MemoryPhase::ReloadPreparing);o.serial=i+1;o.managedLedger=true;o.liveReserved=100;o.replacementReserved=o.overlapReserved=300;
        accepted+=sampler.observe(o,true);
    }return accepted;});
    const auto submitted=submissions.wait_for(500ms);if(submitted!=std::future_status::ready) gate.release();
    ASSERT_EQ(submitted,std::future_status::ready);EXPECT_EQ(submissions.get(),64u);EXPECT_EQ(sampler.dropped(),64u);
    auto stopping=std::async(std::launch::async,[&]{sampler.stop(observation(MemoryPhase::Shutdown));});
    EXPECT_EQ(stopping.wait_for(20ms),std::future_status::timeout);gate.release();stopping.get();
    EXPECT_NE(factoryThread,std::this_thread::get_id());EXPECT_EQ(readThread,factoryThread);EXPECT_EQ(destroyThread,factoryThread);
    const auto rows=memory_test::rows(path("slow.csv"));ASSERT_EQ(rows.size(),66u);
    EXPECT_EQ(rows.front().at("phase"),"renderer_ready");EXPECT_EQ(rows.back().at("phase"),"shutdown");
    for(std::size_t i=1;i+1<rows.size();++i) {
        EXPECT_EQ(rows[i].at("kind"),"event");EXPECT_EQ(rows[i].at("serial"),std::to_string(i));
        EXPECT_EQ(rows[i].at("sample_after_observation"),"1");EXPECT_EQ(rows[i].at("overlap_reserved_bytes"),"300");
        EXPECT_EQ(rows[i].at("renderer"),context().renderer);
    }
}
TEST(MemorySampler, ReportsStaleCachedSamplesAndObservationAge) {
    std::promise<void> read;unsigned calls=0;
    MemorySampler sampler(path("stale.csv"),context(),observation(),[&](const auto&) {
        return MemoryReader([&]{if(!calls++) read.set_value();return PhysicalMemory{"ok",0,1024,768,256};});
    },10s);
    ASSERT_EQ(read.get_future().wait_for(5s),std::future_status::ready);
    std::this_thread::sleep_for(2100ms);auto o=observation(MemoryPhase::ReloadExchange);o.observedNs-=3000000000ull;
    EXPECT_TRUE(sampler.observe(o,true));sampler.stop(observation(MemoryPhase::Shutdown));
    const auto rows=memory_test::rows(path("stale.csv"));ASSERT_EQ(rows.size(),3u);
    EXPECT_EQ(rows[1].at("stale"),"1");EXPECT_GT(std::stod(rows[1].at("sample_age_ms")),2000);
    EXPECT_GT(std::stod(rows[1].at("observation_age_ms")),3000);
}
TEST(MemorySampler, PreservesUnknownCountersAndReaderExceptions) {
    for(int mode:{0,1,2}) {
        const bool fail=mode!=0;const auto output=path(mode==1?"factory-exception.csv":mode==2?"read-exception.csv":"unsupported.csv");
        MemorySampler sampler(output,context(),observation(),[&](const auto&)->MemoryReader {
            if(mode==1) throw std::runtime_error("fixture");
            return [mode]{if(mode==2) throw std::runtime_error("read fixture");return PhysicalMemory{"unverified_context"};};
        });sampler.stop(observation(MemoryPhase::Shutdown));
        const auto rows=memory_test::rows(output);ASSERT_EQ(rows.size(),2u);
        for(const auto& r:rows) {EXPECT_EQ(r.at("nvml_status"),fail?"reader_exception":"unverified_context");
            EXPECT_TRUE(r.at("used_bytes").empty());EXPECT_TRUE(r.at("overlap_reserved_bytes").empty());
            EXPECT_TRUE(r.at("ready_vector_bytes").empty());EXPECT_EQ(r.at("physical_scope"),"device_wide_other_processes_included");}
    }
}

TEST(MemorySampler, UnwritableOutputFailsBeforeStartingWorker) {
    bool started=false;
    EXPECT_THROW(MemorySampler(path("absent-parent")+"/trace.csv",context(),observation(),[&](const auto&) {
        started=true;return MemoryReader([]{return PhysicalMemory{};});
    }),std::runtime_error);
    EXPECT_FALSE(started);
}

TEST(MemorySampler, RetainsLifecycleEventsWhenLatestSnapshotLockIsBusy) {
    Gate driverGate,snapshotGate;std::promise<void> reading,locked;unsigned calls=0;
    MemorySampler sampler(path("busy-latest.csv"),context(),observation(),[&](const auto&) {
        return MemoryReader([&]{if(!calls++){reading.set_value();driverGate.wait();}return PhysicalMemory{"ok",0,1024,768,256};});
    },10s);
    Release driverRelease{driverGate};ASSERT_EQ(reading.get_future().wait_for(5s),std::future_status::ready);
    auto locker=std::async(std::launch::async,[&]{std::lock_guard lock(MemorySamplerProbe::mutex(sampler));locked.set_value();snapshotGate.wait();});
    Release snapshotRelease{snapshotGate};ASSERT_EQ(locked.get_future().wait_for(5s),std::future_status::ready);
    EXPECT_FALSE(sampler.observe(observation(),false));
    auto failure=observation(MemoryPhase::ReloadFailed);failure.reloadAttempt=17;failure.replacementEpoch=9;
    EXPECT_TRUE(sampler.observe(failure,true));EXPECT_EQ(sampler.dropped(),1u);
    snapshotGate.release();locker.get();driverGate.release();sampler.stop(observation(MemoryPhase::Shutdown));
    const auto rows=memory_test::rows(path("busy-latest.csv"));ASSERT_EQ(rows.size(),3u);
    EXPECT_EQ(rows[1].at("phase"),"reload_failed");EXPECT_EQ(rows[1].at("reload_attempt"),"17");
    EXPECT_EQ(rows[1].at("replacement_epoch"),"9");EXPECT_EQ(rows.back().at("dropped_events"),"0");
    EXPECT_EQ(rows.back().at("dropped_observations"),"1");EXPECT_EQ(rows.back().at("skipped_latest_observations"),"2");
}

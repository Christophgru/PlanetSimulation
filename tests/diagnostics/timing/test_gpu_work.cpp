#include "QueryTestSupport.h"
#include <thread>
using namespace timing_test;
using rendering::GpuWorkProfiler;
using rendering::GpuWorkStage;

TEST(GpuWorkProfiler, DelayedQueriesAreBoundedAndEveryReceiptRetainsItsIdentity) {
    FakeQueries api;const auto file=path("bounded.csv");
    {
        GpuWorkProfiler profiler(file);GpuWorkProfiler::Binding bind(&profiler);profiler.frame(12);
        for(unsigned i=0;i<GpuWorkProfiler::capacity+1;++i) {
            GpuWorkProfiler::Scope event(GpuWorkStage::GrassAllocation,identity);
        }
        EXPECT_EQ(profiler.stats().pending,GpuWorkProfiler::capacity);
        EXPECT_EQ(profiler.stats().dropped,1u);EXPECT_EQ(generated,2*GpuWorkProfiler::capacity);
        profiler.collect();EXPECT_EQ(reads,0u);
        ready();queries.begin()->second.ready=false;
        profiler.collect();EXPECT_EQ(profiler.stats().pending,1u);
        ready();profiler.collect();EXPECT_EQ(profiler.stats().ready,GpuWorkProfiler::capacity);
        EXPECT_EQ(profiler.stats().pending,0u);
        {GpuWorkProfiler::Scope reused(GpuWorkStage::TerrainField,identity);}
        EXPECT_EQ(generated,2*GpuWorkProfiler::capacity);ready();profiler.collect();
    }
    const auto data=rows(file);ASSERT_EQ(data.size(),GpuWorkProfiler::capacity+2);
    unsigned dropped=0;
    for(const auto& r:data) {
        EXPECT_EQ(r.at("epoch"),"4");EXPECT_EQ(r.at("request_serial"),"7");EXPECT_EQ(r.at("body"),"2");
        EXPECT_EQ(r.at("field"),"123");EXPECT_EQ(r.at("topology"),"456");EXPECT_EQ(r.at("identity_valid"),"1");
        EXPECT_EQ(r.at("frame"),"12");
        if(r.at("status")=="dropped") {++dropped;EXPECT_EQ(r.at("gpu_ms"),"");}
        else {EXPECT_EQ(r.at("status"),"ready");EXPECT_EQ(r.at("gpu_ms"),"1");}
    }
    EXPECT_EQ(dropped,1u);
}
TEST(GpuWorkProfiler, ShutdownDoesNotReadBusyQueriesAndUntracedScopesAllocateNothing) {
    FakeQueries api;
    {GpuWorkProfiler::Scope unbound(GpuWorkStage::TerrainField,identity);}
    {GpuWorkProfiler disabled;GpuWorkProfiler::Binding bind(&disabled);
     GpuWorkProfiler::Scope event(GpuWorkStage::TerrainField,identity);}
    EXPECT_EQ(generated,0u);
    const auto file=path("missing.csv");
    {GpuWorkProfiler p(file);GpuWorkProfiler::Binding bind(&p);
     GpuWorkProfiler::Scope event(GpuWorkStage::GrassMetadata,identity);}
    EXPECT_EQ(reads,0u);const auto data=rows(file);ASSERT_EQ(data.size(),1u);
    EXPECT_EQ(data[0].at("status"),"missing_shutdown");EXPECT_EQ(data[0].at("gpu_ms"),"");
}
TEST(GpuWorkProfiler, NestedContextBindingsRestoreRequestKeysAndExceptionsRetainReceipts) {
    FakeQueries api;const auto first=path("outer.csv"),second=path("inner.csv");
    {
        GpuWorkProfiler a(first),b(second);GpuWorkProfiler::Binding outer(&a);
        GpuWorkProfiler::Request request(3,9,1);
        {GpuWorkProfiler::Binding inner(&b);EXPECT_EQ(GpuWorkProfiler::identity().epoch,0u);
         GpuWorkProfiler::Request innerRequest(8,11,0);
         GpuWorkProfiler::Scope event(GpuWorkStage::TerrainExpansion,GpuWorkProfiler::identity());}
        EXPECT_EQ(GpuWorkProfiler::identity().epoch,3u);EXPECT_EQ(GpuWorkProfiler::identity().serial,9u);
        try {GpuWorkProfiler::Scope event(GpuWorkStage::GrassMetadata,GpuWorkProfiler::identity());
             throw std::runtime_error("cancel preparation");} catch(const std::runtime_error&) {}
        ready();a.collect();b.collect();
    }
    ASSERT_EQ(rows(first).size(),1u);ASSERT_EQ(rows(second).size(),1u);
    EXPECT_EQ(rows(first)[0].at("epoch"),"3");EXPECT_EQ(rows(first)[0].at("status"),"failed_ready");
    EXPECT_EQ(rows(second)[0].at("epoch"),"8");EXPECT_EQ(GpuWorkProfiler::identity().epoch,0u);
}
TEST(GpuWorkProfiler, FailedQueryAllocationIsReportedWithoutChangingSubmission) {
    FakeQueries api;const auto file=path("allocation.csv");
    {GpuWorkProfiler p(file);GpuWorkProfiler::Binding bind(&p);failAllocation=true;
     GpuWorkProfiler::Scope event(GpuWorkStage::GrassMetadata,identity);EXPECT_EQ(p.stats().dropped,1u);}
    ASSERT_EQ(rows(file).size(),1u);EXPECT_EQ(rows(file)[0].at("status"),"allocation_failed");EXPECT_EQ(reads,0u);
}
TEST(GpuWorkProfiler, CpuWorkersDoNotInheritContextOrRequestBindings) {
    FakeQueries api;const auto file=path("thread.csv");
    {GpuWorkProfiler p(file);GpuWorkProfiler::Binding bind(&p);GpuWorkProfiler::Request request(2,3,0);
     std::thread worker([] {
         EXPECT_EQ(GpuWorkProfiler::identity().epoch,0u);
         GpuWorkProfiler::Scope event(GpuWorkStage::TerrainField,identity);
     });worker.join();EXPECT_EQ(generated,0u);EXPECT_EQ(GpuWorkProfiler::identity().epoch,2u);}
    EXPECT_TRUE(rows(file).empty());
}

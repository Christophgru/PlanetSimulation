#include "QueryTestSupport.h"
using namespace timing_test;
using rendering::FrameProfiler;
using rendering::FrameStage;
using rendering::GpuWorkProfiler;
using rendering::GpuWorkStage;

TEST(FrameProfilerTiming, SpanAndNestedWorkAreSeparateAndResultsRequireAvailability) {
    FakeQueries api;const auto file=path("frame.csv");
    {
        FrameProfiler p(file);p.beginFrame(20);
        {FrameProfiler::Scope opaque(&p,FrameStage::Opaque);
         GpuWorkProfiler::Scope placement(GpuWorkStage::GrassPlacement,identity);}
        p.endFrame();p.collect();EXPECT_EQ(reads,0u);
        ready();queries.at(1).ready=false;p.collect();EXPECT_FALSE(p.gpuReady());
        ready();p.collect();EXPECT_TRUE(p.gpuReady());
    }
    const auto data=rows(file);ASSERT_EQ(data.size(),1u);
    EXPECT_EQ(data[0].at("gpu_valid"),"1");EXPECT_EQ(data[0].at("gpu_frame_span_ms"),"5");
    EXPECT_EQ(data[0].at("gpu_ms"),"3");EXPECT_EQ(data[0].at("gpu_opaque_ms"),"3");
    EXPECT_EQ(data[0].at("gpu_status"),"ready");
    const auto work=rows(file+".gpu-work.csv");ASSERT_EQ(work.size(),1u);EXPECT_EQ(work[0].at("gpu_ms"),"1");
}
TEST(FrameProfilerTiming, BusyRingAndShutdownMissingSamplesAreExplicitAndEventsAreBounded) {
    FakeQueries api;const auto file=path("frame-busy.csv");
    {
        FrameProfiler p(file);
        for(int i=0;i<9;++i) {
            p.beginFrame(i);
            for(int j=0;j<65;++j) {FrameProfiler::Scope scope(&p,FrameStage::Opaque);}
            p.endFrame();
        }
    }
    EXPECT_EQ(reads,0u);EXPECT_EQ(generated,8u*128);
    const auto data=rows(file);ASSERT_EQ(data.size(),9u);unsigned busy=0;
    for(const auto& r:data) {
        EXPECT_EQ(r.at("gpu_valid"),"0");EXPECT_EQ(r.at("gpu_frame_span_ms"),"");EXPECT_EQ(r.at("gpu_ms"),"");
        EXPECT_EQ(r.at("gpu_events_dropped"),"2");
        if(r.at("gpu_status")=="ring_full") ++busy;else EXPECT_EQ(r.at("gpu_status"),"missing_shutdown");
    }
    EXPECT_EQ(busy,1u);
}

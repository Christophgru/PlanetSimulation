#include "QueryTestSupport.h"
#include "rendering/diagnostics/timing/NativeLoopProfiler.h"
using namespace timing_test;
using rendering::NativeLoopProfiler;
namespace {
std::chrono::milliseconds elapsed{};
unsigned clockCalls=0;
NativeLoopProfiler::Clock::time_point now() {++clockCalls;return NativeLoopProfiler::Clock::time_point(elapsed);}
void advance(int milliseconds) {elapsed+=std::chrono::milliseconds(milliseconds);}
void loading(NativeLoopProfiler& p) {
    NativeLoopProfiler::Scope frame(p,1);
    {NativeLoopProfiler::Scope::Interval poll(frame,NativeLoopProfiler::Part::EventPoll);advance(3);}
    advance(7);
    {NativeLoopProfiler::Scope::Interval present(frame,NativeLoopProfiler::Part::Presentation);advance(11);}
    advance(2);frame.outcome(NativeLoopProfiler::Outcome::Loading);
    return; // The production loading continue must still close the receipt.
}
}
TEST(NativeLoopTiming, CompleteWallIncludesPollingPreparationPresentationAndMinimizedWait) {
    elapsed={};clockCalls=0;const auto file=path("native-loop.csv");
    {
        NativeLoopProfiler p(file,now);
        {NativeLoopProfiler::Scope frame(p,0);
         {NativeLoopProfiler::Scope::Interval poll(frame,NativeLoopProfiler::Part::EventPoll);advance(2);}
         advance(5);
         {NativeLoopProfiler::Scope::Interval present(frame,NativeLoopProfiler::Part::Presentation);advance(13);}
         advance(1);frame.outcome(NativeLoopProfiler::Outcome::Rendered);}
        loading(p);
        {NativeLoopProfiler::Scope frame(p,2);advance(4);
         {NativeLoopProfiler::Scope::Interval wait(frame,NativeLoopProfiler::Part::EventWait);advance(50);}
         advance(1);frame.outcome(NativeLoopProfiler::Outcome::Minimized);}
    }
    const auto data=rows(file);ASSERT_EQ(data.size(),3u);
    EXPECT_EQ(data[0].at("frame"),"0");EXPECT_EQ(data[0].at("wall_ms"),"21");
    EXPECT_EQ(data[0].at("event_poll_ms"),"2");EXPECT_EQ(data[0].at("presentation_ms"),"13");
    EXPECT_EQ(data[0].at("event_wait_ms"),"0");EXPECT_EQ(data[0].at("outcome"),"rendered");
    EXPECT_EQ(data[1].at("wall_ms"),"23");EXPECT_EQ(data[1].at("event_poll_ms"),"3");
    EXPECT_EQ(data[1].at("presentation_ms"),"11");EXPECT_EQ(data[1].at("outcome"),"loading");
    EXPECT_EQ(data[2].at("wall_ms"),"55");EXPECT_EQ(data[2].at("event_wait_ms"),"50");
    EXPECT_EQ(data[2].at("outcome"),"minimized");
}
TEST(NativeLoopTiming, ExceptionClosesReceiptAndDisabledTracingDoesNotReadClock) {
    elapsed={};clockCalls=0;const auto file=path("native-exception.csv");
    {
        NativeLoopProfiler p(file,now);
        EXPECT_THROW({NativeLoopProfiler::Scope frame(p,7);
            NativeLoopProfiler::Scope::Interval poll(frame,NativeLoopProfiler::Part::EventPoll);
            advance(9);throw std::runtime_error("event callback failure");},std::runtime_error);
    }
    const auto data=rows(file);ASSERT_EQ(data.size(),1u);
    EXPECT_EQ(data[0].at("frame"),"7");EXPECT_EQ(data[0].at("outcome"),"exception");
    EXPECT_EQ(data[0].at("wall_ms"),"9");EXPECT_EQ(data[0].at("event_poll_ms"),"9");
    clockCalls=0;
    {NativeLoopProfiler p("",now);NativeLoopProfiler::Scope frame(p,0);
     NativeLoopProfiler::Scope::Interval poll(frame,NativeLoopProfiler::Part::EventPoll);advance(100);}
    EXPECT_EQ(clockCalls,0u);
}

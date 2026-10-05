#include "rendering/diagnostics/timing/PublicationProfiler.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <map>
#include <sstream>
#include <vector>

using rendering::PublicationProfiler;
namespace {
using P=PublicationProfiler;
using Row=std::map<std::string,std::string>;
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> fields;std::istringstream input(s);std::string field;
    while(std::getline(input,field,',')) fields.push_back(field);
    if(!s.empty() && s.back()==',') fields.emplace_back();return fields;
}
std::vector<Row> rows(const std::string& path) {
    std::ifstream input(path);std::string line;std::getline(input,line);const auto header=split(line);
    std::vector<Row> result;
    while(std::getline(input,line)) {
        auto values=split(line);EXPECT_EQ(values.size(),header.size());if(values.size()!=header.size()) continue;
        Row row;for(std::size_t i=0;i<header.size();++i) row[header[i]]=values[i];result.push_back(row);
    }
    return result;
}
P::Clock::time_point timePoint{};
unsigned clocks=0;
auto now() {++clocks;return timePoint;}
void at(int ms) {timePoint=P::Clock::time_point{}+std::chrono::milliseconds(ms);}
std::string path(const char* name) {
    std::filesystem::create_directories(PLANET_TIMING_OUTPUT);return std::string(PLANET_TIMING_OUTPUT)+"/"+name+".publications.csv";
}
P::Key key(std::uint64_t serial=7) {P::Key k;k.epoch=2;k.serial=serial;k.body=1;k.field=42;k.backend=2;k.resident=true;return k;}
P::Generation generation() {return {42,99,43,100,8,9,{.1,.2,.3}};}
}
TEST(PublicationProfiler, WallAndFramesEndOnlyOnMatchingCompleteConsumerDraw) {
    const auto out=path("complete");at(10);P p(out,now);p.frame(5);const auto id=p.begin(key());
    at(12);p.phase(id,P::Phase::CpuStart);at(15);p.phase(id,P::Phase::CpuEnd);
    at(16);p.phase(id,P::Phase::GpuSubmit);p.frame(6);at(18);p.phase(id,P::Phase::GpuReady);
    p.prepared(id,generation());EXPECT_EQ(p.stats().finished,0u);
    auto wrong=generation();++wrong.landRevision;p.rendered(2,1,7,wrong);
    wrong=generation();wrong.grassEye[0]=.9;p.rendered(2,1,7,wrong);
    p.rendered(3,1,7,generation());p.rendered(2,0,7,generation());p.rendered(2,1,8,generation());
    EXPECT_THROW(p.finish(id,P::Outcome::Published),std::invalid_argument);
    EXPECT_EQ(p.stats().finished,0u);p.frame(8);at(25);p.rendered(2,1,7,generation());p.collect();
    auto result=rows(out);ASSERT_EQ(result.size(),1u);const auto& r=result[0];
    EXPECT_EQ(r.at("outcome"),"published");EXPECT_EQ(r.at("publication_ms"),"15");EXPECT_EQ(r.at("publication_frames"),"3");
    EXPECT_EQ(r.at("cpu_start_ms"),"2");EXPECT_EQ(r.at("cpu_end_ms"),"5");EXPECT_EQ(r.at("gpu_ready_ms"),"8");
    EXPECT_EQ(r.at("prepared_ms"),"8");EXPECT_EQ(r.at("land_topology"),"99");EXPECT_EQ(r.at("water_revision"),"9");
}
TEST(PublicationProfiler, GrassHasIndependentIdentityAndReplacesAnUndrawnGeneration) {
    const auto out=path("grass");at(0);P p(out,now);p.frame(1);
    const auto terrain=p.begin(key());at(4);p.prepared(terrain,generation());
    const auto grass=p.begin(key(),P::Kind::Grass);EXPECT_NE(terrain,grass);
    at(6);p.prepared(grass,generation());at(10);p.rendered(2,1,7,generation());
    const auto failed=p.begin(key(),P::Kind::Grass);at(12);p.finish(failed,P::Outcome::PreparationFailed);p.collect();
    auto result=rows(out);ASSERT_EQ(result.size(),3u);
    EXPECT_EQ(result[0]["outcome"],"replaced_before_draw");EXPECT_TRUE(result[0]["publication_ms"].empty());
    EXPECT_EQ(result[1]["kind"],"grass");EXPECT_EQ(result[1]["publication_ms"],"6");
    EXPECT_EQ(result[2]["outcome"],"preparation_failed");EXPECT_TRUE(result[2]["publication_frames"].empty());
}
TEST(PublicationProfiler, FixedSlotsReportOverflowAndLateNotificationsCannotModifyReusedSlots) {
    const auto out=path("bounded");at(0);
    {
        P p(out,now);std::array<std::uint64_t,P::capacity> ids{};
        for(auto& id:ids) id=p.begin(key());EXPECT_EQ(p.stats().peakActive,P::capacity);
        EXPECT_EQ(p.begin(key()),0u);at(2);
        for(auto id:ids) p.finish(id,P::Outcome::Coalesced);
        EXPECT_EQ(p.begin(key()),0u); // Completed rows retain their slots until drained.
        p.collect();EXPECT_EQ(p.stats().pendingRows,0u);const auto reused=p.begin(key());EXPECT_GT(reused,ids.back());
        at(4);p.phase(ids[0],P::Phase::CpuEnd);p.finish(ids[0],P::Outcome::CpuFailed);p.prepared(ids[0],generation());
        EXPECT_EQ(p.stats().active,1u);EXPECT_EQ(p.stats().dropped,2u);
    }
    auto result=rows(out);ASSERT_EQ(result.size(),P::capacity+2);
    EXPECT_EQ(result[P::capacity]["outcome"],"trace_overflow");EXPECT_EQ(result[P::capacity]["dropped_starts"],"2");
    EXPECT_TRUE(result[P::capacity]["elapsed_ms"].empty());
    EXPECT_EQ(result.back()["outcome"],"missing_shutdown");EXPECT_TRUE(result.back()["cpu_end_ms"].empty());
}
TEST(PublicationProfiler, UnknownFrameIsMissingAndEpochDiscardDoesNotPublish) {
    const auto out=path("epochs");at(0);P p(out,now);const auto id=p.begin(key());
    p.prepared(id,generation());at(5);p.discardOlderEpochs(3);p.rendered(2,1,7,generation());p.collect();
    auto result=rows(out);ASSERT_EQ(result.size(),1u);EXPECT_EQ(result[0]["outcome"],"obsolete");
    for(const char* name:{"start_frame","end_frame","elapsed_frames","publication_ms","publication_frames"}) EXPECT_TRUE(result[0][name].empty());
    EXPECT_EQ(result[0]["elapsed_ms"],"5");
}
TEST(PublicationProfiler, DisabledDoesNotReadClockOrRetainWork) {
    clocks=0;P p("",now);p.frame(3);EXPECT_EQ(p.begin(key()),0u);
    p.phase(5,P::Phase::CpuStart);p.prepared(5,generation());p.rendered(2,1,7,generation());
    p.finish(5,P::Outcome::CpuFailed);p.discardOlderEpochs(3);p.collect();
    EXPECT_EQ(clocks,0u);EXPECT_EQ(p.stats().begun,0u);
}

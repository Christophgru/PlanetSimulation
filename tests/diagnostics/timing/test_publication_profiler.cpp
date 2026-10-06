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
    clocks=0;P p("",now);p.frame(3);p.captureMode(true);EXPECT_EQ(p.begin(key()),0u);
    p.phase(5,P::Phase::CpuStart);p.prepared(5,generation());p.rendered(2,1,7,generation());
    p.contactBound(5,0,generation());p.terrain(2,1,7);
    p.finish(5,P::Outcome::CpuFailed);p.discardOlderEpochs(3);p.collect();
    EXPECT_EQ(clocks,0u);EXPECT_EQ(p.stats().begun,0u);
}
TEST(PublicationProfiler, CpuTerrainIncludesGrassPreparationAndCaptureModeIsAdmissionScoped) {
    const auto out=path("capture-cpu");at(0);P p(out,now);p.captureMode(true);
    auto k=key();k.resident=false;const auto id=p.begin(k);EXPECT_EQ(p.terrain(2,1,7),id);
    p.prepared(id,generation());auto drawn=generation();drawn.grassEye={.4,.5,.6};
    p.captureMode(false);at(10);p.rendered(2,1,7,drawn);p.collect();auto result=rows(out);
    ASSERT_EQ(result.size(),1u);EXPECT_EQ(result[0]["outcome"],"published");EXPECT_EQ(result[0]["capture_mode"],"1");
    EXPECT_TRUE(result[0]["grass_eye_x"].empty());EXPECT_EQ(p.terrain(2,1,7),0u);
}
TEST(PublicationProfiler, HandoffCoexistsWithTerrainAndRequiresActualCharacterContacts) {
    const auto out=path("handoff");at(0);P p(out,now);p.frame(1);
    auto k=key();k.resident=false;const auto terrain=p.begin(k);p.prepared(terrain,generation());
    const auto handoff=p.begin(k,P::Kind::Handoff);at(4);
    {P::Preparation preparation(&p,handoff);preparation.bound(0,generation());}
    p.rendered(2,1,7,generation());EXPECT_EQ(p.stats().active,1u);
    auto wrong=generation();++wrong.landRevision;p.rendered(2,1,7,wrong,true);EXPECT_EQ(p.stats().active,1u);
    auto drawn=generation();drawn.grassEye={.4,.5,.6};p.frame(3);at(10);p.rendered(2,1,7,drawn,true);p.collect();
    auto result=rows(out);ASSERT_EQ(result.size(),2u);EXPECT_EQ(result[0]["outcome"],"published");
    EXPECT_EQ(result[1]["kind"],"handoff");EXPECT_EQ(result[1]["outcome"],"published");
    EXPECT_EQ(result[1]["origin_body"],"0");EXPECT_EQ(result[1]["body"],"1");EXPECT_EQ(result[1]["contact_bound_ms"],"4");
    EXPECT_EQ(result[1]["publication_ms"],"10");
    const auto pending=p.begin(k,P::Kind::Handoff);p.contactBound(pending,0,generation());
    auto replacement=generation();++replacement.landRevision;
    const auto newer=p.begin(k);p.prepared(newer,replacement);p.rendered(2,1,7,replacement,true);p.collect();
    result=rows(out);ASSERT_EQ(result.size(),4u);EXPECT_EQ(result[2]["outcome"],"replaced_before_draw");
    EXPECT_EQ(result[3]["outcome"],"published");
}
TEST(PublicationProfiler, UnfinishedPreparationClosesFailureAndCpuGrassNeedsItsActualAnchor) {
    const auto out=path("cpu-grass");at(0);P p(out,now);auto k=key();k.resident=false;
    const auto failed=p.begin(k,P::Kind::Grass);{P::Preparation preparation(&p,failed);}
    const auto grass=p.begin(k,P::Kind::Grass);
    {P::Preparation preparation(&p,grass);preparation.ready(generation());}
    auto wrong=generation();wrong.grassEye={.4,.5,.6};p.rendered(2,1,7,wrong);EXPECT_EQ(p.stats().active,1u);
    at(10);p.rendered(2,1,7,generation());p.collect();auto result=rows(out);ASSERT_EQ(result.size(),2u);
    EXPECT_EQ(result[0]["outcome"],"preparation_failed");EXPECT_TRUE(result[0]["publication_ms"].empty());
    EXPECT_EQ(result[1]["outcome"],"published");EXPECT_EQ(std::stod(result[1]["grass_eye_x"]),.1);
}
TEST(PublicationProfiler, CaptureExceptionClosesOnlyUnconsumedAttemptsInItsEpoch) {
    const auto out=path("capture-failure");at(0);P p(out,now);p.captureMode(true);
    const auto first=p.begin(key());p.prepared(first,generation());p.rendered(2,1,7,generation());
    const auto pending=p.begin(key(8));p.prepared(pending,generation());
    auto future=key(9);future.epoch=3;const auto other=p.begin(future);p.prepared(other,generation());
    try {P::CaptureFailure failure(p,2);throw std::runtime_error("draw failed");} catch(const std::runtime_error&) {}
    EXPECT_EQ(p.stats().active,1u);p.finish(other,P::Outcome::Shutdown);p.collect();auto result=rows(out);
    ASSERT_EQ(result.size(),3u);EXPECT_EQ(result[0]["outcome"],"published");
    EXPECT_EQ(result[1]["outcome"],"preparation_failed");EXPECT_TRUE(result[1]["publication_ms"].empty());
    EXPECT_EQ(result[2]["outcome"],"shutdown");
}
TEST(PublicationProfiler, ReloadRequiresLiveExchangeAndEveryMatchingBodyDrawEvenAfterRowsDrain) {
    const auto out=path("reload-complete");at(0);P p(out,now);p.frame(1);
    auto rootKey=key();rootKey.body=P::unknown;
    const auto root=p.begin(rootKey,P::Kind::Reload);
    const auto first=p.begin(key(),P::Kind::Terrain,root);
    auto secondKey=key(8);secondKey.body=0;
    const auto second=p.begin(secondKey,P::Kind::Terrain,root);
    EXPECT_EQ(p.child(root,1),first);EXPECT_EQ(p.child(root,0),second);
    at(4);p.prepared(first,generation());p.prepared(second,generation());
    p.rendered(2,1,7,generation());p.sceneRendered(2);EXPECT_EQ(p.stats().finished,0u);
    at(8);p.committed(root);p.sceneRendered(3);p.sceneRendered(2);EXPECT_EQ(p.stats().finished,0u);
    at(10);p.rendered(2,1,7,generation());p.sceneRendered(2);p.collect();
    EXPECT_EQ(p.stats().active,2u);auto result=rows(out);ASSERT_EQ(result.size(),1u);
    EXPECT_EQ(result[0]["parent_attempt"],std::to_string(root));
    auto wrong=generation();++wrong.waterRevision;p.rendered(2,0,8,wrong);
    p.sceneRendered(2);EXPECT_EQ(p.stats().active,2u);
    p.frame(5);at(15);p.rendered(2,0,8,generation());p.sceneRendered(2);p.collect();
    result=rows(out);ASSERT_EQ(result.size(),3u);
    const auto& r=result[1];EXPECT_EQ(r.at("kind"),"reload");EXPECT_EQ(r.at("outcome"),"published");
    EXPECT_EQ(r.at("scene_exchange_ms"),"8");EXPECT_EQ(r.at("publication_ms"),"15");
    EXPECT_EQ(r.at("publication_frames"),"4");EXPECT_EQ(r.at("child_attempts"),"2");
    EXPECT_TRUE(r.at("land_revision").empty());
}
TEST(PublicationProfiler, ReloadFailuresCascadeAndEmptySceneStillNeedsPostExchangeDraw) {
    const auto out=path("reload-terminal");at(0);P p(out,now);
    for(auto outcome:{P::Outcome::InvalidConfig,P::Outcome::Superseded,P::Outcome::PreparationFailed,P::Outcome::Shutdown}) {
        const auto root=p.begin(key(),P::Kind::Reload);
        const auto c=p.begin(key(),P::Kind::Terrain,root);p.prepared(c,generation());
        p.finish(root,outcome);p.committed(root);p.rendered(2,1,7,generation());p.sceneRendered(2);
    }
    const auto empty=p.begin(key(),P::Kind::Reload);p.sceneRendered(2);EXPECT_EQ(p.stats().active,1u);
    at(10);p.committed(empty);EXPECT_EQ(p.stats().active,1u);
    at(12);p.sceneRendered(2);p.collect();auto result=rows(out);ASSERT_EQ(result.size(),9u);
    for(std::size_t i=0;i<8;++i) EXPECT_TRUE(result[i]["publication_ms"].empty());
    EXPECT_EQ(result.back()["outcome"],"published");EXPECT_EQ(result.back()["child_attempts"],"0");
}
TEST(PublicationProfiler, DroppedReloadChildCannotProduceSuccessfulSceneLatency) {
    const auto out=path("reload-overflow");at(0);P p(out,now);
    const auto root=p.begin(key(),P::Kind::Reload);
    std::array<std::uint64_t,P::capacity-1> ids{};
    for(auto& id:ids) id=p.begin(key());
    EXPECT_EQ(p.begin(key(),P::Kind::Terrain,root),0u);
    for(auto id:ids) p.finish(id,P::Outcome::Obsolete);
    p.collect();p.committed(root);p.sceneRendered(2);p.collect();auto result=rows(out);
    ASSERT_EQ(result.size(),P::capacity+1);EXPECT_EQ(result.back()["outcome"],"trace_incomplete");
    EXPECT_TRUE(result.back()["publication_ms"].empty());EXPECT_EQ(result.back()["child_attempts"],"0");
}
TEST(PublicationProfiler, CaptureReplacementClosesCommittedChildAndIncompleteRootAfterDraw) {
    const auto out=path("reload-capture-replacement");at(0);P p(out,now);p.captureMode(true);
    auto rootKey=key();rootKey.body=P::unknown;
    const auto root=p.begin(rootKey,P::Kind::Reload);
    const auto child=p.begin(key(),P::Kind::Terrain,root);p.prepared(child,generation());p.committed(root);
    const auto grass=p.begin(key(),P::Kind::Grass);p.prepared(grass,generation());
    EXPECT_EQ(p.stats().active,3u); // Equivalent capture grass leaves the child consumable.
    auto changed=generation();++changed.landRevision;
    const auto replacement=p.begin(key(8));p.prepared(replacement,changed);
    EXPECT_EQ(p.stats().active,2u); // Root and replacement, closed child/grass.
    p.collect();p.rendered(2,1,8,changed);EXPECT_EQ(p.stats().active,1u);
    p.sceneRendered(2);p.collect();EXPECT_EQ(p.stats().active,0u);
    const auto result=rows(out);ASSERT_EQ(result.size(),4u);
    for(const auto& row:result) {
        if(row.at("attempt")==std::to_string(root)) {
            EXPECT_EQ(row.at("outcome"),"trace_incomplete");EXPECT_TRUE(row.at("publication_ms").empty());
        } else if(row.at("attempt")==std::to_string(child)) {
            EXPECT_EQ(row.at("outcome"),"replaced_before_draw");EXPECT_TRUE(row.at("publication_ms").empty());
        } else if(row.at("attempt")==std::to_string(replacement)) EXPECT_EQ(row.at("outcome"),"published");
    }
}

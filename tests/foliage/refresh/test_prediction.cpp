#include <gtest/gtest.h>
#include "rendering/runtime/terrain/refresh/GrassRefresh.h"

using rendering::GrassRefresh;

TEST(GrassRefresh, SurfaceForecastUsesClampedMovementInsteadOfGpuWallTime) {
    GrassRefresh planner;
    const glm::dvec3 start{1000,0,0};
    planner.observe(start,0,1,1,22.5);planner.submitted(start);
    // 110 ms publication latency advances only 60 ms of surface movement:
    // one 50 ms clamped step plus a 10 ms polling frame, at 80 m/s.
    const double angle=80*.06/1000;
    planner.published(.11,{1000*std::cos(angle),1000*std::sin(angle),0},80);
    EXPECT_NEAR(planner.surfaceHorizon(),.09,1e-8);
    EXPECT_GT(planner.horizon(),planner.surfaceHorizon());
}

TEST(GrassRefresh, DelayedPublicationAcrossSteepDescentKeepsCoverage) {
    GrassRefresh planner;
    constexpr double margin=22.5,delay=.15,step=.01;
    const glm::dvec3 velocity{80,-200,0};
    glm::dvec3 installed=.5*velocity*delay,staged{0}; double readyAt=-1;
    int publications=0;
    // Warm motion observation before entering the slope. Forecasting includes
    // height, while a horizontal-only/half-margin trigger loses this reserve.
    planner.observe(-step*velocity,-step,1,1,margin);
    for(int frame=0;frame<300;++frame) {
        const double now=frame*step;const auto eye=velocity*now;
        planner.observe(eye,now,1,1,margin);
        if(readyAt>=0 && now+1e-9>=readyAt) {
            installed=staged;readyAt=-1;planner.published(delay);++publications;
        }
        const auto path=planner.predict(eye);
        if(readyAt<0 && GrassRefresh::needsRefresh(path,installed,margin)) {
            staged=GrassRefresh::anchor(path,margin);readyAt=now+delay;
        }
        EXPECT_LT(glm::length(eye-installed),margin) << frame;
    }
    EXPECT_GT(publications,10);
}

TEST(GrassRefresh, DelayedPatchIsRejectedIfMovementStopsOrReverses) {
    GrassRefresh planner;constexpr double margin=22.5;
    planner.observe({0,0,0},0,1,1,margin);
    planner.observe({10,0,0},.05,1,1,margin);
    const auto staged=GrassRefresh::anchor(planner.predict({10,0,0}),margin);
    EXPECT_TRUE(GrassRefresh::covers(staged,{50,0,0},margin));
    EXPECT_FALSE(GrassRefresh::covers(staged,{10,0,0},margin));
    EXPECT_FALSE(GrassRefresh::covers(staged,{-10,0,0},margin));
    EXPECT_TRUE(GrassRefresh::covers({10,0,0},{10,0,0},margin));
}

TEST(GrassRefresh, RefreshAnticipatesIntermediateCrestEvenWhenEndpointsAreFlat) {
    GrassRefresh::Path path{{{0,0,0},{4,16,0},{8,30,0},{12,16,0},{16,0,0}}};
    constexpr double margin=22.5;
    EXPECT_TRUE(GrassRefresh::needsRefresh(path,{0,0,0},margin));
    const auto anchor=GrassRefresh::anchor(path,margin);
    EXPECT_EQ(anchor,path.back());
    path.fill({0,0,0});
    EXPECT_EQ(GrassRefresh::anchor(path,margin),glm::dvec3(0));
}

TEST(GrassRefresh, StopsReversalsTeleportsAndSceneChangesDiscardStaleMotion) {
    GrassRefresh planner;constexpr double margin=22.5;
    planner.observe({0,0,0},0,1,1,margin);
    planner.observe({1,0,0},.1,1,1,margin);
    EXPECT_GT(planner.predict({1,0,0}).back().x,1);
    planner.observe({0,0,0},.2,1,1,margin);
    EXPECT_LT(planner.predict({0,0,0}).back().x,0);
    planner.observe({0,0,0},.3,1,1,margin);
    EXPECT_EQ(planner.predict({0,0,0}).back(),glm::dvec3(0));
    for(const auto& state:std::array<std::array<int,2>,3>{{{{1,2}},{{2,2}},{{2,1}}}}) {
        planner.observe({1000,0,0},.4,state[0],state[1],margin);
        EXPECT_EQ(planner.predict({1000,0,0}).back(),glm::dvec3(1000,0,0));
    }
    planner.published(10);
    EXPECT_LE(planner.horizon(),.5);
    auto path=planner.predict({1000,0,0});path.back()={10000,10000,0};
    EXPECT_LE(glm::length(GrassRefresh::anchor(path,margin)-path.front()),2*margin+1e-9);
}

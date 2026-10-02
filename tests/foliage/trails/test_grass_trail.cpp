#include "rendering/foliage/trails/GrassTrail.h"
#include <gtest/gtest.h>
#include <limits>
#include <cmath>
#include <stdexcept>

using rendering::GrassTrail;
TEST(GrassTrail, GroundedMotionPersistsAcrossIdleButFlightAndRelocationBreakPaths) {
    GrassTrail trail;
    trail.observe({0,0,100},true); trail.observe({.1,0,100},true);
    trail.observe({.1,0,100.4},true); // Radial remesh movement is not walking.
    EXPECT_TRUE(trail.segments().empty());
    trail.observe({.4,0,100},true);
    ASSERT_EQ(trail.segments().size(),1);
    const auto revision=trail.revision();
    trail.observe({.4,0,100},true); EXPECT_EQ(trail.revision(),revision);
    trail.observe({.8,0,105},false); trail.observe({2,0,100},true);
    EXPECT_EQ(trail.segments().size(),1);
    trail.observe({2.4,0,100},true); EXPECT_EQ(trail.segments().size(),2);
    trail.observe({20,0,100},true); EXPECT_EQ(trail.segments().size(),2);
    trail.breakPath(); trail.observe({25,0,100},true);
    EXPECT_EQ(trail.segments().size(),2);
}
TEST(GrassTrail, BoundedHistoryRetainsLatestMarksAndRestoresExactly) {
    GrassTrail trail;
    for (std::size_t i=0;i<=GrassTrail::capacity+10;++i) trail.observe({i*.3,0,6371000},true);
    ASSERT_EQ(trail.segments().size(),GrassTrail::capacity);
    EXPECT_NEAR(trail.segments().front().start.x,3,1e-10);
    GrassTrail restored; restored.restore(trail.segments());
    const auto origin=trail.segments().back().end;
    EXPECT_EQ(restored.hierarchy(origin),trail.hierarchy(origin));
    EXPECT_EQ(restored.hierarchy(origin).size(),4*(2*GrassTrail::capacity-1));
    restored.observe(origin,true); EXPECT_EQ(restored.segments().size(),GrassTrail::capacity);
}
TEST(GrassTrail, ReplayRejectsNonfiniteOversizedAndTeleportSegments) {
    GrassTrail trail;
    EXPECT_THROW(trail.observe({std::numeric_limits<double>::infinity(),0,0},true),std::invalid_argument);
    EXPECT_THROW(trail.restore({{{0,0,0},{2,0,0}}}),std::invalid_argument);
    EXPECT_THROW(trail.restore(std::vector<rendering::TrailSegment>(GrassTrail::capacity+1)),std::invalid_argument);
    EXPECT_THROW(trail.restore({{{0,0,0},{0,std::numeric_limits<double>::quiet_NaN(),0}}}),std::invalid_argument);
}
TEST(GrassTrail, HierarchyMatchesCapsuleDistancesAndPrunesDistantMarksAtPlanetScale) {
    GrassTrail trail;
    const glm::dvec3 origin(0,0,6371000);
    for (int i=0;i<64;++i) trail.observe(origin+glm::dvec3(i*.3,std::sin(i*.3),0),true);
    const auto tree=trail.hierarchy(origin);
    for (int x=-2;x<65;++x) for (double y:{-.4,0.,.4,3.}) {
        const glm::dvec3 local(x*.3,y,0),point=origin+local;
        double brute=.6,accelerated=.6;
        for (const auto& s:trail.segments()) {
            const auto delta=s.end-s.start;
            const auto t=glm::clamp(glm::dot(point-s.start,delta)/glm::dot(delta,delta),0.,1.);
            brute=std::min(brute,glm::length(point-s.start-t*delta));
        }
        std::size_t node=0,visited=0;
        while (node<tree.size()/4) {
            ++visited;
            const auto lo=tree[node*4],hi=tree[node*4+1];
            const glm::dvec3 a=glm::vec3(lo),b=glm::vec3(hi);
            if (glm::any(glm::lessThan(local,a))||glm::any(glm::greaterThan(local,b))) { node=lo.w; continue; }
            if (hi.w>0) {
                const glm::dvec3 start=glm::vec3(tree[node*4+2]),end=glm::vec3(tree[node*4+3]);
                const auto delta=end-start;
                const auto t=glm::clamp(glm::dot(local-start,delta)/glm::dot(delta,delta),0.,1.);
                accelerated=std::min(accelerated,glm::length(local-start-t*delta));
            }
            ++node;
        }
        EXPECT_NEAR(accelerated,brute,2e-6);
        if (y==3) EXPECT_EQ(visited,1);
    }
}

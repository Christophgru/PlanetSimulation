#include <gtest/gtest.h>
#include "rendering/FrameReuse.h"
#include "rendering/FrameProfiler.h"

TEST(FrameRate, MeasuresWallTimeAndIgnoresInvalidSamples) {
    rendering::FrameRate rate;
    for(int i=0;i<30;++i) rate.sample(0.01);
    EXPECT_NEAR(rate.fps,100,1e-10); EXPECT_NEAR(rate.milliseconds,10,1e-10);
    rate.sample(0); rate.sample(-1); rate.sample(std::numeric_limits<double>::quiet_NaN());
    EXPECT_NEAR(rate.fps,100,1e-10);
}
TEST(FrameReuse, RequiresStoppedSimulationAndIdenticalCameraGeometryAndSize) {
    rendering::FrameReuse cache;
    const glm::mat4 view(1);
    const std::vector<std::uint64_t> revisions{1,1};
    EXPECT_FALSE(cache.matches(view,60,1280,720,20,revisions,true,false));
    cache.remember(view,60,1280,720,20,revisions);
    EXPECT_TRUE(cache.matches(view,60,1280,720,20,revisions,true,false));
    EXPECT_FALSE(cache.matches(view,60,1280,720,20,revisions,false,false));
    EXPECT_FALSE(cache.matches(view,60,1280,720,20,revisions,true,true));
    EXPECT_FALSE(cache.matches(view,60,1280,720,20.001,revisions,true,false));
    EXPECT_FALSE(cache.matches(view,70,1280,720,20,revisions,true,false));
    EXPECT_FALSE(cache.matches(view,60,1281,720,20,revisions,true,false));
    EXPECT_FALSE(cache.matches(view,60,1280,721,20,revisions,true,false));
    EXPECT_FALSE(cache.matches(view,60,1280,720,20,{1,2},true,false));
    glm::mat4 moved=view; moved[3][0]=0.00001f;
    EXPECT_FALSE(cache.matches(moved,60,1280,720,20,revisions,true,false));
    cache.invalidate();
    EXPECT_FALSE(cache.matches(view,60,1280,720,20,revisions,true,false));
}
TEST(FrameReuse, PreciseEyeAndCameraModeInvalidateEvenWithTheSameFloatView) {
    rendering::FrameReuse cache;
    const glm::mat4 view(1);
    const glm::dvec3 eye(1e8,1,0);
    cache.remember(view,60,1280,720,20,{1},eye,0);
    EXPECT_TRUE(cache.matches(view,60,1280,720,20,{1},true,false,eye,0));
    EXPECT_FALSE(cache.matches(view,60,1280,720,20,{1},true,false,eye,1));
    const glm::dvec3 moved=eye+glm::dvec3(0.001,0,0);
    EXPECT_EQ(glm::vec3(moved),glm::vec3(eye)); // Same float view can hide real movement.
    EXPECT_FALSE(cache.matches(view,60,1280,720,20,{1},true,false,moved,0));
}

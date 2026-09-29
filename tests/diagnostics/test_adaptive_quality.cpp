#include <gtest/gtest.h>
#include "rendering/diagnostics/AdaptiveQuality.h"

TEST(AdaptiveQuality, LimitsPixelDetailToAvailableMemory) {
    rendering::AdaptiveQuality quality(256ull * 1024 * 1024);
    const auto [w, h] = quality.size(1280, 720);
    EXPECT_LT(w, 1280);
    EXPECT_EQ(double(w) / 1280, quality.scale());
    EXPECT_LT(double(w) * h * 112, 256.0 * 1024 * 1024 * .25);
}
TEST(AdaptiveQuality, ReducesDetailAfterSustainedSlowFramesAndRecoversWithHysteresis) {
    rendering::AdaptiveQuality quality;
    quality.size(1280, 720);
    for (int i=0; i<20; ++i) quality.observe(1.6+i*.05, 75, true);
    EXPECT_LT(quality.scale(), 1);
    const double reduced = quality.scale();
    for (int i=0; i<5; ++i) quality.observe(2.7+i*.05, 18, true);
    EXPECT_EQ(quality.scale(), reduced); // dwell and rolling average prevent oscillation
    for (int i=0; i<80; ++i) quality.observe(4.0+i*.05, 18, true);
    EXPECT_GT(quality.scale(), reduced);
}
TEST(AdaptiveQuality, PausedSceneDoesNotDriveQualityAndWindowCanResize) {
    rendering::AdaptiveQuality quality;
    quality.size(800, 600);
    for (int i=0; i<100; ++i) quality.observe(i*.1, 200, false);
    EXPECT_EQ(quality.scale(), 1);
    EXPECT_EQ(quality.size(1200, 700), (std::pair<int,int>{1200, 700}));
}

#include <gtest/gtest.h>
#include <vector>
#include "rendering/RenderDiagnostics.h"

namespace {
constexpr int kWidth = 6;
constexpr int kHeight = 5;
const std::vector<double> kSunColor = {1.0, 0.5, 0.2};
const std::vector<double> kPlanetColor = {0.2, 0.4, 1.0};

std::vector<unsigned char> backgroundFrame() {
    std::vector<unsigned char> rgba(kWidth * kHeight * 4);
    for (int i = 0; i < kWidth * kHeight; ++i) {
        rgba[i * 4] = 25;
        rgba[i * 4 + 1] = 25;
        rgba[i * 4 + 2] = 38;
        rgba[i * 4 + 3] = 255;
    }
    return rgba;
}

void paint(std::vector<unsigned char>& rgba, int x, int y,
           unsigned char r, unsigned char g, unsigned char b) {
    const int index = (y * kWidth + x) * 4;
    rgba[index] = r;
    rgba[index + 1] = g;
    rgba[index + 2] = b;
}
}

TEST(RenderDiagnosticsTest, CountsBothBodiesAndTheirSeparateBounds) {
    auto rgba = backgroundFrame();
    for (int y = 1; y <= 2; ++y) {
        for (int x = 1; x <= 2; ++x) paint(rgba, x, y, 255, 127, 51);
    }
    paint(rgba, 4, 3, 51, 102, 255);

    const auto result = rendering::analyzeFrame(rgba, kWidth, kHeight,
                                                kSunColor, kPlanetColor);
    EXPECT_EQ(result.background[0], 25);
    EXPECT_EQ(result.drawn.count, 5);
    EXPECT_EQ(result.drawn.minX, 1);
    EXPECT_EQ(result.drawn.maxX, 4);
    EXPECT_EQ(result.drawn.minY, 1);
    EXPECT_EQ(result.drawn.maxY, 3);
    EXPECT_EQ(result.sun.count, 4);
    EXPECT_EQ(result.planet.count, 1);
    EXPECT_TRUE(result.bodiesVisible());
    EXPECT_TRUE(result.bodiesSeparate());
}

TEST(RenderDiagnosticsTest, ColorToleranceAcceptsFramebufferRoundingButRejectsOtherColors) {
    auto rgba = backgroundFrame();
    paint(rgba, 1, 1, 255, 120, 59); // Within 8 of the configured Sun color.
    paint(rgba, 2, 1, 255, 119, 51); // Nine below the expected green channel.

    const auto result = rendering::analyzeFrame(rgba, kWidth, kHeight,
                                                kSunColor, kPlanetColor);
    EXPECT_EQ(result.drawn.count, 2);
    EXPECT_EQ(result.sun.count, 1);
    EXPECT_EQ(result.planet.count, 0);
    EXPECT_FALSE(result.bodiesVisible());
}

TEST(RenderDiagnosticsTest, MissingOrOverlappingBodiesFailVisibilityCheck) {
    auto rgba = backgroundFrame();
    paint(rgba, 1, 1, 255, 127, 51);
    auto onlySun = rendering::analyzeFrame(rgba, kWidth, kHeight,
                                           kSunColor, kPlanetColor);
    EXPECT_FALSE(onlySun.bodiesVisible());
    EXPECT_FALSE(onlySun.bodiesSeparate());

    paint(rgba, 3, 3, 255, 127, 51);
    paint(rgba, 2, 2, 51, 102, 255);
    auto overlapping = rendering::analyzeFrame(rgba, kWidth, kHeight,
                                                kSunColor, kPlanetColor);
    EXPECT_TRUE(overlapping.bodiesVisible());
    EXPECT_FALSE(overlapping.bodiesSeparate());
}

TEST(RenderDiagnosticsTest, InvalidFramebufferDimensionsAreRejected) {
    auto rgba = backgroundFrame();
    EXPECT_THROW(rendering::analyzeFrame(rgba, 0, kHeight, kSunColor, kPlanetColor),
                 std::invalid_argument);
    rgba.pop_back();
    EXPECT_THROW(rendering::analyzeFrame(rgba, kWidth, kHeight, kSunColor, kPlanetColor),
                 std::invalid_argument);
}

TEST(RenderDiagnosticsTest, ShortColorArrayCannotBeMistakenForABody) {
    auto rgba = backgroundFrame();
    paint(rgba, 1, 1, 255, 127, 51);
    auto result = rendering::analyzeFrame(rgba, kWidth, kHeight,
                                          {1.0}, kPlanetColor);
    EXPECT_EQ(result.drawn.count, 1);
    EXPECT_EQ(result.sun.count, 0);
}

TEST(RenderDiagnosticsTest, DetectsDarkAndLightTerrainPixelsAsPlanetColored) {
    auto rgba = backgroundFrame();
    paint(rgba, 1, 1, 26, 51, 128);   // Half-strength blue terrain.
    paint(rgba, 2, 1, 61, 122, 255);  // Light terrain, blue clamped to 255.
    paint(rgba, 3, 1, 100, 100, 100); // Unrelated gray.
    const auto result = rendering::analyzeFrame(rgba, kWidth, kHeight,
                                                kSunColor, kPlanetColor);
    EXPECT_EQ(result.planet.count, 2);
    EXPECT_EQ(result.sun.count, 0);
}

TEST(RenderDiagnosticsTest, RecognizesLandscapePaletteAndWaterLikePixels) {
    auto rgba = backgroundFrame();
    paint(rgba, 1, 1, 255, 127, 51);
    paint(rgba, 3, 1, 88, 150, 66); // Green plain.
    paint(rgba, 4, 1, 34, 69, 84);  // Blue water over dark ground.
    const auto result = rendering::analyzeFrame(rgba, kWidth, kHeight,
        kSunColor, kPlanetColor, true);
    EXPECT_EQ(result.sun.count, 1);
    EXPECT_EQ(result.planet.count, 2);
    EXPECT_EQ(result.waterLike.count, 1);
    EXPECT_TRUE(result.bodiesSeparate());
}

TEST(RenderDiagnosticsTest, ConfiguredBackgroundAndStarsDoNotBecomePlanetPixels) {
    auto rgba = backgroundFrame();
    paint(rgba, 0, 0, 203, 220, 255);
    paint(rgba, 3, 2, 88, 150, 66);
    const auto result = rendering::analyzeFrame(
        rgba, kWidth, kHeight, kSunColor, kPlanetColor, true,
        {25.0 / 255.0, 25.0 / 255.0, 38.0 / 255.0}, {0.82, 0.9, 1.0});
    EXPECT_EQ(result.background, (std::array<unsigned char, 3>{25, 25, 38}));
    EXPECT_EQ(result.starLike.count, 1);
    EXPECT_EQ(result.planet.count, 1);
    EXPECT_EQ(result.planet.minX, 3);
    EXPECT_EQ(result.planet.maxX, 3);
}

TEST(RenderDiagnosticsTest, WhiteSunIsRestrictedToItsProjectedRegionAndNotCountedAsStars) {
    auto rgba = backgroundFrame();
    paint(rgba, 1, 1, 250, 250, 250); // Sun.
    paint(rgba, 5, 4, 250, 250, 250); // Identical white background star.
    paint(rgba, 4, 2, 88, 150, 66);   // Earth.
    const std::vector<double> white{250.0 / 255, 250.0 / 255, 250.0 / 255};
    const auto result = rendering::analyzeFrame(rgba, kWidth, kHeight, white, kPlanetColor,
        true, {}, {0.82, 0.9, 1.0}, std::array<int, 4>{0, 0, 2, 2});
    EXPECT_EQ(result.sun.count, 1);
    EXPECT_EQ(result.starLike.count, 1);
    EXPECT_TRUE(result.bodiesSeparate());
    EXPECT_EQ(result.sun.minX, 1);
    EXPECT_EQ(result.sun.maxY, 1);
}

TEST(RenderDiagnosticsTest, DepthIdentifiesShadowedTerrainWithoutConfusingBackgroundOrSun) {
    auto rgba = backgroundFrame();
    std::vector<float> depth(kWidth * kHeight, 1.0f);
    paint(rgba, 1, 1, 3, 5, 2); // Within the Sun bounds: not a planet.
    paint(rgba, 3, 2, 3, 5, 2); // Shadowed terrain.
    paint(rgba, 4, 2, 3, 5, 2); // Same color but sky depth: not geometry.
    paint(rgba, 5, 4, 3, 5, 2); // Opaque, but outside the planet's bounds.
    depth[1 * kWidth + 1] = 0.5f;
    depth[2 * kWidth + 3] = 0.5f;
    depth[4 * kWidth + 5] = 0.5f;
    const auto result = rendering::analyzeFrame(rgba, kWidth, kHeight, kSunColor, kPlanetColor,
        false, {}, {}, std::array<int, 4>{0, 0, 2, 2}, depth, std::array<int, 4>{0, 0, 4, 3});
    EXPECT_EQ(result.planet.count, 1);
    EXPECT_EQ(result.planet.minX, 3);
    EXPECT_EQ(result.planet.minY, 2);
}

TEST(RenderDiagnosticsTest, DepthDoesNotMakeEmptyOrBlackFramesPass) {
    for (bool black : {false, true}) {
        auto rgba = backgroundFrame();
        if (black) paint(rgba, 3, 2, 0, 0, 0);
        std::vector<float> depth(kWidth * kHeight, 0.5f);
        const auto result = rendering::analyzeFrame(rgba, kWidth, kHeight, kSunColor, kPlanetColor,
            false, {}, {}, std::array<int, 4>{0, 0, -1, -1}, depth,
            std::array<int, 4>{0, 0, kWidth - 1, kHeight - 1});
        EXPECT_EQ(result.planet.count, 0);
        EXPECT_FALSE(result.bodiesVisible());
    }
}

TEST(RenderDiagnosticsTest, InvalidDepthDimensionsAreRejected) {
    EXPECT_THROW(rendering::analyzeFrame(backgroundFrame(), kWidth, kHeight, kSunColor, kPlanetColor,
        false, {}, {}, std::nullopt, std::vector<float>{0.5f}), std::invalid_argument);
}

TEST(RenderDiagnosticsTest, LightingMetricsIncludeBlackTerrainButExcludeSunMoonAndSky) {
    std::vector<unsigned char> rgba(kWidth * kHeight * 4, 0);
    std::vector<float> depth(kWidth * kHeight, 1);
    std::vector<unsigned char> ids(kWidth * kHeight, 0);
    paint(rgba, 1, 1, 255, 255, 255); // Sun.
    paint(rgba, 2, 1, 0, 0, 0);       // Invisible but correctly drawn night-side Earth.
    paint(rgba, 3, 1, 255, 255, 255); // Moon.
    paint(rgba, 4, 1, 255, 255, 255); // Star, no geometry depth.
    for (int x = 1; x <= 3; ++x) {
        depth[kWidth + x] = 0.5;
        ids[kWidth + x] = x;
    }
    const std::array<int, 4> wholeScreen{0, 0, kWidth - 1, kHeight - 1};
    const auto metrics = rendering::measureLightingFrame(rgba, depth, kWidth, kHeight,
        wholeScreen, wholeScreen, ids); // Projected Sun bounds deliberately overlap everything.
    EXPECT_EQ(metrics.terrainPixels, 1);
    EXPECT_DOUBLE_EQ(metrics.terrainMeanLuminance, 0);
    EXPECT_EQ(metrics.skyPixels, kWidth * kHeight - 3);
    EXPECT_NEAR(metrics.skyMeanLuminance, 1.0 / metrics.skyPixels, 1e-12);
    paint(rgba, 2, 1, 3, 5, 2);
    const auto visible = rendering::analyzeFrame(rgba, kWidth, kHeight, {1, 1, 1}, kPlanetColor,
        false, {}, {1, 1, 1}, wholeScreen, depth, wholeScreen, ids);
    EXPECT_EQ(visible.planet.count, 1);
    EXPECT_EQ(visible.sun.count, 1);
    EXPECT_EQ(visible.starLike.count, 1);
}

TEST(RenderDiagnosticsTest, LightingMetricsRejectMissingBuffersAndDoNotInventTerrain) {
    auto rgba = backgroundFrame();
    const std::array<int, 4> wholeScreen{0, 0, kWidth - 1, kHeight - 1};
    std::vector<float> depth(kWidth * kHeight, 1);
    EXPECT_THROW(rendering::measureLightingFrame(rgba, {}, kWidth, kHeight, wholeScreen, wholeScreen), std::invalid_argument);
    EXPECT_THROW(rendering::measureLightingFrame(rgba, depth, kWidth, kHeight, wholeScreen, wholeScreen, {2}), std::invalid_argument);
    const auto empty = rendering::measureLightingFrame(rgba, depth, kWidth, kHeight, wholeScreen, wholeScreen);
    EXPECT_EQ(empty.terrainPixels, 0);
    EXPECT_DOUBLE_EQ(empty.terrainMeanLuminance, 0);
}

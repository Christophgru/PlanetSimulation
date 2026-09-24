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

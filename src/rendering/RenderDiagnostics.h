#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace rendering {

struct PixelBounds {
    int count = 0;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = -1;
    int maxY = -1;

    void include(int x, int y) {
        ++count;
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }
};

struct FrameAnalysis {
    std::array<unsigned char, 3> background{};
    PixelBounds drawn;
    PixelBounds sun;
    PixelBounds planet;
    PixelBounds waterLike;
    PixelBounds starLike;

    bool bodiesVisible() const {
        return drawn.count > 0 && sun.count > 0 && planet.count > 0;
    }

    bool bodiesSeparate() const {
        return bodiesVisible() &&
               (sun.maxX < planet.minX || planet.maxX < sun.minX ||
                sun.maxY < planet.minY || planet.maxY < sun.minY);
    }
};

struct LightingFrameMetrics {
    int terrainPixels = 0;
    int skyPixels = 0;
    double terrainMeanLuminance = 0.0;
    double terrainMaxLuminance = 0.0;
    double terrainLuminanceStddev = 0.0;
    double skyMeanLuminance = 0.0;
    int skyInteriorPixels = 0;
    double skyInteriorMeanLuminance = 0.0;
    std::array<double, 3> skyInteriorMeanRGB{};
};

// Include black terrain in the measurement: a moonless night is a valid image.
// Geometry depth and projected bounds distinguish it from a missing render.
inline LightingFrameMetrics measureLightingFrame(const std::vector<unsigned char>& rgba,
                                                  const std::vector<float>& depth,
                                                  int width, int height,
                                                  const std::array<int, 4>& planetRegion,
                                                  const std::array<int, 4>& sunRegion,
                                                  const std::vector<unsigned char>& objectIds = {}) {
    if (width <= 0 || height <= 0 || rgba.size() != static_cast<std::size_t>(width) * height * 4 ||
        depth.size() != static_cast<std::size_t>(width) * height ||
        (!objectIds.empty() && objectIds.size() != depth.size()))
        throw std::invalid_argument("Invalid lighting measurement framebuffer");
    LightingFrameMetrics result;
    const auto contains = [](const std::array<int, 4>& bounds, int x, int y) {
        return x >= bounds[0] && y >= bounds[1] && x <= bounds[2] && y <= bounds[3];
    };
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto index = static_cast<std::size_t>(y) * width + x;
        const double value = (0.2126 * rgba[4 * index] + 0.7152 * rgba[4 * index + 1] +
                              0.0722 * rgba[4 * index + 2]) / 255.0;
        const bool selectedPlanet = objectIds.empty() ?
            contains(planetRegion, x, y) && !contains(sunRegion, x, y) : objectIds[index] == 2;
        if (depth[index] >= 0.0f && depth[index] < 1.0f && selectedPlanet) {
            ++result.terrainPixels;
            result.terrainMeanLuminance += value;
            result.terrainLuminanceStddev += value * value;
            result.terrainMaxLuminance = std::max(result.terrainMaxLuminance, value);
        } else if (depth[index] == 1.0f) {
            ++result.skyPixels;
            result.skyMeanLuminance += value;
            // Multisample color resolves can mix a body's edge into a pixel
            // whose resolved depth/stencil sample still says sky. Measure star
            // visibility only where the surrounding 3x3 pixels are all sky.
            bool interior = true;
            for (int ny = std::max(0, y - 1); ny <= std::min(height - 1, y + 1); ++ny)
                for (int nx = std::max(0, x - 1); nx <= std::min(width - 1, x + 1); ++nx) {
                    const auto neighbor = static_cast<std::size_t>(ny) * width + nx;
                    if (depth[neighbor] != 1.0f || (!objectIds.empty() && objectIds[neighbor] != 0))
                        interior = false;
                }
            if (interior) {
                ++result.skyInteriorPixels;
                result.skyInteriorMeanLuminance += value;
                for (int c = 0; c < 3; ++c) result.skyInteriorMeanRGB[c] += rgba[4 * index + c] / 255.0;
            }
        }
    }
    if (result.terrainPixels) {
        result.terrainMeanLuminance /= result.terrainPixels;
        result.terrainLuminanceStddev = std::sqrt(std::max(0.0,
            result.terrainLuminanceStddev / result.terrainPixels - result.terrainMeanLuminance * result.terrainMeanLuminance));
    }
    if (result.skyPixels) result.skyMeanLuminance /= result.skyPixels;
    if (result.skyInteriorPixels) {
        result.skyInteriorMeanLuminance /= result.skyInteriorPixels;
        for (double& channel : result.skyInteriorMeanRGB) channel /= result.skyInteriorPixels;
    }
    return result;
}

inline bool matchesColor(const std::vector<unsigned char>& pixels, std::size_t index,
                         const std::vector<double>& color) {
    if (color.size() < 3) return false;
    for (int channel = 0; channel < 3; ++channel) {
        const int expected = static_cast<int>(std::lround(color[channel] * 255.0));
        if (std::abs(static_cast<int>(pixels[index + channel]) - expected) > 8) {
            return false;
        }
    }
    return true;
}

inline bool matchesTerrainTint(const std::vector<unsigned char>& pixels,
                               std::size_t index,
                               const std::vector<double>& color) {
    if (color.size() < 3 || color[2] <= 0.0 || color[1] <= 0.0) return false;
    const double factor = pixels[index + 2] < 250 ?
        pixels[index + 2] / (255.0 * color[2]) :
        pixels[index + 1] / (255.0 * color[1]);
    if (factor < 0.2 || factor > 1.3) return false;
    for (int channel = 0; channel < 3; ++channel) {
        const int expected = static_cast<int>(std::lround(
            std::clamp(color[channel] * factor, 0.0, 1.0) * 255.0));
        if (std::abs(static_cast<int>(pixels[index + channel]) - expected) > 8)
            return false;
    }
    return true;
}

inline bool matchesTintAboveBackground(const std::vector<unsigned char>& pixels,
                                       std::size_t index,
                                       const std::array<unsigned char, 3>& background,
                                       const std::vector<double>& color) {
    if (color.size() != 3) return false;
    int strongestDelta = 0;
    double minimumFactor = 0.0;
    double maximumFactor = std::numeric_limits<double>::infinity();
    for (int channel = 0; channel < 3; ++channel) {
        if (color[channel] <= 0.0) continue;
        const int value = pixels[index + channel];
        strongestDelta = std::max(strongestDelta,
            value - static_cast<int>(background[channel]));
        const double scale = 255.0 * color[channel];
        minimumFactor = std::max(minimumFactor,
            (value - 18.0 - background[channel]) / scale);
        if (value < 237) {
            maximumFactor = std::min(maximumFactor,
                (value + 18.0 - background[channel]) / scale);
        }
    }
    return strongestDelta >= 24 && minimumFactor <= maximumFactor;
}

inline FrameAnalysis analyzeFrame(const std::vector<unsigned char>& rgba,
                                  int width, int height,
                                  const std::vector<double>& sunColor,
                                  const std::vector<double>& planetColor,
                                  bool landscapePalette = false,
                                  const std::vector<double>& backgroundColor = {},
                                  const std::vector<double>& starColor = {},
                                  const std::optional<std::array<int, 4>>& sunPixelRegion = std::nullopt,
                                  const std::vector<float>& depth = {},
                                  const std::optional<std::array<int, 4>>& planetPixelRegion = std::nullopt,
                                  const std::vector<unsigned char>& objectIds = {}) {
    if (width <= 0 || height <= 0 ||
        rgba.size() != static_cast<std::size_t>(width) * height * 4) {
        throw std::invalid_argument("Invalid RGBA framebuffer dimensions");
    }
    if (!depth.empty() && depth.size() != static_cast<std::size_t>(width) * height)
        throw std::invalid_argument("Invalid depth framebuffer dimensions");
    if (!objectIds.empty() && objectIds.size() != static_cast<std::size_t>(width) * height)
        throw std::invalid_argument("Invalid object framebuffer dimensions");

    FrameAnalysis analysis;
    if (backgroundColor.size() == 3) {
        std::array<int, 3> expected{};
        for (int channel = 0; channel < 3; ++channel) {
            expected[channel] = static_cast<int>(std::lround(
                std::clamp(backgroundColor[channel], 0.0, 1.0) * 255.0));
        }
        int bestDistance = std::numeric_limits<int>::max();
        for (std::size_t index = 0; index < rgba.size(); index += 4) {
            int distance = 0;
            for (int channel = 0; channel < 3; ++channel) {
                const int delta = static_cast<int>(rgba[index + channel]) -
                                  expected[channel];
                distance += delta * delta;
            }
            if (distance < bestDistance) {
                bestDistance = distance;
                analysis.background = {rgba[index], rgba[index + 1], rgba[index + 2]};
                if (distance == 0) break;
            }
        }
    } else {
        analysis.background = {rgba[0], rgba[1], rgba[2]};
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4;
            if (rgba[index] == analysis.background[0] &&
                rgba[index + 1] == analysis.background[1] &&
                rgba[index + 2] == analysis.background[2]) continue;

            analysis.drawn.include(x, y);
            const bool inSunRegion = !sunPixelRegion ||
                (x >= (*sunPixelRegion)[0] && y >= (*sunPixelRegion)[1] &&
                 x <= (*sunPixelRegion)[2] && y <= (*sunPixelRegion)[3]);
            const bool sunLike = (objectIds.empty() ? inSunRegion : objectIds[index / 4] == 1) && matchesColor(rgba, index, sunColor);
            // Shadowed terrain may be below the color classifier's threshold.
            // Require actual opaque geometry within the projected planet bounds,
            // outside the Sun, and a nonblack pixel distinct from the background.
            const bool selectedPlanet = !objectIds.empty() ? objectIds[index / 4] == 2 : planetPixelRegion && sunPixelRegion &&
                !inSunRegion && x >= (*planetPixelRegion)[0] && y >= (*planetPixelRegion)[1] &&
                x <= (*planetPixelRegion)[2] && y <= (*planetPixelRegion)[3];
            const bool planetGeometry = !depth.empty() && selectedPlanet &&
                depth[index / 4] >= 0.0f && depth[index / 4] < 1.0f &&
                (rgba[index] != 0 || rgba[index + 1] != 0 || rgba[index + 2] != 0);
            const bool starLike = !sunLike && !planetGeometry && (objectIds.empty() || objectIds[index / 4] == 0) && matchesTintAboveBackground(
                rgba, index, analysis.background, starColor);
            if (starLike) analysis.starLike.include(x, y);
            if (sunLike) analysis.sun.include(x, y);
            if (!starLike && !sunLike && (objectIds.empty() || selectedPlanet) && (planetGeometry || matchesColor(rgba, index, planetColor) ||
                matchesTerrainTint(rgba, index, planetColor) ||
                (landscapePalette && rgba[index + 1] > rgba[index] + 6)))
                analysis.planet.include(x, y);
            if (!starLike && landscapePalette &&
                rgba[index + 2] > rgba[index + 1] + 6 &&
                rgba[index + 1] > rgba[index] + 6)
                analysis.waterLike.include(x, y);
        }
    }
    return analysis;
}

} // namespace rendering

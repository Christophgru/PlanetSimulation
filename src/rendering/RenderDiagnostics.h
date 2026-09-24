#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
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
                                  const std::vector<double>& starColor = {}) {
    if (width <= 0 || height <= 0 ||
        rgba.size() != static_cast<std::size_t>(width) * height * 4) {
        throw std::invalid_argument("Invalid RGBA framebuffer dimensions");
    }

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
            const bool starLike = matchesTintAboveBackground(
                rgba, index, analysis.background, starColor);
            if (starLike) analysis.starLike.include(x, y);
            if (matchesColor(rgba, index, sunColor)) analysis.sun.include(x, y);
            if (!starLike && (matchesColor(rgba, index, planetColor) ||
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

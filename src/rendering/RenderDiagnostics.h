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

inline FrameAnalysis analyzeFrame(const std::vector<unsigned char>& rgba,
                                  int width, int height,
                                  const std::vector<double>& sunColor,
                                  const std::vector<double>& planetColor) {
    if (width <= 0 || height <= 0 ||
        rgba.size() != static_cast<std::size_t>(width) * height * 4) {
        throw std::invalid_argument("Invalid RGBA framebuffer dimensions");
    }

    FrameAnalysis analysis;
    analysis.background = {rgba[0], rgba[1], rgba[2]};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4;
            if (rgba[index] == analysis.background[0] &&
                rgba[index + 1] == analysis.background[1] &&
                rgba[index + 2] == analysis.background[2]) continue;

            analysis.drawn.include(x, y);
            if (matchesColor(rgba, index, sunColor)) analysis.sun.include(x, y);
            if (matchesColor(rgba, index, planetColor)) analysis.planet.include(x, y);
        }
    }
    return analysis;
}

} // namespace rendering

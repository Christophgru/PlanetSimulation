#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace rendering {
// Scale only the interactive scene. The final HUD and window stay at native size.
class AdaptiveQuality {
public:
    static constexpr std::array<double, 6> scales{1.0, .8, .65, .5, .35, .25};
    explicit AdaptiveQuality(std::optional<std::uint64_t> availableBytes = std::nullopt)
        : availableBytes_(availableBytes) {}
    void observe(double seconds, double frameMs, bool moving) {
        if (!moving || !std::isfinite(frameMs) || frameMs <= 0 || frameMs > 500) return;
        // A rolling estimate damps one-off terrain uploads and driver stalls.
        averageMs_ = averageMs_ == 0 ? frameMs : averageMs_ * .85 + frameMs * .15;
        if (seconds - lastChange_ < 1.5) return;
        if (averageMs_ > 55 && level_ + 1 < scales.size()) {
            ++level_; lastChange_ = seconds;
        } else if (averageMs_ < 32 && level_ > 0 && fits(level_ - 1, width_, height_)) {
            --level_; lastChange_ = seconds;
        }
    }
    std::pair<int,int> size(int width, int height) {
        width_ = width; height_ = height;
        while (!fits(level_, width, height) && level_ + 1 < scales.size()) ++level_;
        const auto scale = scales[level_];
        return {std::max(1, static_cast<int>(std::lround(width * scale))),
                std::max(1, static_cast<int>(std::lround(height * scale)))};
    }
    double scale() const { return scales[level_]; }
    double averageMs() const { return averageMs_; }
private:
    bool fits(std::size_t level, int width, int height) const {
        if (!availableBytes_) return true;
        // Upper bound for main HDR ping-pong, atmosphere field, reflection,
        // screen color/depth and driver overhead. Reserve 75% for other uses.
        const double bytes = double(width) * height * scales[level] * scales[level] * 112;
        return bytes <= static_cast<double>(*availableBytes_) * .25;
    }
    std::optional<std::uint64_t> availableBytes_;
    std::size_t level_ = 0;
    int width_ = 0, height_ = 0;
    double averageMs_ = 0, lastChange_ = 0;
};
} // namespace rendering

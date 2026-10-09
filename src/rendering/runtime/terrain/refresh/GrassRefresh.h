#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <glm/glm.hpp>

namespace rendering {
// Interactive scheduling only. Positions/velocities are planet-local meters;
// capture keeps its recorded placement centers and deterministic schedule.
class GrassRefresh {
public:
    using Path = std::array<glm::dvec3, 5>;
    void observe(glm::dvec3 eye, double now, std::uint64_t epoch, int mode, double margin) {
        if(epoch != epoch_) { latency_ = .2; surfaceLatency_ = .075; epoch_ = epoch; ready_ = false; }
        if(mode != mode_) surfaceLatency_ = .075;
        const double elapsed = now - time_;
        velocity_ = glm::dvec3(0);
        // Mode changes, teleports and suspended frames must not extrapolate an
        // unrelated pose. Stationary/reversed motion takes effect immediately.
        if(ready_ && mode == mode_ && elapsed > 0 && elapsed < .5 &&
           glm::length(eye - eye_) < 4 * margin)
            velocity_ = (eye - eye_) / elapsed;
        eye_ = eye; time_ = now; mode_ = mode; ready_ = true;
    }
    void submitted(glm::dvec3 eye) { submittedEye_ = eye; }
    void published(double elapsed,glm::dvec3 eye=glm::dvec3(0),double surfaceSpeed=0) {
        if(std::isfinite(elapsed) && elapsed > 0)
            latency_ = std::clamp(std::max(.95 * latency_, 1.5 * elapsed), .03, .5);
        if(surfaceSpeed>0 && glm::length(eye)>0 && glm::length(submittedEye_)>0) {
            const auto a=glm::normalize(eye),b=glm::normalize(submittedEye_);
            const double distance=std::atan2(glm::length(glm::cross(a,b)),glm::dot(a,b)) *
                .5*(glm::length(eye)+glm::length(submittedEye_));
            // Surface controls clamp each movement step to 50 ms. Forecast in
            // observed movement time, not wall time spent in GPU preparation.
            surfaceLatency_=std::clamp(std::max(.95*surfaceLatency_,1.5*distance/surfaceSpeed),.005,.5);
        }
    }
    double horizon() const { return latency_; }
    double surfaceHorizon() const { return surfaceLatency_; }
    static bool covers(glm::dvec3 anchor,glm::dvec3 eye,double margin) {
        return glm::length(anchor-eye)<=margin;
    }
    Path predict(glm::dvec3 eye) const {
        Path path;
        for(std::size_t i = 0; i < path.size(); ++i)
            path[i] = eye + velocity_ * (latency_ * i / (path.size() - 1));
        return path;
    }
    static bool needsRefresh(const Path& path, glm::dvec3 previous, double margin) {
        if(glm::length(path.front() - previous) >= .5 * margin) return true;
        return std::any_of(path.begin(), path.end(), [&](auto p) {
            return glm::length(p - previous) >= .85 * margin;
        });
    }
    static glm::dvec3 anchor(const Path& path, double margin) {
        // The staged patch is centered at the expected publication-time eye,
        // while the installed patch covers motion during preparation. Never
        // extend placement bounds: only the center advances, by at most two
        // existing margins. Publication rechecks coverage after stops/turns.
        const auto lead = path.back() - path.front(); const double distance = glm::length(lead);
        return distance > 2 * margin && distance > 0 ? path.front() + lead * (2 * margin / distance) : path.back();
    }
private:
    glm::dvec3 eye_{0}, velocity_{0},submittedEye_{0};
    double time_ = 0, latency_ = .2,surfaceLatency_ = .075;
    std::uint64_t epoch_ = 0;
    int mode_ = -1;
    bool ready_ = false;
};
}

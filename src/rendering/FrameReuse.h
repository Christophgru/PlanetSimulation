#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

namespace rendering {
// Whole-frame reuse is deliberately exact and restricted to stopped simulation.
// Moving scenes still render every frame; sub-texel shadow reuse is separate.
class FrameReuse {
public:
    bool matches(const glm::mat4& view,float fov,int width,int height,double time,
                 const std::vector<std::uint64_t>& revisions,bool stopped,bool pending,
                 const glm::dvec3& eye=glm::dvec3(0),int cameraMode=0) const {
        if (!valid_ || !stopped || pending || fov!=fov_ || width!=width_ || height!=height_ ||
            time!=time_ || revisions!=revisions_ || eye!=eye_ || cameraMode!=cameraMode_) return false;
        for (int c=0;c<4;++c) for (int r=0;r<4;++r) if (view[c][r]!=view_[c][r]) return false;
        return true;
    }
    void remember(const glm::mat4& view,float fov,int width,int height,double time,
                  const std::vector<std::uint64_t>& revisions,
                  const glm::dvec3& eye=glm::dvec3(0),int cameraMode=0) {
        eye_=eye; cameraMode_=cameraMode;
        view_=view; fov_=fov; width_=width; height_=height; time_=time; revisions_=revisions; valid_=true;
    }
    void invalidate() { valid_=false; }
private:
    bool valid_=false;
    glm::mat4 view_{1};
    glm::dvec3 eye_{0};
    int cameraMode_=0;
    float fov_=0;
    int width_=0,height_=0;
    double time_=0;
    std::vector<std::uint64_t> revisions_;
};
} // namespace rendering

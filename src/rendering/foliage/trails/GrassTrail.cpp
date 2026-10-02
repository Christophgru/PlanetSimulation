#include "rendering/foliage/trails/GrassTrail.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <functional>
#include <cmath>
#include <limits>

namespace rendering {
namespace {
bool finite(const glm::dvec3& v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
}
void GrassTrail::observe(const glm::dvec3& point,bool grounded) {
    if (!finite(point)) throw std::invalid_argument("Grass trail requires finite positions");
    if (!grounded) { breakPath(); return; }
    if (!anchor_) { anchor_=point; return; }
    const auto delta=point-*anchor_;
    // Ignore stationary contact/remesh jitter. Relocation never joins two
    // distant placements, and airborne motion breaks continuity on landing.
    const double distance=glm::length(delta);
    if (distance>1.5) { anchor_=point; return; }
    const auto up=glm::length(point)>1e-6 ? glm::normalize(point) : glm::dvec3(0);
    if (glm::length(delta-up*glm::dot(delta,up))<.2) return;
    if (segments_.size()==capacity) segments_.erase(segments_.begin());
    segments_.push_back({*anchor_,point}); anchor_=point; ++revision_;
}
void GrassTrail::restore(const std::vector<TrailSegment>& segments) {
    if (segments.size()>capacity) throw std::invalid_argument("Grass trail replay exceeds history capacity");
    for (const auto& s:segments) if (!finite(s.start)||!finite(s.end)||glm::length(s.end-s.start)>1.500001)
        throw std::invalid_argument("Grass trail replay has invalid segment");
    segments_=segments; breakPath(); ++revision_;
}
std::vector<glm::vec4> GrassTrail::hierarchy(const glm::dvec3& origin) const {
    std::vector<glm::vec4> nodes;
    if (segments_.empty()) return nodes;
    nodes.reserve((2*segments_.size()-1)*4);
    std::vector<std::size_t> order(segments_.size());
    std::iota(order.begin(),order.end(),0);
    const std::function<void(std::size_t,std::size_t)> build=[&](std::size_t first,std::size_t last) {
        glm::dvec3 lo(std::numeric_limits<double>::max()),hi(-std::numeric_limits<double>::max());
        for (auto i=first;i<last;++i) {
            const auto& s=segments_[order[i]];
            lo=glm::min(lo,glm::min(s.start,s.end)); hi=glm::max(hi,glm::max(s.start,s.end));
        }
        const std::size_t node=nodes.size(); nodes.resize(node+4);
        nodes[node]=glm::vec4(glm::vec3(lo-origin-glm::dvec3(radius)),0);
        nodes[node+1]=glm::vec4(glm::vec3(hi-origin+glm::dvec3(radius)),last-first==1 ? 1 : 0);
        if (last-first==1) {
            const auto& s=segments_[order[first]];
            nodes[node+2]=glm::vec4(glm::vec3(s.start-origin),0);
            nodes[node+3]=glm::vec4(glm::vec3(s.end-origin),0);
        } else {
            const auto extent=hi-lo;
            const int axis=extent.x>extent.y ? (extent.x>extent.z ? 0 : 2) : (extent.y>extent.z ? 1 : 2);
            const auto mid=(first+last)/2;
            std::nth_element(order.begin()+first,order.begin()+mid,order.begin()+last,[&](auto a,auto b) {
                const auto ca=segments_[a].start[axis]+segments_[a].end[axis];
                const auto cb=segments_[b].start[axis]+segments_[b].end[axis];
                return ca==cb ? a<b : ca<cb;
            });
            build(first,mid); build(mid,last);
        }
        nodes[node].w=float(nodes.size()/4); // Escape past this whole subtree.
    };
    build(0,order.size()); return nodes;
}
}

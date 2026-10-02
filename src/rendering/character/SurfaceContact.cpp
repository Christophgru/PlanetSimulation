#include "rendering/character/SurfaceContact.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rendering {
void SurfaceContact::clear() { vertices_=nullptr; indices_=nullptr; cached_=next_=0; }
void SurfaceContact::bind(const std::vector<float>& vertices,const std::vector<unsigned>& indices,
                         std::uint64_t revision,double radiusMeters,int stride) {
    if (!std::isfinite(radiusMeters) || radiusMeters<=0 || stride<3)
        throw std::invalid_argument("Invalid surface contact mesh scale");
    if (vertices_!=&vertices || indices_!=&indices || revision_!=revision || stride_!=stride || radiusMeters_!=radiusMeters)
        cached_=next_=0;
    vertices_=&vertices; indices_=&indices; revision_=revision;
    radiusMeters_=radiusMeters; stride_=stride;
}
bool SurfaceContact::hit(unsigned triangle,const glm::dvec3& radial,GroundContact& result) const {
    const std::size_t first=std::size_t(triangle)*3;
    if (!indices_ || first+2>=indices_->size()) return false;
    glm::dvec3 p[3];
    for (int j=0;j<3;++j) {
        const std::size_t i=std::size_t((*indices_)[first+j])*stride_;
        if (i+2>=vertices_->size()) return false;
        p[j]={(*vertices_)[i],(*vertices_)[i+1],(*vertices_)[i+2]};
    }
    const auto n=glm::cross(p[1]-p[0],p[2]-p[0]);
    const double n2=glm::dot(n,n), denom=glm::dot(n,radial);
    if (n2<1e-28 || std::abs(denom)<1e-14) return false;
    const double t=glm::dot(n,p[0])/denom;
    if (t<=0) return false;
    const auto point=radial*t;
    for (int j=0;j<3;++j)
        if (glm::dot(glm::cross(p[(j+1)%3]-p[j],point-p[j]),n)<-1e-8*n2) return false;
    result={point*radiusMeters_,glm::normalize(denom>0 ? n : -n)};
    return true;
}
GroundContact SurfaceContact::sample(const glm::dvec3& direction,const GroundQuery& fallback) {
    const double length=glm::length(direction);
    if (!std::isfinite(length) || length<1e-12)
        throw std::invalid_argument("Ground query requires a finite radial direction");
    const auto radial=direction/length;
    GroundContact result;
    for (unsigned i=0;i<cached_;++i) if (hit(cache_[i],radial,result)) return result;
    if (indices_) for (std::size_t i=0;i<indices_->size()/3;++i) {
        if (!hit(i,radial,result)) continue;
        cache_[next_]=i; next_=(next_+1)%cache_.size();
        cached_=std::min<unsigned>(cached_+1,cache_.size());
        return result;
    }
    return fallback(radial);
}
}

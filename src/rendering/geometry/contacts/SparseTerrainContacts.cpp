#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace rendering {
namespace {
glm::dvec3 radial(const TerrainInputSample& s) { return {s.radial[0],s.radial[1],s.radial[2]}; }
// Two float position conversions plus the contact barycentric edge tolerance.
// A positive radial scale preserves a triangle's cone; expanded endpoint bounds
// include float direction rounding. Queries intersect the cone's unit triangle.
constexpr double directionPadding=1e-6;
}
SparseTerrainContacts::SparseTerrainContacts(PlanetField field,TerrainTopology topology)
    :field_(std::move(field)),topology_(std::move(topology)),generation_(topology_.generation) {
    const auto start=std::chrono::steady_clock::now();
    topology_.validate();
    if(generation_.field!=field_.fingerprint()) throw std::invalid_argument("Stale contact planet field");
    if(topology_.indices.empty()) throw std::invalid_argument("Empty contact topology");
    double sink=0;
    for(const auto& s:topology_.samples) sink=std::max(sink,s.sinkMeters);
    const double minimumRadius=radiusMeters()-field_.maximumReliefMeters()-sink;
    if(!std::isfinite(minimumRadius) || minimumRadius<=radiusMeters()*1e-6)
        throw std::invalid_argument("Contact topology needs a positive radial surface");
    generation_.backend=TerrainBackend::Compute;
    order_.resize(topology_.indices.size()/3);std::iota(order_.begin(),order_.end(),0);
    build(0,order_.size());
    positions_.reserve(positionCapacity);
    stats_.topologyBytes=topology_.topologyInputBytes;
    stats_.indexBytes=nodes_.size()*sizeof(Node)+order_.size()*sizeof(unsigned);
    stats_.buildMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
unsigned SparseTerrainContacts::build(unsigned first,unsigned count) {
    const unsigned id=nodes_.size();nodes_.emplace_back();
    Node node;node.first=first;node.count=count;
    node.low=glm::dvec3(std::numeric_limits<double>::infinity());node.high=-node.low;
    for(unsigned i=first;i<first+count;++i) for(int corner=0;corner<3;++corner) {
        const auto p=radial(topology_.samples[topology_.indices[3*order_[i]+corner]]);
        node.low=glm::min(node.low,p);node.high=glm::max(node.high,p);
    }
    node.low-=directionPadding;node.high+=directionPadding;
    if(count>8) {
        const auto extent=node.high-node.low;
        int axis=extent.y>extent.x ? 1 : 0;if(extent.z>extent[axis]) axis=2;
        const auto center=[&](unsigned triangle) {
            double sum=0;for(int j=0;j<3;++j) sum+=topology_.samples[topology_.indices[3*triangle+j]].radial[axis];
            return sum;
        };
        const unsigned middle=first+count/2;
        std::nth_element(order_.begin()+first,order_.begin()+middle,order_.begin()+first+count,
            [&](unsigned a,unsigned b) {const auto x=center(a),y=center(b);return x==y ? a<b : x<y;});
        node.count=0;node.left=build(first,count/2);node.right=build(middle,count-count/2);
    }
    nodes_[id]=node;return id;
}
bool SparseTerrainContacts::intersects(const Node& node,const glm::dvec3& direction) const {
    double near=0,far=std::numeric_limits<double>::infinity();
    for(int axis=0;axis<3;++axis) {
        if(std::abs(direction[axis])<1e-30) {
            if(node.low[axis]>0 || node.high[axis]<0) return false;
        } else {
            double a=node.low[axis]/direction[axis],b=node.high[axis]/direction[axis];
            if(a>b) std::swap(a,b);
            near=std::max(near,a);far=std::min(far,b);
            if(near>far) return false;
        }
    }
    return true;
}
std::vector<unsigned> SparseTerrainContacts::candidates(const glm::dvec3& direction) {
    const double length2=glm::dot(direction,direction);
    if(!std::isfinite(length2) || std::abs(length2-1)>1e-12)
        throw std::invalid_argument("Contact index requires a unit radial");
    ++stats_.queries;
    std::vector<unsigned> result,stack{0};
    while(!stack.empty()) {
        const auto id=stack.back();stack.pop_back();++stats_.nodesVisited;
        const auto& node=nodes_[id];if(!intersects(node,direction)) continue;
        if(node.count) for(unsigned i=node.first;i<node.first+node.count;++i) result.push_back(order_[i]);
        else {stack.push_back(node.right);stack.push_back(node.left);}
    }
    // Preserve the full mesh's first-triangle tie rule at shared edges/poles.
    std::sort(result.begin(),result.end());stats_.candidateTriangles+=result.size();return result;
}
glm::vec3 SparseTerrainContacts::position(unsigned id) {
    if(const auto it=positions_.find(id);it!=positions_.end()) return it->second;
    const auto& s=topology_.samples.at(id);const auto r=radial(s);
    const auto height=field_.heightAt(r,topology_.surfacePolicy);++stats_.heightEvaluations;
    const auto shaped=r*(1.0+height/field_.radiusWorld());
    // Materialize the first conversion. GCC can otherwise keep two SIMD lanes
    // at excess double precision across a float-to-double round trip here.
    volatile float rounded[3];
    for(int j=0;j<3;++j) rounded[j]=static_cast<float>(shaped[j]);
    std::array<float,3> packed{rounded[0],rounded[1],rounded[2]};
    if(s.sinkMeters!=0) {
        const glm::dvec3 converted(packed[0],packed[1],packed[2]);
        const auto sunk=converted-glm::normalize(converted)*(s.sinkMeters/radiusMeters());
        for(int j=0;j<3;++j) packed[j]=static_cast<float>(sunk[j]);
    }
    const glm::vec3 p(packed[0],packed[1],packed[2]);
    if(positions_.size()==positionCapacity) positions_.clear();
    positions_.emplace(id,p);return p;
}
std::array<glm::dvec3,3> SparseTerrainContacts::triangle(unsigned id) {
    if(id>=topology_.indices.size()/3) throw std::out_of_range("Contact triangle outside topology");
    std::array<glm::dvec3,3> result;
    for(int j=0;j<3;++j) result[j]=position(topology_.indices[3*id+j]);
    return result;
}
SparseContactStats SparseTerrainContacts::stats() const {
    auto result=stats_;result.residentPositions=positions_.size();return result;
}
}

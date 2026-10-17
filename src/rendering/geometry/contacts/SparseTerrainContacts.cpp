#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "rendering/geometry/terrain/detail/TerrainMorph.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
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
    CpuTrace::Scope scope("terrain.contact_index");
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
    constexpr unsigned absent=std::numeric_limits<unsigned>::max();
    const auto& parent=topology_.contactParent_;
    if(parent && topology_.contactTopology_==topology_.generation.topology &&
       topology_.contactParentFaces_.size()==std::size_t(topology_.triangleCount()) &&
       parent->generation_.field==generation_.field && parent->topology_.surfacePolicy==topology_.parentPolicy) {
        // Longest-edge children remain inside their original triangle's cone.
        // Query the immutable coarse tree for untouched faces; index only the
        // split faces locally. All IDs map back to this plan's triangle order.
        parent_=parent;
        parentToCurrent_.assign(parent_->topology_.triangleCount(),absent);
        std::vector<unsigned> counts(parentToCurrent_.size(),0);
        for(auto root:topology_.contactParentFaces_) ++counts.at(root);
        for(unsigned face=0;face<topology_.triangleCount();++face) {
            const auto root=topology_.contactParentFaces_[face];
            if(counts[root]==1) parentToCurrent_[root]=face;
            else order_.push_back(face);
        }
        stats_.parentTriangles=parentToCurrent_.size();
        const auto shared=parent_->stats();
        stats_.sharedParentBytes=shared.topologyBytes+shared.indexBytes;
    } else {
        order_.resize(topology_.triangleCount());std::iota(order_.begin(),order_.end(),0);
    }
    // The derived face map is needed only while constructing the local index.
    // Public canonicalization or a changed sealed hash forces the full path.
    topology_.contactParentFaces_.clear();topology_.contactParentFaces_.shrink_to_fit();
    topology_.contactParent_.reset();topology_.contactTopology_=0;
    stats_.localTriangles=order_.size();
    if(!order_.empty()) build(0,order_.size());
    positions_.reserve(positionCapacity);
    stats_.topologyBytes=topology_.topologyInputBytes+(parent_?parent_->stats().topologyBytes:0);
    stats_.indexBytes=nodes_.size()*sizeof(Node)+order_.size()*sizeof(unsigned)+
        parentToCurrent_.size()*sizeof(unsigned)+(parent_?parent_->stats().indexBytes:0);
    stats_.buildMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
TerrainTopologySnapshot SparseTerrainContacts::topologySnapshot(const std::shared_ptr<SparseTerrainContacts>& owner) {
    if(!owner) return {};
    return TerrainTopologySnapshot(std::shared_ptr<const TerrainTopology>(owner,&owner->topology_));
}
unsigned SparseTerrainContacts::build(unsigned first,unsigned count) {
    // Only build-local scratch: repeated median comparisons used to fetch all
    // three indirect vertices again. Cache exact bounds and unscaled centroid
    // sums once, preserving the original split axis and triangle-ID tie rule.
    // The 72 bytes/triangle are released before the contact index is published;
    // no field evaluation or additional resident generation is introduced.
    struct Bounds {glm::dvec3 low,high,center;};
    std::vector<Bounds> bounds(topology_.triangleCount());
    for(unsigned i=first;i<first+count;++i) {
        auto& b=bounds[order_[i]];
        b.low=glm::dvec3(std::numeric_limits<double>::infinity());b.high=-b.low;b.center=glm::dvec3(0);
        for(int corner=0;corner<3;++corner) {
            const auto p=radial(topology_.samples[topology_.indices[3*order_[i]+corner]]);
            b.low=glm::min(b.low,p);b.high=glm::max(b.high,p);b.center+=p;
        }
    }
    const auto partition=[&](const auto& self,unsigned begin,unsigned size) -> unsigned {
        const unsigned id=nodes_.size();nodes_.emplace_back();
        Node node;node.first=begin;node.count=size;
        node.low=glm::dvec3(std::numeric_limits<double>::infinity());node.high=-node.low;
        for(unsigned i=begin;i<begin+size;++i) {
            const auto& b=bounds[order_[i]];
            node.low=glm::min(node.low,b.low);node.high=glm::max(node.high,b.high);
        }
        node.low-=directionPadding;node.high+=directionPadding;
        if(size>8) {
            const auto extent=node.high-node.low;
            int axis=extent.y>extent.x ? 1 : 0;if(extent.z>extent[axis]) axis=2;
            const unsigned middle=begin+size/2;
            std::nth_element(order_.begin()+begin,order_.begin()+middle,order_.begin()+begin+size,
                [&](unsigned a,unsigned b) {
                    const auto x=bounds[a].center[axis],y=bounds[b].center[axis];
                    return x==y ? a<b : x<y;
                });
            node.count=0;node.left=self(self,begin,size/2);node.right=self(self,middle,size-size/2);
        }
        nodes_[id]=node;return id;
    };
    return partition(partition,first,count);
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
    std::vector<unsigned> result;
    collectCandidates(direction,result,stats_);
    // Preserve the full mesh's first-triangle tie rule at shared edges/poles.
    std::sort(result.begin(),result.end());stats_.candidateTriangles+=result.size();return result;
}
void SparseTerrainContacts::collectCandidates(const glm::dvec3& direction,std::vector<unsigned>& result,
    SparseContactStats& stats) const {
    if(parent_) {
        std::vector<unsigned> coarse;
        // This traversal never mutates shared parent counters/position caches;
        // worker snapshots and installed contact generations can query safely.
        parent_->collectCandidates(direction,coarse,stats);
        for(auto root:coarse) if(const auto face=parentToCurrent_[root];face!=std::numeric_limits<unsigned>::max())
            result.push_back(face);
    }
    if(nodes_.empty()) return;
    std::vector<unsigned> stack{0};
    while(!stack.empty()) {
        const auto id=stack.back();stack.pop_back();++stats.nodesVisited;
        const auto& node=nodes_[id];if(!intersects(node,direction)) continue;
        if(node.count) for(unsigned i=node.first;i<node.first+node.count;++i) result.push_back(order_[i]);
        else {stack.push_back(node.right);stack.push_back(node.left);}
    }
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
    const glm::vec3 p=topology_.morph.empty()?glm::vec3(packed[0],packed[1],packed[2]):
        terrainMorphPosition(field_,s,topology_.surfacePolicy,topology_.morph[id],topology_.parentPolicy);
    if(!topology_.morph.empty()) {
        const auto& m=topology_.morph[id];
        stats_.heightEvaluations+=m.blend[0]==0?1:
            (m.parent[0]==m.parent[1] && m.parent[0]==m.parent[2]?2:4);
    }
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

#include "rendering/geometry/Terrain.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <queue>
#include <stdexcept>

namespace rendering {
void TerrainSurface::refineSurfaceError(TerrainTopology& geometry, const glm::dvec3& eyeBody) const {
    CpuTrace::Scope scope("TerrainSurface::refineSurfaceError");
    // Canonical radial IDs, not rounded render positions, join both sides of
    // every edge. All error probes use the same filtered/sunk field as drawing.
    geometry.canonicalize(field_.fingerprint());
    const int originalCount=geometry.triangleCount();
    const double scale=radius_*metersPerUnit_;
    TerrainQueryCache queries(field_,8192,geometry.surfacePolicy);
    std::vector<GridSample> vertices;
    vertices.reserve(2*lod_.max_triangle_budget+2);
    const auto sampleAt=[&](const glm::dvec3& radial) {
        auto sample=makeGridSample(radial,queries.heightAt(radial));
        const auto profile=geometry.surfacePolicy.profile(radial);
        const double curvature=std::lerp(profile.x*profile.x,profile.y*profile.y,profile.z)/(8*scale);
        sample.sinkMeters=static_cast<float>(std::min(geometry.surfacePolicy.distances[3],
            field_.omittedReliefMeters(profile)+curvature));
        sample.position-=radial*(sample.sinkMeters/scale);
        return sample;
    };
    for(const auto& s:geometry.samples) vertices.push_back(sampleAt({s.radial[0],s.radial[1],s.radial[2]}));

    using Key=std::pair<std::uint32_t,std::uint32_t>;
    const auto key=[](std::uint32_t a,std::uint32_t b) {return Key{std::min(a,b),std::max(a,b)};};
    constexpr auto absent=std::numeric_limits<std::uint32_t>::max();
    struct Edge {std::array<int,2> faces{-1,-1};std::uint32_t middle=absent;};
    struct Triangle {std::array<std::uint32_t,3> v;double score=0;bool alive=true;};
    struct EdgeHash {
        std::size_t operator()(const Key& k) const {
            return std::hash<std::uint64_t>{}((std::uint64_t(k.first)<<32)|k.second);
        }
    };
    // Only keyed lookup is needed: face/queue order determines every split.
    // A closed triangular shell has 3/2 as many edges as faces. Reserve that
    // bounded capacity to avoid tree walks and growth during refinement.
    std::unordered_map<Key,Edge,EdgeHash> edges;
    edges.reserve(std::size_t(lod_.max_triangle_budget)*3/2);
    std::vector<Triangle> triangles;
    triangles.reserve(2*lod_.max_triangle_budget);
    const auto attach=[&](const std::array<std::uint32_t,3>& v) {
        const int id=static_cast<int>(triangles.size());
        triangles.push_back({v});
        for(int side=0;side<3;++side) {
            auto& e=edges[key(v[side],v[(side+1)%3])];
            if(e.faces[0]<0) e.faces[0]=id;
            else if(e.faces[1]<0) e.faces[1]=id;
            else throw std::logic_error("Non-manifold terrain refinement edge");
        }
        return id;
    };
    for(std::size_t t=0;t<geometry.indices.size();t+=3)
        attach({geometry.indices[t],geometry.indices[t+1],geometry.indices[t+2]});

    struct Candidate {double score;int face;};
    const auto lowerPriority=[](const Candidate& a,const Candidate& b) {
        return a.score==b.score ? a.face>b.face : a.score<b.score;
    };
    std::priority_queue<Candidate,std::vector<Candidate>,decltype(lowerPriority)> pending(lowerPriority);
    const auto midpoint=[&](const Key& k) {
        auto& e=edges.at(k);
        if(e.middle==absent) {
            const auto radial=glm::normalize(vertices[k.first].radial+vertices[k.second].radial);
            e.middle=static_cast<std::uint32_t>(vertices.size());
            vertices.push_back(sampleAt(radial));
        }
        return e.middle;
    };
    const auto assess=[&](int id) {
        auto& triangle=triangles[id];
        const auto v=triangle.v;
        const glm::dvec3 a=vertices[v[0]].position,b=vertices[v[1]].position,c=vertices[v[2]].position;
        const auto center=(a+b+c)/3.0;
        const double reach=std::max({glm::length(a-center),glm::length(b-center),glm::length(c-center)});
        const double distance=std::max(0.0,glm::length(center-eyeBody)-reach)*scale;
        if(distance>lod_.mid_surface_distance_m) return;
        // A fixed near error target blends into an angular error target with
        // distance. A steep planar slope has little error; a crest has much more.
        const double tolerance=std::max(lod_.geometric_error_m,0.001*distance);
        double error=glm::length(sampleAt(glm::normalize(center)).position-center)*scale;
        double longest=0;
        for(int side=0;side<3;++side) {
            const auto x=v[side],y=v[(side+1)%3];
            const auto mid=midpoint(key(x,y));
            error=std::max(error,glm::length(vertices[mid].position-
                (vertices[x].position+vertices[y].position)*0.5)*scale);
            longest=std::max(longest,glm::length(vertices[x].radial-vertices[y].radial)*scale);
        }
        triangle.score=error/tolerance;
        // The minimum split edge is 4 cm, so its two halves stay >=2 cm.
        if(triangle.score>1 && longest>=0.04) pending.push({triangle.score,id});
    };
    for(int id=0;id<originalCount;++id) assess(id);
    const auto edgeLength2=[&](const Key& k) {
        const auto delta=vertices[k.first].radial-vertices[k.second].radial;
        return glm::dot(delta,delta);
    };
    int liveCount=originalCount;
    while(!pending.empty()) {
        const int id=pending.top().face;pending.pop();
        if(!triangles[id].alive) continue;
        const auto v=triangles[id].v;
        Key split=key(v[0],v[1]);
        double longest=-1;
        for(int side=0;side<3;++side) {
            const auto k=key(v[side],v[(side+1)%3]);
            // Use the footprint, not relief height: on a cliff a long vertical
            // edge can have a vanishing footprint and create sliver triangles.
            const double length2=edgeLength2(k);
            if(length2>longest) {longest=length2;split=k;}
        }
        const auto requested=split;
        // Propagate through longer neighboring edges until this is a longest
        // edge on BOTH sides. Blindly splitting a neighbor's shortest edge
        // repeatedly collapses its angles, eventually wasting the whole budget
        // on triangles thinner than render precision. Length strictly increases
        // along this walk, so it terminates without recursion or a depth cap.
        for(;;) {
            auto next=split;
            for(int face:edges.at(split).faces) {
                if(face<0) throw std::logic_error("Open terrain refinement edge");
                const auto adjacent=triangles[face].v;
                for(int side=0;side<3;++side) {
                    const auto k=key(adjacent[side],adjacent[(side+1)%3]);
                    const double length2=edgeLength2(k);
                    if(length2>longest) {longest=length2;next=k;}
                }
            }
            if(next==split) break;
            split=next;
        }
        const auto neighbors=edges.at(split).faces;
        if(neighbors[0]<0 || neighbors[1]<0) throw std::logic_error("Open terrain refinement edge");
        // Splitting both incident triangles costs exactly two new triangles.
        if(liveCount+2>lod_.max_triangle_budget) {geometry.errorBudgetLimited=true;break;}
        const auto middle=midpoint(split);
        for(int face:neighbors) {
            triangles[face].alive=false;
            const auto old=triangles[face].v;
            for(int side=0;side<3;++side) {
                auto& e=edges.at(key(old[side],old[(side+1)%3]));
                for(auto& f:e.faces) if(f==face) f=-1;
            }
        }
        std::array<int,4> children{};int child=0;
        for(int face:neighbors) {
            const auto old=triangles[face].v;
            for(int side=0;side<3;++side) if(key(old[side],old[(side+1)%3])==split) {
                children[child++]=attach({old[side],middle,old[(side+2)%3]});
                children[child++]=attach({middle,old[(side+1)%3],old[(side+2)%3]});
                break;
            }
        }
        edges.erase(split);
        liveCount+=2;
        for(int face:children) assess(face);
        if(split!=requested && triangles[id].alive) pending.push({triangles[id].score,id});
    }
    geometry.indices.clear();geometry.indices.reserve(3*liveCount);
    geometry.remainingErrorRatio=0;
    for(const auto& t:triangles) if(t.alive) {
        geometry.indices.insert(geometry.indices.end(),t.v.begin(),t.v.end());
        geometry.remainingErrorRatio=std::max(geometry.remainingErrorRatio,t.score);
    }
    geometry.samples.clear();geometry.samples.reserve(vertices.size());
    for(const auto& v:vertices) geometry.samples.push_back({{v.radial.x,v.radial.y,v.radial.z},v.sinkMeters});
    geometry.errorRefinedTriangles=liveCount-originalCount;
    geometry.planningQueries.requests+=queries.stats().requests;
    geometry.planningQueries.evaluations+=queries.stats().evaluations;
    geometry.planningQueries.hits+=queries.stats().hits;
    // At most 2*budget triangle records and 2*budget+2 vertex/probe records
    // are created. The caller's final canonicalization removes unused probes.
}
} // namespace rendering

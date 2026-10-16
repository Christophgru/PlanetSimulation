#include "rendering/geometry/Terrain.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <queue>
#include <stdexcept>

namespace rendering {
void TerrainSurface::refineSurfaceError(TerrainTopology& geometry, const glm::dvec3& eyeBody, bool localOnly, TerrainQueryCache* sharedQueries) const {
    CpuTrace::Scope scope("TerrainSurface::refineSurfaceError");
    // Canonical radial IDs, not rounded render positions, join both sides of
    // every edge. All error probes use the same filtered/sunk field as drawing.
    if(!localOnly) geometry.canonicalize(field_.fingerprint());
    const int originalCount=geometry.triangleCount();
    const double scale=radius_*metersPerUnit_;
    TerrainQueryCache localQueries(field_,8192,geometry.surfacePolicy);
    auto& queries=sharedQueries?*sharedQueries:localQueries;
    const auto initialQueries=queries.stats();
    std::vector<GridSample> vertices;
    vertices.reserve(2*lod_.max_triangle_budget+2);
    const auto sampleAt=[&](const glm::dvec3& radial) {
        auto sample=makeGridSample(radial,queries.heightAt(radial));
        const auto profile=geometry.surfacePolicy.profile(radial);
        const double curvature=geometry.surfacePolicy.curvatureMeters(profile);
        sample.sinkMeters=profile.x==geometry.surfacePolicy.spacing[7] && profile.y==geometry.surfacePolicy.spacing[7] ? 0 :
            static_cast<float>(std::min(geometry.surfacePolicy.distances[3],field_.omittedReliefMeters(profile)+curvature));
        sample.position-=radial*(sample.sinkMeters/scale);
        return sample;
    };
    std::vector<bool> evaluated;
    for(const auto& s:geometry.samples) {
        const glm::dvec3 radial(s.radial[0],s.radial[1],s.radial[2]);
        vertices.push_back(localOnly?GridSample{radial,radial,0,s.sinkMeters}:sampleAt(radial));
        evaluated.push_back(!localOnly);
    }
    std::vector<TerrainMorphSample> morph(localOnly?vertices.size():0);
    if(localOnly) for(std::size_t i=0;i<morph.size();++i) morph[i].parent.fill(geometry.samples[i]);

    using Key=std::pair<std::uint32_t,std::uint32_t>;
    const auto key=[](std::uint32_t a,std::uint32_t b) {return Key{std::min(a,b),std::max(a,b)};};
    constexpr auto absent=std::numeric_limits<std::uint32_t>::max();
    struct Edge {std::array<int,2> faces{-1,-1};std::uint32_t middle=absent;};
    struct Triangle {
        std::array<std::uint32_t,3> v;
        double score=0,errorScore=0,longestPhysical=0;
        double localDistance=std::numeric_limits<double>::infinity();
        int priority=0;
        bool alive=true;
    };
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
    std::vector<std::array<TerrainInputSample,3>> triangleParents;
    if(localOnly) triangleParents.reserve(2*lod_.max_triangle_budget);
    const auto attach=[&](const std::array<std::uint32_t,3>& v, const std::array<TerrainInputSample,3>* parent=nullptr) {
        const int id=static_cast<int>(triangles.size());
        triangles.push_back({v});
        if(localOnly) {
            triangleParents.emplace_back();
            if(parent) triangleParents.back()=*parent;
            else for(int j=0;j<3;++j) triangleParents.back()[j]=geometry.samples.at(v[j]);
        }
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

    struct Candidate {double score;int face,priority;};
    const auto lowerPriority=[](const Candidate& a,const Candidate& b) {
        if(a.priority!=b.priority) return a.priority<b.priority;
        return a.score==b.score ? a.face>b.face : a.score<b.score;
    };
    std::priority_queue<Candidate,std::vector<Candidate>,decltype(lowerPriority)> pending(lowerPriority);
    const auto midpoint=[&](const Key& k) {
        auto& e=edges.at(k);
        if(e.middle==absent) {
            const auto radial=glm::normalize(vertices[k.first].radial+vertices[k.second].radial);
            e.middle=static_cast<std::uint32_t>(vertices.size());
            vertices.push_back(sampleAt(radial));evaluated.push_back(true);
            if(localOnly) {
                TerrainMorphSample m;
                m.parent=triangleParents.at(e.faces[0]>=0?e.faces[0]:e.faces[1]);
                morph.push_back(m);
            }
        }
        return e.middle;
    };
    const auto assess=[&](int id) {
        auto& triangle=triangles[id];
        const auto v=triangle.v;
        if(localOnly) {
            const auto center=glm::normalize(vertices[v[0]].radial+vertices[v[1]].radial+vertices[v[2]].radial);
            double reach=0;
            for(auto corner:v) reach=std::max(reach,glm::length(vertices[corner].radial-center)*scale);
            const double distance=std::max(0.0,glm::length(center-glm::normalize(eyeBody))*scale-reach);
            if(distance>lod_.local_detail_radius_m+lod_.local_transition_m) return;
            for(auto corner:v) if(!evaluated[corner]) {
                vertices[corner]=sampleAt(vertices[corner].radial);evaluated[corner]=true;
            }
        }
        const glm::dvec3 a=vertices[v[0]].position,b=vertices[v[1]].position,c=vertices[v[2]].position;
        const auto center=(a+b+c)/3.0;
        const double reach=std::max({glm::length(a-center),glm::length(b-center),glm::length(c-center)});
        const double distance=std::max(0.0,glm::length(center-eyeBody)-reach)*scale;
        if(distance>lod_.mid_surface_distance_m) return;
        // A fixed near error target blends into an angular error target with
        // distance. A steep planar slope has little error; a crest has much more.
        double tolerance=std::max(lod_.geometric_error_m,0.001*distance),edgeTarget=0;
        if(lod_.local_detail_radius_m>0) {
            const auto radialCenter=glm::normalize(vertices[v[0]].radial+vertices[v[1]].radial+vertices[v[2]].radial);
            double radialReach=0;
            for(auto corner:v) radialReach=std::max(radialReach,glm::length(vertices[corner].radial-radialCenter)*scale);
            // Ground-centered footprint, independent of clearance or slope.
            triangle.localDistance=std::max(0.0,glm::length(radialCenter-glm::normalize(eyeBody))*scale-radialReach);
            const double beyond=std::max(0.0,triangle.localDistance-lod_.local_detail_radius_m);
            if(beyond<=lod_.local_transition_m) {
                triangle.priority=beyond==0 ? 2 : 1;
                const double blend=beyond/lod_.local_transition_m;
                tolerance=std::lerp(lod_.local_error_m,tolerance,blend);
                edgeTarget=lod_.local_edge_m*(1+3*blend)*(1+3*blend);
            }
        }
        double error=glm::length(sampleAt(glm::normalize(center)).position-center)*scale;
        double longest=0;
        for(int side=0;side<3;++side) {
            const auto x=v[side],y=v[(side+1)%3];
            const auto mid=midpoint(key(x,y));
            error=std::max(error,glm::length(vertices[mid].position-
                (vertices[x].position+vertices[y].position)*0.5)*scale);
            longest=std::max(longest,glm::length(vertices[x].radial-vertices[y].radial)*scale);
            triangle.longestPhysical=std::max(triangle.longestPhysical,glm::length(vertices[x].position-vertices[y].position)*scale);
        }
        triangle.errorScore=error/tolerance;
        triangle.score=std::max(triangle.errorScore,edgeTarget>0?triangle.longestPhysical/edgeTarget:0);
        // Preserve the legacy 4 cm floor outside the opt-in detail footprint.
        if(triangle.score>1 && longest>=(triangle.priority ? .0001 : .04))
            pending.push({triangle.score,id,triangle.priority});
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
                children[child++]=attach({old[side],middle,old[(side+2)%3]},localOnly?&triangleParents[face]:nullptr);
                children[child++]=attach({middle,old[(side+1)%3],old[(side+2)%3]},localOnly?&triangleParents[face]:nullptr);
                break;
            }
        }
        edges.erase(split);
        liveCount+=2;
        for(int face:children) assess(face);
        if(split!=requested && triangles[id].alive) pending.push({triangles[id].score,id,triangles[id].priority});
    }
    geometry.indices.clear();geometry.indices.reserve(3*liveCount);
    geometry.remainingErrorRatio=0;
    for(const auto& t:triangles) if(t.alive) {
        geometry.indices.insert(geometry.indices.end(),t.v.begin(),t.v.end());
        geometry.remainingErrorRatio=std::max(geometry.remainingErrorRatio,t.errorScore);
        if(lod_.local_detail_radius_m>0 && t.localDistance<=lod_.local_detail_radius_m) {
            ++geometry.localDetailTriangles;
            geometry.localMaxEdgeMeters=std::max(geometry.localMaxEdgeMeters,t.longestPhysical);
            geometry.localRemainingErrorRatio=std::max(geometry.localRemainingErrorRatio,t.errorScore);
        }
    }
    geometry.localDetailLimited=geometry.localDetailTriangles>0 &&
        (geometry.localMaxEdgeMeters>lod_.local_edge_m || geometry.localRemainingErrorRatio>1);
    geometry.samples.clear();geometry.samples.reserve(vertices.size());
    for(const auto& v:vertices) geometry.samples.push_back({{v.radial.x,v.radial.y,v.radial.z},v.sinkMeters});
    if(localOnly) {
        geometry.morph=std::move(morph);
        for(std::size_t i=0;i<geometry.samples.size();++i) {
            auto& s=geometry.samples[i];
            const glm::dvec3 r(s.radial[0],s.radial[1],s.radial[2]);
            const auto profile=geometry.surfacePolicy.profile(r);
            s.sinkMeters=profile.x==geometry.surfacePolicy.spacing[7] && profile.y==geometry.surfacePolicy.spacing[7] ? 0 :
                static_cast<float>(std::min(geometry.surfacePolicy.distances[3],field_.omittedReliefMeters(profile)+geometry.surfacePolicy.curvatureMeters(profile)));
            const double distance=glm::length(r-glm::normalize(eyeBody))*scale;
            double blend=std::clamp((distance-lod_.local_detail_radius_m)/lod_.local_transition_m,0.0,1.0);
            geometry.morph[i].blend[0]=blend*blend*(3-2*blend);
        }
    }
    geometry.errorRefinedTriangles=liveCount-originalCount;
    geometry.planningQueries.requests+=queries.stats().requests-initialQueries.requests;
    geometry.planningQueries.evaluations+=queries.stats().evaluations-initialQueries.evaluations;
    geometry.planningQueries.hits+=queries.stats().hits-initialQueries.hits;
    // At most 2*budget triangle records and 2*budget+2 vertex/probe records
    // are created. The caller's final canonicalization removes unused probes.
}
} // namespace rendering

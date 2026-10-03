#include "rendering/geometry/Terrain.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace rendering {
void TerrainSurface::refineShoreline(TerrainTopology& geometry, const glm::dvec3& eyeBody, TerrainQueryCache& queries) const {
    CpuTrace::Scope scope("TerrainSurface::refineShoreline");
    const double scale = radius_*metersPerUnit_;
    const int originalCount = geometry.triangleCount();
    // Shared edge decisions split both neighbors, including the coarse side
    // of the refinement boundary. Every selected edge adds exactly one
    // triangle per incident face, so the hard budget is known before emitting.
    for (int pass=0;pass<8;++pass) {
        if (geometry.triangleCount()+2>lod_.max_triangle_budget) break;
        struct Edge {
            glm::dvec3 a, b;
            GridSample middle;
            int uses=0;
            double distance=0;
            double sinkMeters=0;
            bool wanted=false, split=false;
        };
        std::map<EdgeKey,Edge> edges;
        auto sample = [&](unsigned index) {
            const auto& s=geometry.samples.at(index);
            const glm::dvec3 radial(s.radial[0],s.radial[1],s.radial[2]);
            // Legacy classification uses float-rounded unsunk positions.
            const glm::dvec3 p(geometry.planningPositions.at(index));
            return GridSample{radial,p,(glm::length(p)-1)*radius_,s.sinkMeters};
        };
        for (std::size_t t=0;t<geometry.indices.size();t+=3) {
            const std::array<GridSample,3> p{sample(geometry.indices[t]),
                sample(geometry.indices[t+1]),sample(geometry.indices[t+2])};
            const auto center=(p[0].position+p[1].position+p[2].position)/3.0;
            const double reach=std::max({glm::length(p[0].position-center),
                glm::length(p[1].position-center),glm::length(p[2].position-center)});
            const bool nearby=(glm::length(center-eyeBody)-reach)*scale < lod_.shoreline_distance_m;
            double low=std::min({p[0].height,p[1].height,p[2].height})*metersPerUnit_;
            double high=std::max({p[0].height,p[1].height,p[2].height})*metersPerUnit_;
            if (nearby) {
                const double middleHeight=queries.heightAt(glm::normalize(center))*metersPerUnit_;
                low=std::min(low,middleHeight); high=std::max(high,middleHeight);
            }
            const bool shore=nearby && low<=*waterLevelMeters_+2.0 && high>=*waterLevelMeters_-2.0;
            for (int side=0;side<3;++side) {
                const auto a=p[side].position, b=p[(side+1)%3].position;
                auto& edge=edges[edgeKey(a,b)];
                edge.a=a; edge.b=b; ++edge.uses;
                edge.sinkMeters=0.5*(p[side].sinkMeters+p[(side+1)%3].sinkMeters);
                if (shore && glm::length(a-b)*scale>lod_.shoreline_edge_m) {
                    edge.wanted=true;
                    const double fraction=std::clamp(glm::dot(eyeBody-a,b-a)/glm::dot(b-a,b-a),0.0,1.0);
                    edge.distance=glm::length(a+fraction*(b-a)-eyeBody);
                }
            }
        }
        std::vector<Edge*> candidates;
        for (auto& [key,edge]:edges) if (edge.wanted) candidates.push_back(&edge);
        std::stable_sort(candidates.begin(),candidates.end(),[](const Edge* a,const Edge* b) {
            return a->distance<b->distance;
        });
        int spare=lod_.max_triangle_budget-geometry.triangleCount();
        int added=0;
        for (auto* edge:candidates) {
            if (edge->uses>spare) continue;
            edge->split=true; spare-=edge->uses; added+=edge->uses;
            const auto radial=glm::normalize(edge->a+edge->b);
            edge->middle=makeGridSample(radial,queries.heightAt(radial));
            edge->middle.sinkMeters=edge->sinkMeters;
        }
        if (added==0) break;
        TerrainTopology refined;
        refined.samples.reserve(std::size_t(geometry.triangleCount()+added)*3);
        refined.indices.reserve(std::size_t(geometry.triangleCount()+added)*3);
        for (std::size_t t=0;t<geometry.indices.size();t+=3) {
            const std::array<GridSample,3> p{sample(geometry.indices[t]),
                sample(geometry.indices[t+1]),sample(geometry.indices[t+2])};
            std::array<const Edge*,3> e;
            int count=0,splitSide=0,unsplitSide=0;
            for (int side=0;side<3;++side) {
                e[side]=&edges.at(edgeKey(p[side].position,p[(side+1)%3].position));
                if (e[side]->split) { ++count; splitSide=side; }
                else unsplitSide=side;
            }
            if (count==0) emitFace(p[0],p[1],p[2],refined);
            else if (count==1) {
                const int a=splitSide,b=(a+1)%3,c=(a+2)%3;
                emitFace(p[a],e[a]->middle,p[c],refined);
                emitFace(e[a]->middle,p[b],p[c],refined);
            } else if (count==2) {
                const int a=(unsplitSide+1)%3,b=(a+1)%3,c=(a+2)%3;
                emitFace(p[b],e[b]->middle,e[a]->middle,refined);
                emitFace(p[a],e[a]->middle,p[c],refined);
                emitFace(e[a]->middle,e[b]->middle,p[c],refined);
            } else {
                for (int side=0;side<3;++side)
                    emitFace(p[side],e[side]->middle,e[(side+2)%3]->middle,refined);
                emitFace(e[0]->middle,e[1]->middle,e[2]->middle,refined);
            }
        }
        geometry.samples=std::move(refined.samples);
        geometry.planningPositions=std::move(refined.planningPositions);
        geometry.indices=std::move(refined.indices);
    }
    geometry.shorelineAddedTriangles=geometry.triangleCount()-originalCount;
}
} // namespace rendering

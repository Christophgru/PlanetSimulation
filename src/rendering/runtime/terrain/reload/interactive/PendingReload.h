#pragma once
#include "rendering/runtime/terrain/reload/SceneTerrainReplacement.h"
#include "rendering/runtime/terrain/reload/ReloadTracking.h"
#include "app/CommandLineOptions.h"
namespace rendering {
// One requested snapshot, then one transaction. Never retain full CPU topology
// outputs for all bodies; completed sparse contacts reside in off-live meshes.
struct PendingSceneReload {
    app::CommandLineOptions options;
    nlohmann::json document,replay;
    std::optional<app::PreparedScene> prepared;
    std::unique_ptr<SceneTerrainReplacement> transaction;
    ReloadTracking tracking;
    std::vector<Mesh> previewLand;
    glm::dvec3 terrainEye{0};
    std::optional<glm::dvec3> savedGrass,characterEye;
    std::uint64_t epoch;
    std::uint64_t traceAttempt=0,stageAttempt=0;
    double time;
    bool third=false,planned=false;
    std::size_t nextBody=0,nextGrass=0;
    std::optional<std::size_t> cpuBody,gpuBody,grassBody;
    std::optional<TerrainBuildIdentity> cpuIdentity;
    PendingSceneReload(app::CommandLineOptions o,nlohmann::json d,nlohmann::json r,
        app::PreparedScene s,std::uint64_t e,double t):options(std::move(o)),document(std::move(d)),replay(std::move(r)),
        prepared(std::move(s)),tracking(prepared->scenario.planets.size()),previewLand(tracking.ready.size()),epoch(e),time(t) {}
};
}

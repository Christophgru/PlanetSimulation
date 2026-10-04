#include "rendering/runtime/RendererState.h"
#include <type_traits>

namespace rendering {
glm::dvec3 Renderer::Impl::previewReloadEye(app::PreparedScene& prepared,std::vector<Mesh>& land,
    std::vector<glm::dvec3>& anchors,nlohmann::json& replay,
    app::CommandLineOptions& preparedOptions,double time) {
    struct Restore {
        Impl& r;app::PreparedScene& scene;std::vector<Mesh>& land;std::vector<glm::dvec3>& anchors;
        nlohmann::json& replay;app::CommandLineOptions& options;
        AstronautMotion motion;SurfaceContact ground;
        std::uint64_t groundRevision,previews;unsigned presses;
        bool replayRestored,boost;double wind;glm::dvec2 controls;
        Restore(Impl& state,app::PreparedScene& s,std::vector<Mesh>& l,std::vector<glm::dvec3>& a,
            nlohmann::json& j,app::CommandLineOptions& o):r(state),scene(s),land(l),anchors(a),replay(j),options(o),
            motion(r.astronaut.motion),ground(r.astronautGround),groundRevision(r.astronautGroundRevision),
            previews(r.characterPreviews),presses(r.inputContext.spacePresses),replayRestored(r.astronautReplayRestored),
            boost(r.astronautBenchmarkBoost),wind(r.characterWindTime),controls(r.astronautFlightControl) {
            using std::swap;swap(r.scene,scene);land.swap(r.meshes.planetMeshes);
            anchors.swap(r.lastTerrainEyes);replay.swap(r.source.replayDocument);swap(r.options,options);
        }
        ~Restore() noexcept {
            static_assert(std::is_nothrow_swappable_v<app::PreparedScene>);
            static_assert(std::is_nothrow_swappable_v<app::CommandLineOptions>);
            using std::swap;swap(r.astronaut.motion,motion);swap(r.astronautGround,ground);
            swap(r.scene,scene);land.swap(r.meshes.planetMeshes);anchors.swap(r.lastTerrainEyes);
            replay.swap(r.source.replayDocument);swap(r.options,options);
            r.astronautGroundRevision=groundRevision;r.characterPreviews=previews;r.inputContext.spacePresses=presses;
            r.astronautReplayRestored=replayRestored;r.astronautBenchmarkBoost=boost;
            r.characterWindTime=wind;r.astronautFlightControl=controls;
        }
    } restore(*this,prepared,land,anchors,replay,preparedOptions);
    astronaut.motion=AstronautMotion{};astronautGround.clear();astronautGroundRevision=0;astronautReplayRestored=false;
    inputContext.spacePresses=options.benchmarkJumpFrame==0 || options.benchmarkBoostFrame==0 ? 1 : 0;
    astronautBenchmarkBoost=options.benchmarkBoostFrame==0;
    astronautFlightControl={options.benchmarkWalkStep>0 ? 1.0 : 0.0,0};characterWindTime=time;
    if(source.replayDocument.contains("astronaut_pose"))
        characterWindTime=source.replayDocument.at("astronaut_pose").value("wind_time_s",time);
    if(!std::isfinite(characterWindTime) || std::abs(characterWindTime)>1e12)
        throw std::invalid_argument("Invalid character wind replay clock");
    const auto selected=scene.scenario.surface_camera.planet_index;
    return previewAstronautEye(0,meshes.planetMeshes[selected].contacts,meshes.planetMeshes[selected].revision);
}
}

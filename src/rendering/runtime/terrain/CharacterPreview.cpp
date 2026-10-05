#include "rendering/runtime/RendererState.h"
#include <type_traits>

namespace rendering {
std::vector<SurfaceContact> Renderer::Impl::characterTerrainContacts() const {
    std::vector<SurfaceContact> contacts(meshes.planetMeshes.size());
    for(std::size_t i=0;i<contacts.size();++i) {
        const auto& mesh=meshes.planetMeshes[i];
        if(mesh.contacts) contacts[i].bind(mesh.contacts,mesh.revision);
        else contacts[i].bind(mesh.vertices,mesh.indices,mesh.revision,
            scene.scenario.planets[i].radius*scene.scenario.metersPerWorldUnit());
    }
    return contacts;
}
glm::dvec3 Renderer::Impl::previewAstronautEye(double elapsed,
    std::vector<SurfaceContact> contacts) {
    // A planning copy uses prospective contacts. Restore every mutable camera/
    // character field on success or failure, before submitting any GPU work.
    struct Restore {
        Impl& r;
        AstronautMotion motion;
        PlanetSurfaceCamera camera;
        std::optional<OrbitCamera> orbitCamera;
        SurfaceContact ground;
        ChasePose view;
        glm::dvec3 terrainEye,orbitCenter;
        double orbitRadius;
        std::size_t characterPlanet,surfacePlanet,orbitPlanet;
        std::uint64_t groundRevision;
        unsigned spacePresses;
        bool replayRestored;
        explicit Restore(Impl& state):r(state),motion(r.astronaut.motion),camera(*r.scene.surfaceCamera),
            orbitCamera(r.scene.planetOrbitCamera),ground(r.astronautGround),view(r.astronautView),
            terrainEye(r.captureTerrainEye),orbitCenter(r.scene.planetOrbitCenter),orbitRadius(r.scene.planetOrbitOuterRadius),
            characterPlanet(r.astronaut.planetIndex),surfacePlanet(r.scene.scenario.surface_camera.planet_index),
            orbitPlanet(r.scene.orbitPlanetIndex),groundRevision(r.astronautGroundRevision),
            spacePresses(r.inputContext.spacePresses),replayRestored(r.astronautReplayRestored) {}
        ~Restore() noexcept {
            static_assert(std::is_nothrow_swappable_v<AstronautMotion>);
            static_assert(std::is_nothrow_swappable_v<PlanetSurfaceCamera>);
            static_assert(std::is_nothrow_swappable_v<SurfaceContact>);
            using std::swap;
            swap(r.astronaut.motion,motion);swap(*r.scene.surfaceCamera,camera);
            r.scene.planetOrbitCamera.swap(orbitCamera);swap(r.astronautGround,ground);
            r.astronautView=view;r.captureTerrainEye=terrainEye;r.scene.planetOrbitCenter=orbitCenter;
            r.scene.planetOrbitOuterRadius=orbitRadius;r.astronaut.planetIndex=characterPlanet;
            r.scene.scenario.surface_camera.planet_index=surfacePlanet;r.scene.orbitPlanetIndex=orbitPlanet;
            r.astronautGroundRevision=groundRevision;r.inputContext.spacePresses=spacePresses;
            r.astronautReplayRestored=replayRestored;
        }
    } restore(*this);
    prepareAstronaut(elapsed,std::move(contacts),true);
    ++characterPreviews;return astronautView.eye;
}
}

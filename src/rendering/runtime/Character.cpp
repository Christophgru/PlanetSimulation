#include "rendering/runtime/RendererState.h"
#include <GLFW/glfw3.h>

namespace rendering {
namespace {
nlohmann::json vector(const glm::dvec3& v) { return {v.x,v.y,v.z}; }
glm::dvec3 vector(const nlohmann::json& j) {
    if (!j.is_array() || j.size()!=3) throw std::invalid_argument("Astronaut replay needs three-vector values");
    return {j.at(0).get<double>(),j.at(1).get<double>(),j.at(2).get<double>()};
}
}
nlohmann::json Renderer::Impl::astronautState() const {
    const auto& p=astronaut.motion.pose();
    nlohmann::json result={{"root",vector(p.root)},{"up",vector(p.up)},
        {"forward",vector(p.forward)},{"right",vector(p.right)},{"arm_swing",p.armSwing},{"walked_m",p.walkedMeters},
        {"height_m",p.flightHeight},{"vertical_velocity_mps",p.verticalVelocity},{"effect_s",p.effectSeconds},
        {"boost_pulse_s",p.boostPulse},{"airborne",p.airborne},{"jetpack_armed",p.jetpackArmed},{"boosting",p.boosting}};
    for (int i=0;i<2;++i) {
        const auto& f=p.feet[i];
        result["feet"].push_back({{"position",vector(f.contact.position)},{"normal",vector(f.contact.normal)},
            {"forward",vector(f.forward)},{"start",vector(f.start)},{"target",vector(f.target)},
            {"progress",f.progress},{"duration_s",f.duration},{"hip",vector(p.hips[i])},{"knee",vector(p.knees[i])},
            {"ankle",vector(p.ankles[i])},{"reached",p.legReached[i]}});
    }
    return result;
}
void Renderer::Impl::prepareAstronaut(double elapsed) {
    const auto& camera=*scene.surfaceCamera;
    const std::size_t index=scene.scenario.surface_camera.planet_index;
    const auto& planet=scene.scenario.planets[index];
    const auto& body=scene.bodies[index+1];
    const auto& mesh=meshes.planetMeshes[index];
    const double units=scene.scenario.metersPerWorldUnit();
    const double radius=planet.radius*units;
    astronaut.planetIndex=index;
    astronautGround.bind(mesh.vertices,mesh.indices,mesh.revision,radius);
    const GroundQuery fallback=[&](const glm::dvec3& radial) {
        return GroundContact{radial*(radius+scene.terrainSurfaces[index].heightAt(radial)*units),radial};
    };
    const GroundQuery ground=[&](const glm::dvec3& radial) {
        auto contact=astronautGround.sample(radial,fallback);
        if (planet.water.enabled && glm::length(contact.position)<radius+planet.water.level_m) {
            const auto up=glm::normalize(radial);
            contact={up*(radius+planet.water.level_m),up};
        }
        return contact;
    };
    if (astronautGroundRevision!=mesh.revision) {
        astronaut.motion.refreshContacts(ground);
        astronautGroundRevision=mesh.revision;
    }
    const auto root=ground(body.toLocalPoint(camera.position()));
    const auto direction=glm::transpose(body.orientation)*camera.direction();
    astronaut.motion.setGravity(AstronautMotion::surfaceGravity(planet.mass_kg,radius,
        planet.rotation.period_seconds,glm::radians(camera.location().latitudeDeg)));
    while (inputContext.spacePresses) { astronaut.motion.pressSpace(); --inputContext.spacePresses; }
    astronaut.motion.holdBoost(options.renderTestMode ? astronautBenchmarkBoost :
        glfwGetKey(window,GLFW_KEY_SPACE)==GLFW_PRESS);
    astronaut.motion.update(root,direction,elapsed,ground);
    if (!astronautReplayRestored && !options.replayPath.empty()) {
        astronautReplayRestored=true;
        const auto replay=config::Config::load(options.replayPath).data();
        if (replay.contains("astronaut_pose")) {
            const auto& j=replay.at("astronaut_pose");
            AstronautPose p;
            p.root=vector(j.at("root")); p.up=vector(j.at("up"));
            p.forward=vector(j.at("forward")); p.right=vector(j.at("right"));
            p.armSwing=j.at("arm_swing").get<double>();
            p.walkedMeters=j.value("walked_m",0.0);
            p.flightHeight=j.value("height_m",0.0); p.verticalVelocity=j.value("vertical_velocity_mps",0.0);
            p.effectSeconds=j.value("effect_s",0.0); p.boostPulse=j.value("boost_pulse_s",0.0);
            p.airborne=j.value("airborne",false); p.jetpackArmed=j.value("jetpack_armed",false); p.boosting=j.value("boosting",false);
            if (!j.at("feet").is_array() || j.at("feet").size()!=2)
                throw std::invalid_argument("Astronaut replay needs two feet");
            for (int i=0;i<2;++i) {
                const auto& f=j.at("feet").at(i);
                p.feet[i].contact={vector(f.at("position")),vector(f.at("normal"))};
                p.feet[i].forward=vector(f.at("forward")); p.feet[i].start=vector(f.at("start"));
                p.feet[i].target=vector(f.at("target")); p.feet[i].progress=f.at("progress").get<double>();
                p.feet[i].duration=f.value("duration_s",.24);
                p.hips[i]=vector(f.at("hip")); p.knees[i]=vector(f.at("knee"));
                p.ankles[i]=vector(f.at("ankle")); p.legReached[i]=f.at("reached").get<bool>();
            }
            if (glm::length(glm::normalize(p.root)*glm::length(root.position)-root.position)<.1)
                astronaut.motion.restore(p);
        }
    }
    const auto chase=astronaut.motion.chase(direction,ground);
    astronautView={body.position+body.orientation*chase.eye/units,
                   body.position+body.orientation*chase.target/units,
                   body.orientation*chase.up};
}
}

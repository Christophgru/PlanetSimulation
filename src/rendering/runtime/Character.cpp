#include "rendering/runtime/RendererState.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <bit>
#include "rendering/foliage/wind/WindField.h"
#include "rendering/runtime/terrain/reload/ReplayEffects.h"

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
        {"boost_pulse_s",p.boostPulse},{"airborne",p.airborne},{"jetpack_armed",p.jetpackArmed},{"boosting",p.boosting},
        {"velocity_mps",vector(p.velocity)},{"suit_up",vector(p.suitUp)},{"thrust_n",p.thrustN},
        {"body_offset_m",vector(p.bodyOffset)}};
    result["wind_time_s"]=characterWindTime;
    const auto& effect=astronaut.exhaust.state();
    result["exhaust"]={{"schema",1},{"emission_phase_s",effect.emissionPhase},{"next_id",effect.nextId},
        {"particles",nlohmann::json::array()},{"capacity",ExhaustParticles::capacity},
        {"draw_calls_per_view",effect.particles.empty() ? 0 : 1},
        {"instance_bytes_per_view",effect.particles.size()*8*sizeof(float)}};
    for (const auto& particle:effect.particles)
        result["exhaust"]["particles"].push_back({{"position_m",vector(particle.position)},
            {"velocity_mps",vector(particle.velocity)},{"age_s",particle.age},
            {"lifetime_s",particle.lifetime},{"id",particle.id}});
    if (p.navigation) {
        const auto& n=*p.navigation;
        result["navigation"]={{"position_m",vector(n.position)},{"velocity_mps",vector(n.velocity)},
            {"up",vector(n.up)},{"reference_body",n.referenceBody},{"outer_space",n.outerSpace},
            {"suit_up",vector(n.suitUp)}};
        const auto indices=astronaut.motion.gravitySourceIndices();
        result["navigation"]["gravity_body_indices"]=indices;
        for (auto i:indices) result["navigation"]["gravity_bodies"].push_back({
            {"index",i},{"position_m",vector(scene.bodies[i].position*scene.scenario.metersPerWorldUnit())},
            {"radius_m",(i==0 ? scene.scenario.sun.radius : scene.scenario.planets[i-1].radius)*scene.scenario.metersPerWorldUnit()},
            {"mass_kg",i==0 ? scene.scenario.sun.mass_kg : scene.scenario.planets[i-1].mass_kg}});
    }
    result["flight_physics"]={{"mass_kg",JetpackPhysics::massKg},{"drag_area_cd_m2",JetpackPhysics::dragArea},
        {"commanded_horizontal_speed_mps",JetpackPhysics::speedTarget},{"maximum_thrust_n",astronaut.motion.maximumThrust()},
        {"air_pressure_pa",astronaut.motion.airPressure()},{"air_density_kg_m3",astronaut.motion.airDensity()},
        {"estimated_exhaust_input_w",JetpackPhysics::exhaustPower(p.thrustN)}};
    for (int i=0;i<2;++i) {
        const auto& f=p.feet[i];
        result["feet"].push_back({{"position",vector(f.contact.position)},{"normal",vector(f.contact.normal)},
            {"forward",vector(f.forward)},{"start",vector(f.start)},{"target",vector(f.target)},
            {"progress",f.progress},{"duration_s",f.duration},{"hip",vector(p.hips[i])},{"knee",vector(p.knees[i])},
            {"ankle",vector(p.ankles[i])},{"reached",p.legReached[i]}});
    }
    const auto* trail=grass.procedural.existingTrail(astronaut.planetIndex);
    result["terrain_plan_eye_world_units"]=vector(p.navigation ? lastTerrainEyes[astronaut.planetIndex] : captureTerrainEye);
    if (p.navigation) for (const auto& eye:lastTerrainEyes)
        result["terrain_plan_eyes_world_units"].push_back(vector(eye));
    if (p.navigation) {
        result["terrain_face_zones"]=lastFaceZones;
        result["flight_view_world"]={{"direction",vector(scene.surfaceCamera->direction())},
            {"up",vector(scene.surfaceCamera->up())}};
        result["chase_view_world"]={{"eye",vector(astronautView.eye)},
            {"target",vector(astronautView.target)},{"up",vector(astronautView.up)}};
        for (const auto& mesh:meshes.planetMeshes) {
            std::uint64_t hash=14695981039346656037ULL;
            const auto append=[&](std::uint32_t word) {
                for (int byte=0;byte<4;++byte) { hash^=(word>>(8*byte))&255; hash*=1099511628211ULL; }
            };
            for (float value:mesh.vertices) append(std::bit_cast<std::uint32_t>(value));
            for (auto value:mesh.indices) append(value);
            if(mesh.residentTerrain) {
                const auto& k=mesh.terrainStats.generation;
                result["terrain_generation_keys"].push_back({{"field",std::to_string(k.field)},
                    {"topology",std::to_string(k.topology)}, {"field_version",k.fieldVersion},
                    {"topology_version",k.topologyVersion},{"backend","compute"}});
            } else result["terrain_mesh_fnv1a64"].push_back(std::to_string(hash));
        }
    }
    result["grass_trail"]=nlohmann::json::array();
    if (trail) for (const auto& segment:trail->segments())
        result["grass_trail"].push_back({vector(segment.start),vector(segment.end)});
    if (const auto eye=grass.procedural.planningEye(astronaut.planetIndex))
        result["grass_plan_eye"]=vector(*eye);
    return result;
}
void Renderer::Impl::prepareAstronaut(double elapsed,std::vector<SurfaceContact> plannedContacts,bool preview) {
    auto& camera=*scene.surfaceCamera;
    const std::size_t index=scene.scenario.surface_camera.planet_index;
    const auto& planet=scene.scenario.planets[index];
    const auto& body=scene.bodies[index+1];
    const double units=scene.scenario.metersPerWorldUnit();
    const double radius=planet.radius*units;
    astronaut.planetIndex=index;
    const bool prospective=!plannedContacts.empty();
    auto contacts=prospective ? std::move(plannedContacts) : characterTerrainContacts();
    if(contacts.size()!=scene.scenario.planets.size()) throw std::logic_error("Character needs contacts for every body");
    const auto contactRevision=contacts[index].revision();
    if(prospective) astronautGround=contacts[index];
    else {
        const auto& mesh=meshes.planetMeshes[index];
        if(mesh.contacts) astronautGround.bind(mesh.contacts,mesh.revision);
        else astronautGround.bind(mesh.vertices,mesh.indices,mesh.revision,radius);
    }
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
    if (astronautGroundRevision!=contactRevision) {
        astronaut.motion.refreshContacts(ground);
        astronautGroundRevision=contactRevision;
    }
    std::vector<FlightBody> flightBodies;
    flightBodies.reserve(scene.bodies.size());
    const double characterStep=options.benchmarkCharacterStep>0 ? options.benchmarkCharacterStep : options.benchmarkWalkStep/6.0;
    const double orbitalRate=options.renderTestMode ?
        (characterStep>0 ? options.benchmarkStep/characterStep : 0) : (simulationClock.paused() ? 0 : simulationClock.speed());
    const auto root=ground(body.toLocalPoint(camera.position()));
    const auto direction=glm::transpose(body.orientation)*camera.direction();
    const auto environment=[&](const config::PlanetConfig& bodyConfig) {
        FlightEnvironment e; e.radiusMeters=bodyConfig.radius*units; e.planetMassKg=bodyConfig.mass_kg;
        e.spinRadiansPerSecond=bodyConfig.rotation.period_seconds==0 ? 0 : 2*glm::pi<double>()/bodyConfig.rotation.period_seconds;
        e.seaLevelMeters=bodyConfig.water.enabled ? bodyConfig.water.level_m : 0;
        e.atmosphere=bodyConfig.atmosphere; return e;
    };
    for (std::size_t i=0;i<scene.bodies.size();++i) {
        FlightBody b; b.position=scene.bodies[i].position*units;
        b.velocity=scene.bodies[i].velocity*units*orbitalRate; b.orientation=scene.bodies[i].orientation;
        b.orientable=i>0;
        if (i==0) {
            b.environment.radiusMeters=scene.scenario.sun.radius*units;
            b.environment.planetMassKg=scene.scenario.sun.mass_kg;
            b.environment.atmosphere.enabled=false;
        } else {
            b.environment=environment(scene.scenario.planets[i-1]);
            b.angularVelocity=b.orientation*glm::dvec3(0,0,b.environment.spinRadiansPerSecond*orbitalRate);
            b.ground=[this,i,units,contactSource=contacts[i-1]](const glm::dvec3& p) mutable {
                const auto radial=glm::normalize(p);
                const auto& planet=scene.scenario.planets[i-1];
                const double radius=planet.radius*units;
                const GroundQuery fallback=[&](const glm::dvec3& r) {
                    return GroundContact{r*(radius+scene.terrainSurfaces[i-1].heightAt(r)*units),r};
                };
                auto contact=astronaut.planetIndex==i-1 ? astronautGround.sample(radial,fallback) : contactSource.sample(radial,fallback);
                if (planet.water.enabled && glm::length(contact.position)<radius+planet.water.level_m)
                    contact={radial*(radius+planet.water.level_m),radial};
                return contact;
            };
        }
        b.windTime=characterWindTime;
        if (i>0) {
            const auto foliage=scene.scenario.planets[i-1].foliage;
            b.wind=[foliage](const glm::dvec3& p,double time) { return WindField::velocity(foliage,p,time); };
        }
        flightBodies.push_back(std::move(b));
    }
    const auto exhaustBodies=flightBodies;
    astronaut.motion.setFlightWorld(std::move(flightBodies),index+1);
    astronaut.motion.setFlightViewUp(glm::transpose(body.orientation)*camera.up());
    astronaut.motion.setFlightEnvironment(environment(planet),JetpackPhysics::maximumThrust(environment(scene.scenario.planets.front())));
    astronaut.motion.setFlightControl(astronautFlightControl);
    while (inputContext.spacePresses) { astronaut.motion.pressSpace(); --inputContext.spacePresses; }
    astronaut.motion.holdBoost(options.renderTestMode ? astronautBenchmarkBoost :
        glfwGetKey(window,GLFW_KEY_SPACE)==GLFW_PRESS);
    const bool wasAirborne=astronaut.motion.ready() && astronaut.motion.pose().airborne;
    const bool wasBoosting=astronaut.motion.ready() && astronaut.motion.pose().boosting;
    const bool wasOuterSpace=astronaut.motion.ready() && astronaut.motion.pose().navigation && astronaut.motion.pose().navigation->outerSpace;
    astronaut.motion.update(root,direction,elapsed,ground);
    if (!astronautReplayRestored && !options.replayPath.empty()) {
        astronautReplayRestored=true;
        const auto& replay=source.replayDocument;
        if (replay.contains("astronaut_pose")) {
            const auto& j=replay.at("astronaut_pose");
            if (!preview && j.contains("exhaust")) astronaut.exhaust.restore(characterReplay::exhaust(j.at("exhaust")));
            AstronautPose p;
            p.root=vector(j.at("root")); p.up=vector(j.at("up"));
            p.forward=vector(j.at("forward")); p.right=vector(j.at("right"));
            p.armSwing=j.at("arm_swing").get<double>();
            p.walkedMeters=j.value("walked_m",0.0);
            p.flightHeight=j.value("height_m",0.0); p.verticalVelocity=j.value("vertical_velocity_mps",0.0);
            p.effectSeconds=j.value("effect_s",0.0); p.boostPulse=j.value("boost_pulse_s",0.0);
            p.airborne=j.value("airborne",false); p.jetpackArmed=j.value("jetpack_armed",false); p.boosting=j.value("boosting",false);
            p.velocity=j.contains("velocity_mps") ? vector(j.at("velocity_mps")) : p.up*p.verticalVelocity;
            p.suitUp=j.contains("suit_up") ? vector(j.at("suit_up")) : p.up;
            p.thrustN=j.value("thrust_n",0.0);
            if (j.contains("navigation")) {
                const auto& n=j.at("navigation");
                p.navigation=FlightState{vector(n.at("position_m")),vector(n.at("velocity_mps")),vector(n.at("up")),
                    n.at("reference_body").get<std::size_t>(),n.at("outer_space").get<bool>()};
                p.navigation->suitUp=n.contains("suit_up") ? vector(n.at("suit_up")) :
                    body.orientation*p.suitUp;
            }
            p.bodyOffset=j.contains("body_offset_m") ? vector(j.at("body_offset_m")) : glm::dvec3(0);
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
            if (p.navigation || glm::length(glm::normalize(p.root)*glm::length(root.position)-root.position)<.1)
                astronaut.motion.restore(p);
            if (p.navigation && j.contains("flight_view_world")) {
                const auto& v=j.at("flight_view_world");
                camera.setWorldView(vector(v.at("direction")),vector(v.at("up")));
            }
            if (!preview && !terrainPublication && j.contains("grass_plan_eye"))
                grass.procedural.restorePlanningEye(index,vector(j.at("grass_plan_eye")));
            if (!preview && j.contains("grass_trail"))
                grass.procedural.trail(index).restore(characterReplay::trail(j.at("grass_trail")));
        }
    }
    const auto targetBody=astronaut.motion.pose().navigation ?
        astronaut.motion.pose().navigation->referenceBody : astronaut.motion.referenceBody();
    if (targetBody!=index+1) {
        auto& publications=profiler.publications();if(!preview) publications.captureMode(options.renderTestMode);
        std::uint64_t attempt=0;
        if(!preview && publications.enabled()) {
            const auto next=targetBody-1;const auto& key=meshes.planetMeshes[next].terrainStats.generation;
            TerrainBuildIdentity k;k.epoch=terrainSceneEpoch;k.serial=installedTerrainSerial[next];k.bodyIndex=next;
            k.bodyName=scene.scenario.planets[next].name;k.field=key.field;k.fieldVersion=key.fieldVersion;
            k.topologyVersion=key.topologyVersion;k.backend=key.backend;k.resident=bool(terrainPublication);
            k.eye=scene.bodies[targetBody].toLocalPoint(camera.position());k.localMask=lastLocalMask[next];
            attempt=publications.begin(PublicationProfiler::Key::from(k),PublicationProfiler::Kind::Handoff);
        }
        PublicationProfiler::Preparation handoff(&publications,attempt);
        astronaut.motion.reframeFlight(targetBody);
        const auto next=targetBody-1;
        const auto& nextPlanet=scene.scenario.planets[next]; const auto& nextBody=scene.bodies[targetBody];
        captureTerrainEye=nextBody.toLocalPoint(camera.position());
        const auto worldRoot=nextBody.position+nextBody.orientation*astronaut.motion.pose().root/units;
        if (const auto& nav=astronaut.motion.pose().navigation)
            camera.followFlight(worldRoot,nav->up,nav->outerSpace);
        const auto look=camera.direction(); const auto viewUp=camera.up();
        const coordinates::PlanetLocalFrame frame(nextBody.position,nextPlanet.radius,nextBody.orientation);
        const auto location=frame.fromWorld(worldRoot);
        PlanetSurfaceCamera replacement(frame,location,scene.sunPosition,camera.fov(),camera.walkSpeed());
        replacement.mountTerrain(scene.terrainSurfaces[next],camera.configuredClearance(),nextPlanet.water.enabled ?
            std::optional<double>(nextPlanet.water.level_m/units) : std::nullopt);
        const auto ned=frame.nedAt(replacement.location());
        replacement.setDirectionNed(ned.fromWorld(look),ned.fromWorld(viewUp));
        camera=std::move(replacement);
        scene.scenario.surface_camera.planet_index=next;
        scene.orbitPlanetIndex=next; scene.planetOrbitCenter=nextBody.position;
        double height=nextPlanet.terrain_landscape.maximumAbsoluteHeightMeters();
        for (const auto& noise:nextPlanet.surface_noise) height+=noise.amplitude_m;
        scene.planetOrbitOuterRadius=nextPlanet.radius+std::max(height,nextPlanet.water.enabled ? nextPlanet.water.level_m : 0.0)/units;
        if (scene.planetOrbitCamera) {
            const float minimum=scene.planetOrbitOuterRadius+2/units;
            const float initial=std::max(float(2.8*nextPlanet.radius),1.5f*minimum);
            const float maximum=std::max(float(20*nextPlanet.radius),2*initial);
            *scene.planetOrbitCamera=OrbitCamera(glm::vec3(nextBody.position),
                glm::vec3(glm::normalize(worldRoot-nextBody.position)*double(initial)),
                OrbitCamera::Settings{minimum,maximum,.96f});
        }
        // Destination contacts are part of the same prospective/committed set.
        // Bind them now, including on the first handoff frame, before chase or
        // subsequent collision queries can consume the new body's terrain.
        astronaut.planetIndex=next;astronautGround=contacts[next];astronautGroundRevision=astronautGround.revision();
        astronaut.motion.setFlightEnvironment(environment(nextPlanet),JetpackPhysics::maximumThrust(environment(scene.scenario.planets.front())));
        if(!preview) handoff.bound(index,publicationGeneration(next));
        if(!preview) std::cout << "Astronaut destination: " << nextPlanet.name << "\n" << std::flush;
    }
    if (!preview && !options.renderTestMode) {
        if (astronaut.motion.pose().boosting!=wasBoosting)
            std::cout << "Astronaut jetpack thrust " << (astronaut.motion.pose().boosting ? "on" : "off") << "\n" << std::flush;
        const bool outer=astronaut.motion.pose().navigation && astronaut.motion.pose().navigation->outerSpace;
        if (outer!=wasOuterSpace)
            std::cout << "Astronaut orientation: " << (outer ? "outer space" : "local planet") << "\n" << std::flush;
    }
    const auto current=astronaut.planetIndex;
    const auto& currentBody=scene.bodies[current+1];
    const auto& pose=astronaut.motion.pose();
    if (pose.navigation) {
        camera.followFlight(pose.navigation->position/units,pose.navigation->up,pose.navigation->outerSpace);
    } else if (pose.airborne || wasAirborne) {
        camera.endFlight();
        camera.followSurfaceDirection(currentBody.position+currentBody.orientation*pose.root/units);
    }
    if(!preview) grass.procedural.trail(current).observe(pose.root,!pose.airborne &&
        (!scene.scenario.planets[current].water.enabled || glm::length(pose.root)>
            scene.scenario.planets[current].radius*units+scene.scenario.planets[current].water.level_m+.001));
    const GroundQuery currentGround=[&](const glm::dvec3& p) {
        const auto radial=glm::normalize(p); const auto& planet=scene.scenario.planets[current];
        const GroundQuery fallback=[&](const glm::dvec3& r) {
            return GroundContact{r*(scene.scenario.planets[current].radius+
                scene.terrainSurfaces[current].heightAt(r))*units,r};
        };
        auto contact=astronautGround.sample(radial,fallback);
        if(planet.water.enabled && glm::length(contact.position)<planet.radius*units+planet.water.level_m)
            contact={radial*(planet.radius*units+planet.water.level_m),radial};
        return contact;
    };
    const auto chase=astronaut.motion.chase(glm::transpose(currentBody.orientation)*camera.direction(),
        current==index ? ground : currentGround,glm::transpose(currentBody.orientation)*camera.up());
    astronautView={currentBody.position+currentBody.orientation*chase.eye/units,
                   currentBody.position+currentBody.orientation*chase.target/units,
                   currentBody.orientation*chase.up};
    if(preview) return; // Planning never advances exhaust/trails or consumes replay anchors.
    ExhaustEmitter emitter;
    const auto basis=currentBody.orientation*pose.suitBasis();
    const auto worldRoot=currentBody.position*units+currentBody.orientation*pose.root;
    for (int side=0;side<2;++side)
        emitter.nozzles[side]=worldRoot+basis*(glm::dvec3(side==0 ? -.12 : .12,.76,.23)+pose.bodyOffset);
    emitter.up=basis[1]; emitter.firing=pose.boosting && pose.thrustN>0;
    emitter.velocity=pose.navigation ? pose.navigation->velocity :
        exhaustBodies[current+1].surfaceVelocity(worldRoot)+currentBody.orientation*pose.velocity;
    const auto before=astronaut.lastEmitter.value_or(emitter);
    auto sampled=exhaustBodies;
    double sampledOffset=std::numeric_limits<double>::infinity();
    const auto sampleAir=[&](const glm::dvec3& position,double offset) {
        // All particles in one substep share the ephemeris sampling time.
        // Cache descriptors rather than copying heap-owned callbacks per bubble.
        if (offset!=sampledOffset) {
            for (std::size_t i=0;i<sampled.size();++i) {
                auto& b=sampled[i]; const auto& target=exhaustBodies[i];
                b.position=target.position+target.velocity*offset; b.windTime=target.windTime+offset;
                const double spin=glm::length(target.angularVelocity);
                b.orientation=spin>1e-12 ? glm::dmat3(glm::rotate(glm::dmat4(1),spin*offset,target.angularVelocity/spin))*target.orientation : target.orientation;
            }
            sampledOffset=offset;
        }
        ExhaustAir air; air.gravity=FlightNavigation::gravity(sampled,position);
        for (auto i:FlightNavigation::nearest(sampled,position)) {
            const auto& b=sampled[i];
            const double density=JetpackPhysics::density(b.environment,b.localPoint(position));
            if (density<=0) continue;
            air.velocity+=b.airVelocity(position)*density; air.density+=density;
        }
        if (air.density>0) air.velocity/=air.density;
        return air;
    };
    if (astronaut.exhaustTime) {
        const double gap=std::max(0.0,characterWindTime-*astronaut.exhaustTime-elapsed);
        if (gap>=2) {
            auto expired=astronaut.exhaust.state(); expired.particles.clear(); expired.emissionPhase=0;
            astronaut.exhaust.restore(expired);
        } else if (gap>1e-9) {
            auto inactive=before; inactive.firing=false;
            astronaut.exhaust.update(gap,inactive,inactive,[&](const auto& p,double offset) { return sampleAir(p,offset-elapsed); });
        }
    }
    astronaut.exhaust.update(elapsed,before,emitter,sampleAir);
    astronaut.lastEmitter=emitter; astronaut.exhaustTime=characterWindTime;
}
}

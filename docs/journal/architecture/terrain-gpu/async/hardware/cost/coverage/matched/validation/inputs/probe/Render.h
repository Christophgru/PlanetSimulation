#pragma once
#include "rendering/runtime/terrain/reload/ReplayEffects.h"

namespace coverage {
inline bool retainPlan=false;
inline unsigned skippedPreparations=0;
inline int controlledPass=0; // 1: full scene; 2: grass draws suppressed.
inline Snapshot controlledSnapshot;
inline json controlledControls;
inline glm::dvec3 vector(const json& j) { return rendering::characterReplay::vector(j); }
inline glm::mat4 matrix(const json& j) {
    if(j.size()!=16) throw std::invalid_argument("Inspection matrix needs 16 values");
    glm::mat4 m;for(int i=0;i<16;++i) glm::value_ptr(m)[i]=j.at(i).get<float>();return m;
}
inline rendering::AstronautPose pose(const json& j) {
    if(j.at("airborne").get<bool>() || j.contains("navigation"))
        throw std::invalid_argument("Matched inspection requires a grounded pose");
    rendering::AstronautPose p;
    p.root=vector(j.at("root"));p.up=vector(j.at("up"));
    p.forward=vector(j.at("forward"));p.right=vector(j.at("right"));
    p.armSwing=j.at("arm_swing");p.walkedMeters=j.at("walked_m");
    p.effectSeconds=j.at("effect_s");p.suitUp=vector(j.at("suit_up"));
    p.bodyOffset=vector(j.at("body_offset_m"));
    p.flightHeight=j.at("height_m");p.verticalVelocity=j.at("vertical_velocity_mps");
    p.boostPulse=j.at("boost_pulse_s");p.jetpackArmed=j.at("jetpack_armed");p.boosting=j.at("boosting");
    p.velocity=vector(j.at("velocity_mps"));p.thrustN=j.at("thrust_n");
    for(int i=0;i<2;++i) {
        const auto& f=j.at("feet").at(i);auto& foot=p.feet[i];
        foot.contact={vector(f.at("position")),vector(f.at("normal"))};
        foot.forward=vector(f.at("forward"));foot.start=vector(f.at("start"));foot.target=vector(f.at("target"));
        foot.progress=f.at("progress");foot.duration=f.at("duration_s");
        p.hips[i]=vector(f.at("hip"));p.knees[i]=vector(f.at("knee"));p.ankles[i]=vector(f.at("ankle"));
        p.legReached[i]=f.at("reached");
    }
    return p;
}
inline void saveJson(const std::filesystem::path& path,const json& value) {
    std::ofstream out(path.string()+".tmp");out<<value.dump(2)<<'\n';out.close();
    if(!out) throw std::runtime_error("Cannot write matched inspection receipt");
    std::filesystem::rename(path.string()+".tmp",path);
}
struct GroundTarget {
    GLuint fbo=0,texture[2]{},depth=0;
    GroundTarget(int w,int h) {
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(2,texture);
        for(int i=0;i<2;++i) {
            glBindTexture(GL_TEXTURE_2D,texture[i]);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,texture[i],0);
        }
        const GLenum attachments[2]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,attachments);
        glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,w,h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Incomplete eligible-ground target");
    }
    ~GroundTarget() {glDeleteRenderbuffers(1,&depth);glDeleteTextures(2,texture);glDeleteFramebuffers(1,&fbo);}
};
template<class State> void renderMatched(State& r,const Snapshot& native,const json& controls,
        PFNGLGETBUFFERSUBDATAPROC reader) {
    const auto folder=native.folder/"matched";
    std::filesystem::create_directories(folder/"images");std::filesystem::create_directories(folder/"ground");
    json common;
    if(controls.contains("coverage_reference")) {
        std::ifstream in(controls.at("coverage_reference").get<std::string>());in>>common;
    } else {
        const auto view=matrix(native.metadata.at("view"));
        const glm::dvec3 eye=glm::vec3(glm::inverse(view)[3]);
        const auto clip=rendering::surfaceClipPlanes(.2/r.scene.scenario.metersPerWorldUnit(),
            glm::length(eye-r.scene.sunPosition),r.scene.scenario.sun.radius);
        common={{"schema",1},{"view",native.metadata.at("view")},{"projection",native.metadata.at("projection")},
            {"viewport",native.metadata.at("viewport")},{"fov",r.scene.surfaceCamera->fov()},
            {"clip",{clip.nearPlane,clip.farPlane}},{"astronaut",native.metadata.at("astronaut")},
            {"scene_time_s",12.0},{"model",native.metadata.at("model")},
            {"meters_per_radius",native.metadata.at("meters_per_radius")}};
    }
    if(common.at("schema")!=1 || common.at("viewport")!=native.metadata.at("viewport") ||
       common.at("model")!=native.metadata.at("model") ||
       common.at("meters_per_radius")!=native.metadata.at("meters_per_radius"))
        throw std::runtime_error("Common inspection scene/viewport mismatch");
    const auto view=matrix(common.at("view"));const auto projection=matrix(common.at("projection"));
    const float fov=common.at("fov");
    const rendering::ClipPlanes clip{common.at("clip").at(0),common.at("clip").at(1)};
    const int w=native.width,h=native.height;
    const auto derived=rendering::perspectiveProjection(fov,float(w)/h,clip);
    if(std::vector<float>(glm::value_ptr(derived),glm::value_ptr(derived)+16)!=common.at("projection").get<std::vector<float>>())
        throw std::runtime_error("Common inspection projection mismatch");
    // Copy complete owners, including trail anchor/revision and motion step state,
    // so subsequent native frames resume their original state after this exclusion.
    struct Restore {
        State& r;rendering::AstronautMotion motion;rendering::GrassTrail trail;rendering::ExhaustParticles exhaust;
        GLint draw=0,read=0,pack=0,alignment=0,viewport[4]{};
        Restore(State& s):r(s),motion(s.astronaut.motion),trail(s.grass.procedural.trail(0)),exhaust(s.astronaut.exhaust) {
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);glGetIntegerv(GL_VIEWPORT,viewport);
            glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
        }
        ~Restore() {
            r.astronaut.motion=motion;r.grass.procedural.trail(0)=trail;r.astronaut.exhaust=exhaust;
            retainPlan=false;controlledPass=0;
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);
            glBindBuffer(GL_PIXEL_PACK_BUFFER,pack);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
            glPixelStorei(GL_PACK_ALIGNMENT,alignment);
            glDisable(GL_STENCIL_TEST);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
        }
    } restore(r);
    const auto workload=benchmarkObservation(r,glfwGetCurrentContext());
    const auto publication=r.terrainPublicationState();const auto revisions=r.geometryRevisions();
    r.astronaut.motion.restore(pose(common.at("astronaut")));
    r.grass.procedural.trail(0).restore(rendering::characterReplay::trail(common.at("astronaut").at("grass_trail")));
    r.astronaut.exhaust=rendering::ExhaustParticles{};
    retainPlan=true;skippedPreparations=0;
    controlledControls={{"coverage_request",native.request},{"coverage_distance_m",0}};
    controlledSnapshot=Snapshot{};controlledSnapshot.readBuffer=reader;controlledSnapshot.folderOverride=folder/"grass";
    rendering::WaterReflectionTarget output;output.ensure(w,h,false,8192);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    json exposures=json::array();
    const glm::dvec3 eye=glm::vec3(glm::inverse(view)[3]);
    const auto render=[&](int pass) {
        controlledPass=pass;
        const auto exposure=rendering::renderScene(r.scene.scenario,r.scene.atmosphereOptics,r.scene.bodies,view,fov,eye,
            r.shader,r.waterShader,r.skyboxShader,r.waterReflection,r.shadowShader,r.terrainShadows,
            r.atmosphereShader,r.atmosphere,r.reflectionAtmosphere,r.atmosphereColumns,
            r.meshes.sunMesh,r.meshes.skyboxMesh,r.meshes.planetMeshes,r.meshes.waterMeshes,w,h,
            clip,0,true,nullptr,false,output.framebuffer(),&r.grass,common.at("scene_time_s"),&r.astronaut,
            nullptr,r.terrainPublication.get(),nullptr);
        exposures.push_back({{"pass",pass},{"exposure",exposure.exposure},{"hdr",exposure.hdrOutput}});
        glBindFramebuffer(GL_READ_FRAMEBUFFER,output.framebuffer());
        std::vector<std::uint8_t> rgba(std::size_t(w)*h*4),stencil(std::size_t(w)*h);
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        glReadPixels(0,0,w,h,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,stencil.data());
        controlledSnapshot.folder=folder/"images";
        controlledSnapshot.write(pass==1 ? "grass.rgba" : "bare.rgba",rgba);
        controlledSnapshot.write(pass==1 ? "grass.stencil" : "bare.stencil",stencil);
    };
    render(1);
    controlledSnapshot.folder=folder/"grass";controlledSnapshot.finish(r);
    render(2);controlledPass=0;
    // Ground eligibility is rasterized from the actual live GPU VAO, independent
    // of scalar candidate budgets. Bare final stencil removes character/other-body
    // occlusion; transparent water is excluded by the water-clearance biome rule.
    rendering::OwnedShader groundShader("tests/app/terrain/native/coverage/matched/ground.vert",
        "tests/app/terrain/native/coverage/matched/ground.frag");
    GroundTarget ground(w,h);glViewport(0,0,w,h);glDisable(GL_BLEND);glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
    const GLfloat clear[4]={0,0,0,0};glClearBufferfv(GL_COLOR,0,clear);glClearBufferfv(GL_COLOR,1,clear);glClear(GL_DEPTH_BUFFER_BIT);
    groundShader.use();groundShader.setMat4("model",glm::value_ptr(matrix(common.at("model"))));
    groundShader.setMat4("view",glm::value_ptr(view));groundShader.setMat4("projection",glm::value_ptr(projection));
    const auto& planet=r.scene.scenario.planets[0];const double scale=planet.radius*r.scene.scenario.metersPerWorldUnit();
    const auto root=vector(common.at("astronaut").at("root"))/scale;
    groundShader.setFloat3("rootBody",root.x,root.y,root.z);groundShader.setFloat("scale",scale);
    groundShader.setFloat3("planetColor",planet.color[0],planet.color[1],planet.color[2]);
    const auto rock=planet.terrain_material.slopeMetricRange();groundShader.setFloat2("rockRange",rock[0],rock[1]);
    double maximumHeight=planet.terrain_landscape.maximumAbsoluteHeightMeters();
    for(const auto& noise:planet.surface_noise) maximumHeight+=noise.amplitude_m;
    groundShader.setFloat3("landscapeLevels",planet.water.enabled ? planet.water.level_m : 0,.1,maximumHeight);
    groundShader.setInt("landscape",planet.terrain_landscape.enabled);groundShader.setInt("water",planet.water.enabled);
    groundShader.setFloat("waterClearance",planet.foliage.water_clearance_m);groundShader.setFloat("greenRatio",planet.foliage.green_ratio);
    r.meshes.planetMeshes[0].draw();
    std::vector<float> groundPixels(std::size_t(w)*h*4);
    glReadPixels(0,0,w,h,GL_RGBA,GL_FLOAT,groundPixels.data());
    controlledSnapshot.folder=folder/"ground";controlledSnapshot.write("eligible.rgba32f",groundPixels);
    glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,w,h,GL_RGBA,GL_FLOAT,groundPixels.data());
    controlledSnapshot.write("plane.rgba32f",groundPixels);
    const auto afterWorkload=benchmarkObservation(r,glfwGetCurrentContext());
    if(revisions!=r.geometryRevisions() || workload!=afterWorkload || publication!=r.terrainPublicationState() ||
       skippedPreparations!=(r.terrainPublication ? 0 : 2*r.scene.scenario.planets.size()))
        throw std::runtime_error("Matched render mutated native generation/plan or preparation suppression failed");
    saveJson(folder/"controls.json",common);
    saveJson(folder/"receipt.json",{{"schema",1},{"source_native_frame",native.metadata.at("profile_frame")},
        {"source_native_request",native.request},{"native_workload_before",workload},{"native_workload_after",afterWorkload},
        {"native_publication_before",publication},{"native_publication_after",r.terrainPublicationState()},
        {"revisions",revisions},{"cpu_preparations_skipped",skippedPreparations},
        {"final_exposures",exposures},
        {"additional_pixel_reads",6},{"additional_pixel_bytes",std::uint64_t(w)*h*42},
        {"scope","Controlled live-plan scene; nearest opaque grass stencil after character plus RGB contribution after water/atmosphere/tone mapping; flare/overlays excluded"},
        {"root_visibility_tolerance_m",.15},{"composed_rgb_threshold",1},
        {"root_visibility_method","Ray intersection with live triangle plane at projected root; one-sided view-depth occlusion allowance"},
        {"timing_acceptance",false},{"coverage_acceptance",false}});
}
}

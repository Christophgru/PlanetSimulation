#pragma once
// Private blocking inspection of actual main-view draws. Never linked into the
// shipping app or the timed native probe. Reflection buffers are read only after
// the main queues have already been copied, not treated as main-view evidence.
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>

namespace coverage {
using json=nlohmann::json;
struct Blade { glm::vec4 rootFade,upLow,variation,wind; };
static_assert(sizeof(Blade)==64);
struct Snapshot {
    std::uint64_t completed=0,request=0,bufferReads=0,pixelReads=0,bytes=0;
    GLuint framebuffer=0;
    int width=0,height=0,queue=-1;
    json metadata;
    std::array<std::vector<Blade>,2> blades;
    std::vector<float> before,after;
    std::filesystem::path folder;
    std::filesystem::path folderOverride;
    PFNGLGETBUFFERSUBDATAPROC readBuffer=nullptr;

    static std::vector<float> uniform(GLuint program,const char* name,int count) {
        const auto location=glGetUniformLocation(program,name);
        if(location<0) throw std::runtime_error(std::string("Missing inspection uniform ")+name);
        std::vector<float> value(count);glGetUniformfv(program,location,value.data());return value;
    }
    template<class T> void write(const char* name,const std::vector<T>& data) {
        std::ofstream out(folder/name,std::ios::binary);
        out.write(reinterpret_cast<const char*>(data.data()),data.size()*sizeof(T));
        if(!out) throw std::runtime_error("Cannot write coverage buffer");
    }
    std::vector<float> depth() {
        std::vector<float> pixels(std::size_t(width)*height);
        GLint pack=0;glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
        glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,pixels.data());
        glBindBuffer(GL_PIXEL_PACK_BUFFER,pack);
        ++pixelReads;bytes+=pixels.size()*sizeof(float);return pixels;
    }
    template<class State> void draw(State& r,GLenum mode,const void* offset,
            const json& controls,PFNGLDRAWARRAYSINDIRECTPROC original) {
        const auto token=controls.value("coverage_request",0ull);
        GLint program=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);
        const auto& body=r.scene.bodies[1];const auto& planet=r.scene.scenario.planets[0];
        const auto expected=rendering::sphereModel(glm::vec3(body.position),float(planet.radius),glm::mat3(body.orientation));
        const bool earth=GLuint(program)==r.grass.shader.id &&
            uniform(program,"model",16)==std::vector<float>(glm::value_ptr(expected),glm::value_ptr(expected)+16);
        if(queue==1 && earth && uniform(program,"uClipRadius",1)[0]>=0) {
            GLint indirect=0,copy=0;glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING,&indirect);
            glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&copy);
            std::array<std::uint32_t,8> commands{};glBindBuffer(GL_COPY_READ_BUFFER,indirect);
            readBuffer(GL_COPY_READ_BUFFER,0,32,commands.data());glBindBuffer(GL_COPY_READ_BUFFER,copy);
            ++bufferReads;bytes+=32;metadata["reflection_instances_after_main"]={commands[1],commands[5]};
        }
        const bool target=token && token!=completed && r.astronaut.motion.ready() &&
            int(r.cameraInput.mode())==3 && r.astronaut.planetIndex==0 &&
            r.astronaut.motion.pose().walkedMeters>=controls.value("coverage_distance_m",0.0) &&
            earth && uniform(program,"uClipRadius",1)[0]<0;
        if(!target) {original(mode,offset);return;}
        const auto q=reinterpret_cast<std::uintptr_t>(offset)/16;
        if(q>1 || reinterpret_cast<std::uintptr_t>(offset)%16 || mode!=GL_TRIANGLE_STRIP)
            throw std::runtime_error("Unexpected grass indirect command layout");
        GLint drawFramebuffer=0,viewport[4];glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFramebuffer);
        glGetIntegerv(GL_VIEWPORT,viewport);
        if(q==0) {
            if(queue!=-1) throw std::runtime_error("Duplicate main grass queue");
            request=token;framebuffer=drawFramebuffer;width=viewport[2];height=viewport[3];
            if(viewport[0] || viewport[1] || width<=0 || height<=0) throw std::runtime_error("Unsupported inspection viewport");
            const auto* base=std::getenv("PLANET_NATIVE_COVERAGE");
            if(!base) throw std::runtime_error("Missing coverage output directory");
            folder=folderOverride.empty() ? std::filesystem::path(base)/std::to_string(request) : folderOverride;
            std::filesystem::create_directories(folder);
            const auto& pose=r.astronaut.motion.pose();
            metadata={{"schema",1},{"request",request},{"body",0},{"view_identity","main"},
                {"profile_frame",r.profiler.nextFrameNumber()-1},{"backend",r.options.terrainBackend},
                {"viewport",{width,height}},{"walked_m",pose.walkedMeters},{"root_m",{pose.root.x,pose.root.y,pose.root.z}},
                {"meters_per_radius",r.scene.scenario.planets[0].radius*r.scene.scenario.metersPerWorldUnit()},
                {"model",uniform(program,"model",16)},{"view",uniform(program,"view",16)},
                {"projection",uniform(program,"projection",16)},{"wind_s",uniform(program,"uTime",1)[0]},
                {"astronaut",r.astronautState()},{"publication_before_grass",r.terrainPublicationState()},
                {"workload",benchmarkObservation(r,glfwGetCurrentContext())},
                {"land_revision",r.meshes.planetMeshes[0].revision},
                {"land_field",std::to_string(r.meshes.planetMeshes[0].terrainStats.generation.field)},
                {"land_topology",std::to_string(r.meshes.planetMeshes[0].terrainStats.generation.topology)},
                {"depth_scope","main opaque ground before grass and immediately after both queues; later astronaut/water/atmosphere occlusion excluded"}};
            metadata["trail_segments_m"]=json::array();
            if(const auto* trail=r.grass.procedural.existingTrail(0)) for(const auto& segment:trail->segments())
                metadata["trail_segments_m"].push_back({{segment.start.x,segment.start.y,segment.start.z},{segment.end.x,segment.end.y,segment.end.z}});
            GLint readFramebuffer=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFramebuffer);
            glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer);before=depth();glBindFramebuffer(GL_READ_FRAMEBUFFER,readFramebuffer);
        } else if(queue!=0 || request!=token || GLuint(drawFramebuffer)!=framebuffer)
            throw std::runtime_error("Unmatched main grass queue");
        GLint indirect=0,copy=0,bladeBuffer=0;void* bladeOffset=nullptr;
        glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING,&indirect);glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&copy);
        glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&bladeBuffer);
        glGetVertexAttribPointerv(0,GL_VERTEX_ATTRIB_ARRAY_POINTER,&bladeOffset);
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        std::array<std::uint32_t,4> command{};
        glBindBuffer(GL_COPY_READ_BUFFER,indirect);readBuffer(GL_COPY_READ_BUFFER,q*16,16,command.data());++bufferReads;bytes+=16;
        if(command[0]!=(q==0 ? 14u : 4u) || command[2] || command[3]) throw std::runtime_error("Unexpected indirect grass command");
        glBindBuffer(GL_COPY_READ_BUFFER,bladeBuffer);GLint64 size=0;glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&size);
        const auto start=reinterpret_cast<std::uintptr_t>(bladeOffset);
        if(size<0 || size%128 || start!=q*std::uint64_t(size)/2 || command[1]>std::uint64_t(size)/128)
            throw std::runtime_error("Grass queue exceeds allocated capacity");
        blades[q].resize(command[1]);
        if(!blades[q].empty()) {readBuffer(GL_COPY_READ_BUFFER,start,blades[q].size()*64,blades[q].data());++bufferReads;bytes+=blades[q].size()*64;}
        glBindBuffer(GL_COPY_READ_BUFFER,copy);
        metadata["queues"][q]={{"queue",q},{"vertices_per_blade",command[0]},{"instances",command[1]},
            {"offset_bytes",start},{"capacity",std::uint64_t(size)/128},{"record_bytes",64}};
        original(mode,offset);queue=q;
        if(q==1) {
            GLint readFramebuffer=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFramebuffer);
            glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer);after=depth();glBindFramebuffer(GL_READ_FRAMEBUFFER,readFramebuffer);
        }
    }
    template<class State> void finish(State& r) {
        if(queue==-1) return;
        if(queue!=1) throw std::runtime_error("Incomplete main grass inspection");
        if(r.meshes.planetMeshes[0].revision!=metadata.at("land_revision").get<std::uint64_t>())
            throw std::runtime_error("Terrain changed during coverage draw");
        metadata["publication_after_frame"]=r.terrainPublicationState();
        write("detailed.blades",blades[0]);write("quads.blades",blades[1]);
        write("ground.depth",before);write("grass.depth",after);
        metadata["inspection"]={{"buffer_reads",bufferReads},{"pixel_reads",pixelReads},{"read_bytes",bytes},
            {"blocking",true},{"timing_acceptance",false},{"encoding","little-endian IEEE754 float32, OpenGL bottom row first"}};
        std::ofstream out(folder/"snapshot.json.tmp");out<<metadata.dump(2)<<'\n';out.close();
        if(!out) throw std::runtime_error("Cannot write coverage metadata");
        std::filesystem::rename(folder/"snapshot.json.tmp",folder/"snapshot.json");
        completed=request;queue=-1;bufferReads=pixelReads=bytes=0;
    }
};
}

#pragma once
#include <algorithm>
#include <filesystem>
#include <numeric>

namespace order {
struct Blade {glm::vec4 rootFade,upLow,variation,wind;};
static_assert(sizeof(Blade)==64);
inline PFNGLDRAWARRAYSINDIRECTPROC original=nullptr;
inline std::uint64_t token=0,iteration=0;
inline json snapshot;
inline json lastInspection;
inline std::filesystem::path folder;

template<class T> void write(const std::filesystem::path& path,const std::vector<T>& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(data.data()),data.size()*sizeof(T));
    if(!output) throw std::runtime_error("Cannot write order inspection payload");
}
inline glm::mat4 matrix(GLuint program,const char* name) {
    glm::mat4 result;glGetUniformfv(program,glGetUniformLocation(program,name),glm::value_ptr(result));return result;
}
inline void depth(const std::filesystem::path& path) {
    GLint framebuffer=0,readFramebuffer=0,pack=0,viewport[4];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&framebuffer);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFramebuffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);glGetIntegerv(GL_VIEWPORT,viewport);
    if(viewport[0] || viewport[1] || viewport[2]<=0 || viewport[3]<=0) throw std::runtime_error("Unsupported order viewport");
    glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer);glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
    std::vector<float> pixels(std::size_t(viewport[2])*viewport[3]);
    glReadPixels(0,0,viewport[2],viewport[3],GL_DEPTH_COMPONENT,GL_FLOAT,pixels.data());
    glBindBuffer(GL_PIXEL_PACK_BUFFER,pack);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFramebuffer);write(path,pixels);
    snapshot["depth_viewports"][path.parent_path().filename().string()]={viewport[2],viewport[3]};
    snapshot["inspection"]["pixel_reads"]=snapshot["inspection"]["pixel_reads"].get<unsigned>()+1;
}
inline void GLAPIENTRY draw(GLenum mode,const void* offset) {
    const auto request=controls.value("order_request",0ull);
    if(!observing || !raster::active || raster::active->at("body")!=0 || !request) {original(mode,offset);return;}
    if(request!=token) {token=request;iteration=0;}
    if(iteration>=controls.value("order_frames",3u)) {original(mode,offset);return;}
    const auto q=reinterpret_cast<std::uintptr_t>(offset)/16;
    if(q>1 || reinterpret_cast<std::uintptr_t>(offset)%16 || mode!=GL_TRIANGLE_STRIP)
        throw std::runtime_error("Invalid order command");
    const auto view=raster::active->at("view").get<std::string>();
    if(snapshot.is_null()) {
        const auto* output=std::getenv("PLANET_NATIVE_ORDER");if(!output) throw std::runtime_error("Missing order output folder");
        folder=std::filesystem::path(output)/std::to_string(token)/std::to_string(iteration);
        std::filesystem::create_directories(folder);
        auto& r=rendering::RendererRecoveryProbe::state(*renderer);
        snapshot={{"request",token},{"iteration",iteration},{"controls",controls},{"profile_frame",r.profiler.nextFrameNumber()-1},
            {"publication_before",r.terrainPublicationState()},{"workload",benchmarkObservation(r,glfwGetCurrentContext())},
            {"depth_scope","opaque before/after the two grass queues; later character/water/atmosphere excluded"},
            {"inspection",{{"blocking",true},{"timing_acceptance",false},{"buffer_reads",0},{"pixel_reads",0},{"query_reads",0},{"buffer_read_bytes",0},{"buffer_upload_bytes",0}}},
            {"events",json::array()}};
    }
    GLint indirect=0,copy=0,writeBuffer=0,blade=0;void* address=nullptr;
    glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING,&indirect);glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&copy);
    glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING,&writeBuffer);glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&blade);
    glGetVertexAttribPointerv(0,GL_VERTEX_ATTRIB_ARRAY_POINTER,&address);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    std::array<unsigned,4> command{};glBindBuffer(GL_COPY_READ_BUFFER,indirect);bufferRead(GL_COPY_READ_BUFFER,q*16,16,command.data());
    if(command[0]!=(q==0 ? 14u : 4u) || command[2] || command[3]) throw std::runtime_error("Unexpected order command fields");
    glBindBuffer(GL_COPY_READ_BUFFER,blade);GLint64 bytes=0;glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&bytes);
    const auto start=reinterpret_cast<std::uintptr_t>(address);
    if(bytes<0 || bytes%128 || start!=q*std::uint64_t(bytes)/2 || command[1]>std::uint64_t(bytes)/128)
        throw std::runtime_error("Invalid order queue capacity");
    std::vector<Blade> input(command[1]);
    if(!input.empty()) bufferRead(GL_COPY_READ_BUFFER,start,input.size()*64,input.data());
    glBindBuffer(GL_COPY_READ_BUFFER,copy);
    const auto program=raster::active->at("blade_program").get<GLuint>();
    const auto model=matrix(program,"model"),projection=matrix(program,"projection");
    const auto transform=matrix(program,"view")*model;
    const auto control=controls.value("order_mode",std::string("native"));
    if(control!="native" && control!="near" && control!="far") throw std::runtime_error("Invalid order permutation");
    std::vector<std::size_t> indices(input.size());std::iota(indices.begin(),indices.end(),0);
    const auto distance=[&](std::size_t i) {return -(transform*glm::vec4(glm::vec3(input[i].rootFade),1)).z;};
    std::vector<float> depths;depths.reserve(input.size());
    for(std::size_t i=0;i<input.size();++i) {
        depths.push_back(distance(i));if(!std::isfinite(depths.back())) throw std::runtime_error("Nonfinite order depth");
    }
    if(control!="native") std::stable_sort(indices.begin(),indices.end(),[&](auto a,auto b) {
        return control=="near" ? depths[a]<depths[b] : depths[a]>depths[b];
    });
    std::vector<Blade> permuted;permuted.reserve(input.size());for(auto i:indices) permuted.push_back(input[i]);
    const auto path=folder/view;const auto name=q==0 ? "detailed" : "quads";
    write(path/(std::string(name)+".input"),input);write(path/(std::string(name)+".permuted"),permuted);
    write(path/(std::string(name)+".depth-keys"),depths);
    std::vector<unsigned> permutation(indices.begin(),indices.end());write(path/(std::string(name)+".indices"),permutation);
    if(q==0) depth(path/"before.depth");
    // Equal roundtrips in native and sorted controls; all uploads are excluded.
    glBindBuffer(GL_COPY_WRITE_BUFFER,blade);
    if(!permuted.empty()) glBufferSubData(GL_COPY_WRITE_BUFFER,start,permuted.size()*64,permuted.data());
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
    GLuint queries[3];glGenQueries(3,queries);
    glBeginQuery(GL_PRIMITIVES_GENERATED,queries[0]);glBeginQuery(GL_SAMPLES_PASSED,queries[1]);glBeginQuery(GL_TIME_ELAPSED,queries[2]);
    original(mode,offset);
    glEndQuery(GL_TIME_ELAPSED);glEndQuery(GL_SAMPLES_PASSED);glEndQuery(GL_PRIMITIVES_GENERATED);
    std::array<GLuint64,3> results{};
    for(int i=0;i<3;++i) glGetQueryObjectui64v(queries[i],GL_QUERY_RESULT,&results[i]);glDeleteQueries(3,queries);
    if(q==1) depth(path/"after.depth");
    if(!input.empty()) glBufferSubData(GL_COPY_WRITE_BUFFER,start,input.size()*64,input.data());
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
    glBindBuffer(GL_COPY_WRITE_BUFFER,writeBuffer);
    snapshot["events"].push_back({{"view",view},{"queue",q},{"command",command},{"mode",control},
        {"model",std::vector<float>(glm::value_ptr(model),glm::value_ptr(model)+16)},
        {"projection",std::vector<float>(glm::value_ptr(projection),glm::value_ptr(projection)+16)},
        {"view_model",std::vector<float>(glm::value_ptr(transform),glm::value_ptr(transform)+16)},
        {"wind_s",raster::active->at("effective_time_s")},{"generated_primitives",results[0]},
        {"passed_samples",results[1]},{"draw_gpu_ns",results[2]},{"source_buffer_bytes",bytes}});
    snapshot["inspection"]["buffer_reads"]=snapshot["inspection"]["buffer_reads"].get<unsigned>()+1+unsigned(!input.empty());
    snapshot["inspection"]["query_reads"]=snapshot["inspection"]["query_reads"].get<unsigned>()+3;
    snapshot["inspection"]["buffer_read_bytes"]=snapshot["inspection"]["buffer_read_bytes"].get<std::uint64_t>()+16+input.size()*64;
    snapshot["inspection"]["buffer_upload_bytes"]=snapshot["inspection"]["buffer_upload_bytes"].get<std::uint64_t>()+input.size()*128;
}
inline void install() {if(!original && raster::useProgram) {original=__glewDrawArraysIndirect;__glewDrawArraysIndirect=draw;}}
inline void finish(GLFWwindow* window) {
    if(snapshot.is_null()) return;
    if(snapshot["events"].size()!=4) throw std::runtime_error("Incomplete main/reflection order snapshot");
    GLint readFramebuffer=0,pack=0,readBuffer=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFramebuffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
    glGetIntegerv(GL_READ_BUFFER,&readBuffer);glReadBuffer(GL_BACK);glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
    int w=0,h=0;glfwGetFramebufferSize(window,&w,&h);std::vector<unsigned char> rgba(std::size_t(w)*h*4);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());write(folder/"display.rgba",rgba);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,pack);glReadBuffer(readBuffer);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFramebuffer);
    snapshot["viewport"]={w,h};snapshot["inspection"]["pixel_reads"]=snapshot["inspection"]["pixel_reads"].get<unsigned>()+1;
    auto& r=rendering::RendererRecoveryProbe::state(*renderer);
    snapshot["publication_after"]=r.terrainPublicationState();snapshot["workload_after"]=benchmarkObservation(r,window);
    std::ofstream output(folder/"snapshot.json.tmp");output<<snapshot.dump(2)<<'\n';output.close();
    if(!output) throw std::runtime_error("Cannot publish order metadata");
    std::filesystem::rename(folder/"snapshot.json.tmp",folder/"snapshot.json");
    lastInspection=snapshot["inspection"];snapshot=nullptr;++iteration;
}
}

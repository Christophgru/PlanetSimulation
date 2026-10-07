#pragma once
#include "Target.h"
#include <algorithm>
#include "biomes/Render.h"
namespace coverage {
template<class T> void rows(std::ofstream& out,const std::vector<T>& pixels,int x,int y,int tw,int th,int width,int channels) {
    for(int row=0;row<th;++row) {
        out.seekp((std::uint64_t(y+row)*width+x)*channels*sizeof(T));
        out.write(reinterpret_cast<const char*>(pixels.data()+std::size_t(row)*tw*channels),tw*channels*sizeof(T));
    }
    if(!out) throw std::runtime_error("Live area map write failed");
}
template<class State> json renderLive(State& r,const std::filesystem::path& folder,const json& common) {
    const int edge=256,w=common.at("viewport").at(0),h=common.at("viewport").at(1);
    GLint limit[2]{};glGetIntegerv(GL_MAX_VIEWPORT_DIMS,limit);
    if(w<1 || h<1 || w*4>limit[0] || h*4>limit[1]) throw std::runtime_error("Live sampling exceeds viewport limits");
    const auto view=matrix(common.at("view")),projection=matrix(common.at("projection"));
    const auto& p=r.scene.scenario.planets[0];const auto& body=r.scene.bodies[1];
    const double scale=p.radius*r.scene.scenario.metersPerWorldUnit();
    const auto root=vector(common.at("astronaut").at("root"))/scale;
    const auto rock=p.terrain_material.slopeMetricRange();double maximum=p.terrain_landscape.maximumAbsoluteHeightMeters();
    for(const auto& noise:p.surface_noise) maximum+=noise.amplitude_m;
    json parameters={{"root_body",{float(root.x),float(root.y),float(root.z)}},{"meters_per_radius",scale},
        {"planet_color",p.color},{"landscape",p.terrain_landscape.enabled},{"water",p.water.enabled},
        {"landscape_levels",{p.water.enabled ? p.water.level_m : 0,.1,maximum}},
        {"rock_range",rock},{"green_ratio",p.foliage.green_ratio},{"water_clearance_m",p.foliage.water_clearance_m}};
    rendering::OwnedShader ground("tests/app/terrain/native/coverage/live/ground.vert","tests/app/terrain/native/coverage/live/ground.frag");
    rendering::OwnedShader mask("tests/app/terrain/native/coverage/live/mask.vert","tests/app/terrain/native/coverage/live/mask.frag");
    LiveTarget target(edge);
    struct RestoreScissor {
        GLboolean enabled=glIsEnabled(GL_SCISSOR_TEST),colors[4]{};GLint box[4]{};
        RestoreScissor() {glGetIntegerv(GL_SCISSOR_BOX,box);glGetBooleanv(GL_COLOR_WRITEMASK,colors);}
        ~RestoreScissor() {glScissor(box[0],box[1],box[2],box[3]);if(!enabled) glDisable(GL_SCISSOR_TEST);
            glColorMask(colors[0],colors[1],colors[2],colors[3]);glDisable(GL_STENCIL_TEST);}
    } restore;
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
    glEnable(GL_SCISSOR_TEST);glEnable(GL_STENCIL_TEST);glStencilMask(0xff);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
    json samples=json::array();
    for(int factor:{1,2,4}) {
        const int width=w*factor,height=h*factor;
        const auto base=folder/"live"/(std::to_string(factor)+"x");std::filesystem::create_directories(base);
        std::ofstream maps[4];const char* names[]={"eligible.rgba32f","plane.rgba32f","normal-height.rgba32f","material.rgba32f"};
        const int count=factor==1 ? 4 : 2;
        for(int i=0;i<count;++i) maps[i].open(base/names[i],std::ios::binary);
        std::ofstream tags(base/"objects.stencil",std::ios::binary);unsigned tiles=0;
        for(int y=0;y<height;y+=edge) for(int x=0;x<width;x+=edge) {
            const int tw=std::min(edge,width-x),th=std::min(edge,height-y);
            glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glViewport(-x,-y,width,height);glScissor(0,0,tw,th);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);const GLfloat clear[4]{};
            for(int i=0;i<4;++i) glClearBufferfv(GL_COLOR,i,clear);
            glClearStencil(0);glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
            glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
            mask.use();mask.setMat4("view",glm::value_ptr(view));mask.setMat4("projection",glm::value_ptr(projection));
            glStencilFunc(GL_ALWAYS,1,0xff);
            const auto sunModel=rendering::sphereModel(glm::vec3(r.scene.bodies[0].position),float(r.scene.scenario.sun.radius));
            mask.setMat4("model",glm::value_ptr(sunModel));r.meshes.sunMesh.draw();
            glStencilFunc(GL_ALWAYS,3,0xff);
            for(std::size_t i=1;i<r.scene.scenario.planets.size();++i) {
                const auto model=rendering::sphereModel(glm::vec3(r.scene.bodies[i+1].position),float(r.scene.scenario.planets[i].radius),glm::mat3(r.scene.bodies[i+1].orientation));
                mask.setMat4("model",glm::value_ptr(model));r.meshes.planetMeshes[i].draw();
            }
            glStencilFunc(GL_ALWAYS,4,0xff);
            r.astronaut.shader.use();r.astronaut.shader.setFloat("uClipRadius",-1);
            r.astronaut.draw(body.position,body.orientation,p.radius,r.scene.scenario.metersPerWorldUnit(),view,projection);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glStencilFunc(GL_ALWAYS,2,0xff);
            ground.use();ground.setMat4("view",glm::value_ptr(view));ground.setMat4("projection",glm::value_ptr(projection));
            ground.setMat4("model",glm::value_ptr(matrix(common.at("model"))));
            ground.setFloat3("rootBody",root.x,root.y,root.z);ground.setFloat("scale",scale);
            ground.setFloat3("planetColor",p.color[0],p.color[1],p.color[2]);ground.setFloat2("rockRange",rock[0],rock[1]);
            ground.setFloat3("landscapeLevels",p.water.enabled ? p.water.level_m : 0,.1,maximum);
            ground.setInt("landscape",p.terrain_landscape.enabled);ground.setInt("water",p.water.enabled);
            ground.setFloat("waterClearance",p.foliage.water_clearance_m);ground.setFloat("greenRatio",p.foliage.green_ratio);
            r.meshes.planetMeshes[0].draw();
            std::vector<float> pixels(std::size_t(tw)*th*4);
            for(int i=0;i<count;++i) {
                glReadBuffer(GL_COLOR_ATTACHMENT0+i);glReadPixels(0,0,tw,th,GL_RGBA,GL_FLOAT,pixels.data());rows(maps[i],pixels,x,y,tw,th,width,4);
            }
            std::vector<std::uint8_t> stencil(std::size_t(tw)*th);
            glReadPixels(0,0,tw,th,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,stencil.data());rows(tags,stencil,x,y,tw,th,width,1);++tiles;
        }
        for(int i=0;i<count;++i) maps[i].close();tags.close();
        if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Live tiled inspection GL error");
        samples.push_back({{"factor",factor},{"viewport",{width,height}},{"tiles",tiles},{"pixel_reads",(count+1)*tiles},
            {"pixel_bytes",std::uint64_t(width)*height*(count*16+1)}});
    }
    const auto biomes=biomeSamples(r,ground,target,folder,parameters,w,h);
    return {{"schema",1},{"parameters",parameters},{"samples",samples},{"biomes",biomes},{"tile_edge",edge},
        {"gpu_target_bytes",edge*edge*(4*16+4)},{"cpu_read_tile_bytes",edge*edge*(16+1)},
        {"tile_method","Unchanged full projection, translated viewport and bounded scissor"},
        {"occlusion","Actual Sun/other-body land VAOs and full procedural astronaut meshes at each sampling grid"},
        {"composed_mask","Native display grid; color contribution proxy, with separate edge-area bound"},
        {"native_tag_mismatch_tolerance",.0001},{"biome_retention_tolerance",.002},{"convergence_tolerance",.01},{"timing_acceptance",false},{"coverage_acceptance",false}};
}
}

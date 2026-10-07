#pragma once
namespace coverage {
inline void biomeUniforms(rendering::OwnedShader& shader,const json& p) {
    const auto color=p.at("planet_color"),levels=p.at("landscape_levels"),rock=p.at("rock_range");
    shader.setFloat3("planetColor",color.at(0),color.at(1),color.at(2));
    shader.setFloat3("landscapeLevels",levels.at(0),levels.at(1),levels.at(2));
    shader.setFloat2("rockRange",rock.at(0),rock.at(1));
    shader.setInt("landscape",p.at("landscape"));shader.setInt("water",p.at("water"));
    shader.setFloat("waterClearance",p.at("water_clearance_m"));shader.setFloat("greenRatio",p.at("green_ratio"));
}
// Uniform-only diagnostic variants on an unchanged live land VAO. They are
// separate from all native-parameter area samples and do not alter scene config.
template<class State> json biomeSamples(State& r,rendering::OwnedShader& shader,LiveTarget& target,
        const std::filesystem::path& folder,const json& parameters,int w,int h) {
    const int tw=std::min(256,w),th=std::min(256,h),x=(w-tw)/2;
    const auto base=folder/"live/biomes";std::filesystem::create_directories(base);
    json variants=json::array();
    for(const char* name:{"water-pass","water-reject","green-pass","green-reject","meadow","beach","snow","rock-half","beach-edge"}) {
        json p=parameters;p["landscape"]=false;p["water"]=false;p["rock_range"]={2,3};p["green_ratio"]=.1;
        const std::string test=name;
        if(test=="water-pass" || test=="water-reject") {p["water"]=true;p["landscape_levels"]={test=="water-pass" ? -10000 : 10000,.1,20000};}
        if(test=="green-reject") p["green_ratio"]=100;
        if(test=="meadow" || test=="beach" || test=="snow" || test=="beach-edge") {
            p["landscape"]=true;p["planet_color"]={.2,.4,1};p["green_ratio"]=1.15;
            p["landscape_levels"]=test=="meadow" ? json{-100,.1,10000} :
                test=="beach" ? json{100,.1,10000} : test=="snow" ? json{-10000,.1,1} : json{-.2,.1,1000};
        }
        if(test=="rock-half") p["rock_range"]={-1,1};
        glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glViewport(-x,0,w,h);glScissor(0,0,tw,th);
        const GLfloat clear[4]{};for(int i=0;i<4;++i) glClearBufferfv(GL_COLOR,i,clear);
        glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);glStencilFunc(GL_ALWAYS,2,0xff);
        shader.use();biomeUniforms(shader,p);r.meshes.planetMeshes[0].draw();
        const auto path=base/test;std::filesystem::create_directories(path);
        const char* names[]={"position.rgba32f","normal-height.rgba32f","material.rgba32f"};
        int attachments[]={0,2,3};std::vector<float> values(std::size_t(tw)*th*4);
        for(int i=0;i<3;++i) {
            glReadBuffer(GL_COLOR_ATTACHMENT0+attachments[i]);glReadPixels(0,0,tw,th,GL_RGBA,GL_FLOAT,values.data());
            std::ofstream out(path/names[i],std::ios::binary);out.write(reinterpret_cast<const char*>(values.data()),values.size()*sizeof(float));
            if(!out) throw std::runtime_error("Cannot write biome diagnostic");
        }
        variants.push_back({{"name",test},{"parameters",p},{"viewport",{tw,th}},{"native_pixel_origin",{x,0}},
            {"pixel_reads",3},{"pixel_bytes",std::uint64_t(tw)*th*48}});
    }
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Biome diagnostic GL error");
    return {{"scope","Uniform-only predicate diagnostics on installed native land; excluded from area/coverage samples"},{"cases",variants}};
}
}

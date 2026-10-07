#include "Target.h"
#include "rendering/runtime/ResourceOwners.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
using json=nlohmann::json;

glm::mat4 matrix(const json& values) {
    if(values.size()!=16) throw std::invalid_argument("Expected 16 matrix elements");
    glm::mat4 result;for(int i=0;i<16;++i) glm::value_ptr(result)[i]=values.at(i);return result;
}
void write(const std::filesystem::path& path,const std::vector<float>& values) {
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(values.data()),values.size()*sizeof(float));
    if(!out) throw std::runtime_error("Area map write failed");
}
std::string contextUuid() {
    if(!GLEW_EXT_memory_object || !__glewGetUnsignedBytei_vEXT) return "unavailable";
    GLint count=0;glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT,&count);
    if(count!=1) return "ambiguous";
    GLubyte bytes[GL_UUID_SIZE_EXT]{};glGetUnsignedBytei_vEXT(GL_DEVICE_UUID_EXT,0,bytes);
    std::ostringstream out;out<<"GPU-"<<std::hex<<std::setfill('0');
    for(int i=0;i<GL_UUID_SIZE_EXT;++i) {if(i==4 || i==6 || i==8 || i==10) out<<'-';out<<std::setw(2)<<unsigned(bytes[i]);}
    return out.str();
}
void inspect(const json& input,const std::filesystem::path& folder,int factor,int edge) {
    const int width=input.at("viewport").at(0).get<int>()*factor,height=input.at("viewport").at(1).get<int>()*factor;
    if(width<1 || height<1 || width>8192 || height>8192 || edge<16 || edge>512)
        throw std::invalid_argument("Area dimensions exceed qualification limits");
    std::filesystem::create_directories(folder);
    const auto vertices=input.at("vertices").get<std::vector<float>>();
    if(vertices.empty() || vertices.size()%27) throw std::invalid_argument("Expected position/normal/color triangles");
    rendering::VertexArray vao;GLuint vbo=0;glGenBuffers(1,&vbo);glBindVertexArray(vao.id);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STATIC_DRAW);
    for(int i=0;i<3;++i) {glEnableVertexAttribArray(i);glVertexAttribPointer(i,3,GL_FLOAT,GL_FALSE,9*sizeof(float),reinterpret_cast<void*>(i*3*sizeof(float)));}
    rendering::OwnedShader shader("tests/app/terrain/native/coverage/matched/ground.vert","tests/app/terrain/native/coverage/matched/ground.frag");
    GLint linked=0;glGetProgramiv(shader.id,GL_LINK_STATUS,&linked);if(!linked) throw std::runtime_error("Area shader link failed");
    shader.use();shader.setMat4("model",glm::value_ptr(matrix(input.at("model"))));
    shader.setMat4("view",glm::value_ptr(matrix(input.at("view"))));
    const auto root=input.at("root_body");shader.setFloat3("rootBody",root.at(0),root.at(1),root.at(2));
    shader.setFloat("scale",input.at("meters_per_radius"));shader.setFloat3("planetColor",1,1,1);
    shader.setInt("landscape",0);shader.setInt("water",0);shader.setFloat("greenRatio",1);
    shader.setFloat2("rockRange",2,3); // Constant eligible green surface; red foreground occludes it.
    coverage::AreaTarget target(edge);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);
    std::vector<float> maps[2];for(auto& map:maps) map.resize(std::size_t(width)*height*4);
    const auto projection=matrix(input.at("projection"));shader.setMat4("projection",glm::value_ptr(projection));
    GLint viewportLimit[2]{};glGetIntegerv(GL_MAX_VIEWPORT_DIMS,viewportLimit);
    if(width>viewportLimit[0] || height>viewportLimit[1]) throw std::runtime_error("Sampling exceeds viewport limits");
    glEnable(GL_SCISSOR_TEST);unsigned tiles=0;
    for(int y=0;y<height;y+=edge) for(int x=0;x<width;x+=edge) {
        const int tw=std::min(edge,width-x),th=std::min(edge,height-y);
        // Keep one projection and translate the full-size viewport instead.
        // Per-tile off-axis matrix rounding can change edge pixel membership.
        glViewport(-x,-y,width,height);glScissor(0,0,tw,th);
        const GLfloat clear[4]{};glClearBufferfv(GL_COLOR,0,clear);glClearBufferfv(GL_COLOR,1,clear);glClear(GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES,0,vertices.size()/9);
        std::vector<float> pixels(std::size_t(tw)*th*4);
        for(int attachment=0;attachment<2;++attachment) {
            glReadBuffer(GL_COLOR_ATTACHMENT0+attachment);glReadPixels(0,0,tw,th,GL_RGBA,GL_FLOAT,pixels.data());
            for(int row=0;row<th;++row) std::copy_n(pixels.data()+std::size_t(row)*tw*4,tw*4,
                maps[attachment].data()+(std::size_t(y+row)*width+x)*4);
        }
        ++tiles;
    }
    const auto error=glGetError();if(error!=GL_NO_ERROR) throw std::runtime_error("Area GL error "+std::to_string(error));
    write(folder/"eligible.rgba32f",maps[0]);write(folder/"plane.rgba32f",maps[1]);glDeleteBuffers(1,&vbo);
    json receipt={{"schema",1},{"viewport",{width,height}},{"sampling_factor",factor},{"tile_edge",edge},{"tiles",tiles},
        {"gpu_target_bytes",std::uint64_t(edge)*edge*(2*16+4)},{"pixel_reads",2*tiles},
        {"pixel_bytes",std::uint64_t(width)*height*32},{"vertex_bytes",vertices.size()*4},
        {"context_uuid",contextUuid()},{"renderer",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
        {"tile_method","Unchanged full projection with translated viewport and bounded scissor"},
        {"version",reinterpret_cast<const char*>(glGetString(GL_VERSION))},{"timing_acceptance",false},{"coverage_acceptance",false}};
    std::ofstream out(folder/"receipt.json");out<<receipt.dump(2)<<'\n';if(!out) throw std::runtime_error("Area receipt write failed");
}
int main(int argc,char** argv) {
    if(argc!=5) {std::cerr<<"Usage: terrain_area_probe scene.json output factor tile-edge\n";return 2;}
    if(!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(64,64,"Area qualification",nullptr,nullptr);
    if(!window) {glfwTerminate();return 1;}
    glfwMakeContextCurrent(window);glewExperimental=GL_TRUE;
    int result=0;
    try {
        if(glewInit()!=GLEW_OK) throw std::runtime_error("GLEW initialization failed");
        while(glGetError()!=GL_NO_ERROR) {}
        json input;std::ifstream source(argv[1]);source>>input;
        inspect(input,argv[2],std::stoi(argv[3]),std::stoi(argv[4]));
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';result=1;}
    glfwDestroyWindow(window);glfwTerminate();return result;
}

#include "rendering/character/assets/AcesSuit.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <bit>
#include <cstring>
#include <fstream>

namespace rendering {
namespace {
void require(bool valid) { if (!valid) throw std::runtime_error("Invalid NASA ACES suit asset"); }
void read(std::istream& stream,void* data,std::size_t bytes) {
    require(bool(stream.read(static_cast<char*>(data),static_cast<std::streamsize>(bytes))));
}
bool finite(glm::vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
glm::dmat4 translate(glm::dvec3 v) { return glm::translate(glm::dmat4(1),v); }
glm::dmat4 boneTransform(const SuitBone& bind,glm::dvec3 head,glm::dvec3 tail) {
    const auto from=glm::normalize(glm::dvec3(bind.tail-bind.head)),to=glm::normalize(tail-head);
    const double cosine=glm::clamp(glm::dot(from,to),-1.0,1.0);
    glm::dquat rotation;
    if (cosine < -1+1e-10) {
        const auto axis=glm::normalize(glm::cross(from,std::abs(from.x)<.9 ? glm::dvec3(1,0,0) : glm::dvec3(0,1,0)));
        rotation=glm::angleAxis(std::acos(-1.0),axis);
    } else {
        const auto axis=glm::cross(from,to);
        rotation=glm::normalize(glm::dquat(1+cosine,axis.x,axis.y,axis.z));
    }
    return translate(head)*glm::mat4_cast(rotation)*translate(-glm::dvec3(bind.head));
}
}
AcesSuit AcesSuit::load(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open NASA ACES suit: "+path.string());
    return load(stream);
}
AcesSuit AcesSuit::load(std::istream& stream) {
    static_assert(std::endian::native==std::endian::little,"Suit deployment format requires little endian");
    char magic[8]; std::uint32_t header[4]; float offset[3];
    read(stream,magic,sizeof(magic)); read(stream,header,sizeof(header)); read(stream,offset,sizeof(offset));
    require(std::memcmp(magic,"ACESSUIT",8)==0 && header[0]==1 && header[3]==BoneCount);
    // Bound allocation before reading externally supplied counts.
    require(header[1]>0 && header[1]<=500000 && header[2]>0 && header[2]<=1500000 && header[2]%3==0);
    AcesSuit suit;
    suit.bindBodyOffset={offset[0],offset[1],offset[2]};
    require(finite(suit.bindBodyOffset));
    for (auto& bone:suit.bones) {
        float points[6]; read(stream,points,sizeof(points));
        bone={{points[0],points[1],points[2]},{points[3],points[4],points[5]}};
        require(finite(bone.head)&&finite(bone.tail)&&glm::length(bone.tail-bone.head)>1e-4f);
    }
    suit.vertices.resize(header[1]); suit.indices.resize(header[2]);
    read(stream,suit.vertices.data(),suit.vertices.size()*sizeof(SuitVertex));
    read(stream,suit.indices.data(),suit.indices.size()*sizeof(std::uint32_t));
    require(stream.peek()==std::char_traits<char>::eof());
    for (const auto& v:suit.vertices) {
        for (float p:v.position) require(std::isfinite(p)&&std::abs(p)<10);
        float length=0; for (float n:v.normal) { require(std::isfinite(n)); length+=n*n; }
        require(length>.99f && length<1.01f);
        for (float c:v.color) require(std::isfinite(c)&&c>=0&&c<=1);
        for (auto j:v.joints) require(j<BoneCount);
        for (float w:v.weights) require(std::isfinite(w)&&w>=0&&w<=1);
        require(std::abs(v.weights[0]+v.weights[1]-1)<1e-5f);
    }
    for (auto index:suit.indices) require(index<suit.vertices.size());
    return suit;
}
std::array<glm::mat4,AcesSuit::BoneCount> AcesSuit::palette(const AstronautPose& pose) const {
    std::array<glm::mat4,BoneCount> matrices;
    const auto inverse=glm::transpose(pose.suitBasis());
    const auto body=translate(pose.bodyOffset-glm::dvec3(bindBodyOffset));
    matrices[0]=body;
    for (int leg=0;leg<2;++leg) {
        const int base=1+3*leg;
        const auto hip=inverse*(pose.hips[leg]-pose.root);
        const auto knee=inverse*(pose.knees[leg]-pose.root);
        const auto ankle=inverse*(pose.ankles[leg]-pose.root);
        matrices[base]=boneTransform(bones[base],hip,knee);
        matrices[base+1]=boneTransform(bones[base+1],knee,ankle);
        const auto up=pose.feet[leg].contact.normal;
        auto forward=pose.feet[leg].forward-up*glm::dot(pose.feet[leg].forward,up);
        if (glm::length(forward)<1e-8) forward=pose.forward-up*glm::dot(pose.forward,up);
        forward=glm::normalize(forward);
        const glm::dmat3 footBasis(glm::normalize(glm::cross(forward,up)),up,-forward);
        matrices[base+2]=translate(ankle)*glm::dmat4(inverse*footBasis)*translate(-glm::dvec3(bones[base+2].head));
        const int arm=7+3*leg;
        const double sign=leg==0 ? -1 : 1;
        const auto pivot=glm::dvec3(bones[arm].head);
        const auto swing=body*translate(pivot)*glm::rotate(glm::dmat4(1),sign*pose.armSwing*2.2,glm::dvec3(1,0,0))*translate(-pivot);
        matrices[arm]=swing;
        const auto elbow=glm::dvec3(bones[arm+1].head);
        const auto flex=swing*translate(elbow)*glm::rotate(glm::dmat4(1),std::abs(pose.armSwing)*.7,glm::dvec3(1,0,0))*translate(-elbow);
        matrices[arm+1]=matrices[arm+2]=flex;
    }
    return matrices;
}
}

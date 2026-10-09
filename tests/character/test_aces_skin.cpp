#include "rendering/character/assets/AcesSuit.h"
#include <gtest/gtest.h>
#include <sstream>
#include <fstream>
#include <cstring>

namespace {
rendering::GroundContact ground(const glm::dvec3& p) { return {{p.x,1000,p.z},{0,1,0}}; }
glm::vec3 transform(const glm::mat4& m,const glm::vec3& p) { return glm::vec3(m*glm::vec4(p,1)); }
glm::vec3 vertex(const rendering::SuitVertex& v,const std::array<glm::mat4,13>& palette) {
    const glm::vec3 p(v.position[0],v.position[1],v.position[2]);
    return transform(palette[v.joints[0]],p)*v.weights[0]+transform(palette[v.joints[1]],p)*v.weights[1];
}
}
TEST(AcesSuit, OriginalGeometryAndStandingBindPose) {
    const auto suit=rendering::AcesSuit::load("data/character/aces/suit.bin");
    EXPECT_EQ(suit.vertices.size(),110033u); EXPECT_EQ(suit.indices.size()/3,88872u);
    rendering::AstronautMotion motion;
    motion.update(ground({0,1000,0}),{0,0,-1},0,ground);
    const auto palette=suit.palette(motion.pose());
    for (const auto& v:suit.vertices) {
        const glm::vec3 p(v.position[0],v.position[1],v.position[2]);
        ASSERT_LT(glm::length(vertex(v,palette)-p),2e-4f);
    }
}
TEST(AcesSuit, WalkingSkinReachesIKJointsAndMovesTheOriginalVertices) {
    const auto suit=rendering::AcesSuit::load("data/character/aces/suit.bin");
    rendering::AstronautMotion motion;
    for (int i=0;i<9;++i) motion.update(ground({0,1000,-.06*i}),{0,0,-1},.04,ground);
    const auto& pose=motion.pose(); const auto palette=suit.palette(pose);
    const auto inverse=glm::transpose(pose.suitBasis());
    for (int leg=0;leg<2;++leg) {
        const int base=1+3*leg;
        const glm::vec3 hip=inverse*(pose.hips[leg]-pose.root), knee=inverse*(pose.knees[leg]-pose.root), ankle=inverse*(pose.ankles[leg]-pose.root);
        EXPECT_LT(glm::length(transform(palette[base],suit.bones[base].head)-hip),1e-5f);
        EXPECT_LT(glm::length(transform(palette[base],suit.bones[base].tail)-knee),1e-5f);
        EXPECT_LT(glm::length(transform(palette[base+1],suit.bones[base+1].tail)-ankle),1e-5f);
        const glm::vec3 sole=suit.bones[base+2].head-glm::vec3(0,.09,0);
        const glm::vec3 contact=inverse*(pose.feet[leg].contact.position-pose.root);
        EXPECT_LT(glm::length(transform(palette[base+2],sole)-contact),1e-5f);
    }
    std::size_t moved=0;
    for (const auto& v:suit.vertices) {
        const glm::vec3 p(v.position[0],v.position[1],v.position[2]), animated=vertex(v,palette);
        ASSERT_TRUE(std::isfinite(glm::length(animated)));
        ASSERT_LT(glm::length(animated-p),1.5f);
        if (glm::length(animated-p)>.01f) ++moved;
    }
    EXPECT_GT(moved,suit.vertices.size()/2);
}
TEST(AcesSuit, RejectsMalformedAndTruncatedDeploymentAssets) {
    std::ifstream input("data/character/aces/suit.bin",std::ios::binary);
    const std::string good((std::istreambuf_iterator<char>(input)),{});
    ASSERT_FALSE(good.empty());
    for (std::size_t size:{0u,8u,36u,400u}) {
        std::istringstream stream(good.substr(0,size));
        EXPECT_THROW(rendering::AcesSuit::load(stream),std::runtime_error);
    }
    for (std::size_t offset:{0u,8u,12u,20u}) {
        auto broken=good; broken[offset]=char(255); std::istringstream stream(broken);
        EXPECT_THROW(rendering::AcesSuit::load(stream),std::runtime_error);
    }
    auto badIndex=good; const std::uint32_t invalid=0xffffffffu;
    std::memcpy(badIndex.data()+badIndex.size()-4,&invalid,4);
    std::istringstream indexStream(badIndex);
    EXPECT_THROW(rendering::AcesSuit::load(indexStream),std::runtime_error);
    auto badJoint=good; badJoint[36+13*24+40]=char(255);
    std::istringstream jointStream(badJoint);
    EXPECT_THROW(rendering::AcesSuit::load(jointStream),std::runtime_error);
    std::istringstream trailing(good+"unexpected");
    EXPECT_THROW(rendering::AcesSuit::load(trailing),std::runtime_error);
}

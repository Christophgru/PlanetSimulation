#include "rendering/foliage/GrassLod.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <set>

TEST(GrassLod, EightContiguousSortedBatchesRetainAttributesAndReduceVertices) {
    std::vector<rendering::GrassBlade> blades;
    const std::array<float,8> distances{3,8,10,12,14,22,37,52};
    for (int level=7; level>=0; --level) for (int seed=0; seed<256; ++seed) {
        const float distance=distances[level];
        blades.push_back({{distance/100,seed*.000001f,1},{0,0,1},{.3f,.2f,1,seed/256.f}});
    }
    const auto plan=rendering::batchGrass(blades,{0,0,1},100,60,0);
    std::size_t next=0, vertices=0;
    for (int level=0; level<8; ++level) {
        const auto batch=plan.batches[level];
        ASSERT_GT(batch.count,0u);
        EXPECT_EQ(batch.first,next);
        next+=batch.count;
        vertices+=batch.count*rendering::grassLodVertices(level);
        for (std::size_t i=batch.first; i<next; ++i) {
            const auto& blade=plan.blades[i];
            EXPECT_EQ(rendering::grassLodLevel(glm::length(glm::dvec3(blade.root)-glm::dvec3(0,0,1))*100,60),level);
            EXPECT_NE(std::find_if(blades.begin(),blades.end(),[&](const auto& original) {
                return original.root==blade.root && original.up==blade.up && original.variation==blade.variation;
            }),blades.end());
        }
    }
    EXPECT_EQ(next,plan.blades.size());
    EXPECT_LT(next,blades.size());
    EXPECT_LT(vertices,blades.size()*14);
    EXPECT_EQ(rendering::grassLodVertices(7)-2,1); // One actual far triangle.
}

TEST(GrassLod, GuardBandCannotRemoveVisibleBladesOrUndersampleAfterMovement) {
    std::vector<rendering::GrassBlade> blades;
    for (int i=0; i<2000; ++i)
        blades.push_back({{i*.0004f,0,1},{0,0,1},{0,.2f,1,(i%256)/256.f}});
    const auto plan=rendering::batchGrass(blades,{0,0,1},100,60,9);
    std::set<float> present;
    for (const auto& blade:plan.blades) present.insert(blade.root.x);
    for (double movement : {-8.999,0.0,8.999}) {
        const glm::dvec3 eye(movement/100,0,1);
        for (const auto& blade:blades) {
            if (glm::length(glm::dvec3(blade.root)-eye)*100 < rendering::grassLodFadeEnd(blade.variation.w,60))
                EXPECT_TRUE(present.contains(blade.root.x));
        }
        for (int level=0; level<8; ++level) {
            const auto batch=plan.batches[level];
            for (std::size_t i=batch.first; i<batch.first+batch.count; ++i) {
                EXPECT_LE(level,rendering::grassLodLevel(glm::length(glm::dvec3(plan.blades[i].root)-eye)*100,60));
                const double originalDistance=glm::length(glm::dvec3(plan.blades[i].root)-glm::dvec3(0,0,1))*100;
                EXPECT_LE(rendering::grassLodVertices(level),originalDistance<=15+9 ? 14 : 4);
            }
        }
    }
}

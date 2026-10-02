#include "rendering/character/SurfaceContact.h"
#include <gtest/gtest.h>

TEST(SurfaceContact, UsesRenderedTriangleHeightAndNormalsAndInvalidatesItsCache) {
    std::vector<float> vertices={-1,-1,1, 1,-1,1, 0,1,1};
    std::vector<unsigned> indices={0,1,2};
    rendering::SurfaceContact ground;
    ground.bind(vertices,indices,1,1000,3);
    const rendering::GroundQuery fallback=[](const glm::dvec3& n) { return rendering::GroundContact{n*900.0,n}; };
    const auto contact=ground.sample({0,0,1},fallback);
    EXPECT_EQ(contact.position,glm::dvec3(0,0,1000));
    EXPECT_EQ(contact.normal,glm::dvec3(0,0,1));
    EXPECT_EQ(ground.sample({0,0,-1},fallback).position,glm::dvec3(0,0,-900));
    for (int i:{2,5,8}) vertices[i]=.999f;
    ground.bind(vertices,indices,2,1000,3);
    EXPECT_NEAR(ground.sample({0,0,1},fallback).position.z,999,1e-4);
    EXPECT_THROW(ground.sample({0,0,0},fallback),std::invalid_argument);
}

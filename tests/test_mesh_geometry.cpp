#include <gtest/gtest.h>
#include <cmath>
#include "rendering/Mesh.h"

TEST(MeshGeometryTest, SphereHasExpectedVertexAndTriangleCounts) {
    Mesh mesh;
    mesh.buildSphereGeometry(32);
    EXPECT_EQ(mesh.vertices.size(), 33u * 33u * 6u);
    EXPECT_EQ(mesh.indices.size(), 32u * 32u * 6u);
}

TEST(MeshGeometryTest, EveryPositionAndNormalLiesOnTheUnitSphere) {
    Mesh mesh;
    mesh.buildSphereGeometry(16);
    for (std::size_t i = 0; i < mesh.vertices.size(); i += 6) {
        const float x = mesh.vertices[i];
        const float y = mesh.vertices[i + 1];
        const float z = mesh.vertices[i + 2];
        EXPECT_NEAR(std::sqrt(x * x + y * y + z * z), 1.0f, 1e-5f);
        EXPECT_FLOAT_EQ(mesh.vertices[i + 3], x);
        EXPECT_FLOAT_EQ(mesh.vertices[i + 4], y);
        EXPECT_FLOAT_EQ(mesh.vertices[i + 5], z);
    }
}

TEST(MeshGeometryTest, TriangleIndicesStayInsideVertexBuffer) {
    Mesh mesh;
    mesh.buildSphereGeometry(8);
    const std::size_t vertexCount = mesh.vertices.size() / 6;
    for (unsigned int index : mesh.indices) {
        EXPECT_LT(index, vertexCount);
    }
}

TEST(MeshGeometryTest, LongitudeSeamClosesAtEveryLatitude) {
    Mesh mesh;
    constexpr int segments = 12;
    mesh.buildSphereGeometry(segments);
    for (int latitude = 0; latitude <= segments; ++latitude) {
        const std::size_t first = (latitude * (segments + 1)) * 6;
        const std::size_t last = (latitude * (segments + 1) + segments) * 6;
        for (int coordinate = 0; coordinate < 3; ++coordinate) {
            EXPECT_NEAR(mesh.vertices[first + coordinate],
                        mesh.vertices[last + coordinate], 1e-5f);
        }
    }
}

TEST(MeshGeometryTest, RebuildingReplacesOldGeometryAndRejectsTooFewSegments) {
    Mesh mesh;
    mesh.buildSphereGeometry(8);
    mesh.buildSphereGeometry(4);
    EXPECT_EQ(mesh.vertices.size(), 5u * 5u * 6u);
    EXPECT_EQ(mesh.indices.size(), 4u * 4u * 6u);
    EXPECT_THROW(mesh.buildSphereGeometry(2), std::invalid_argument);
}

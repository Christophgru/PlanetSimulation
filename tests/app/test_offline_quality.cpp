#include "rendering/quality/OfflineQuality.h"
#include "rendering/geometry/Terrain.h"
#include <gtest/gtest.h>

TEST(OfflineQuality, NormalSettingsRemainExactAndOfflineRulesAreDerivedOnce) {
    config::ScenarioConfig scene;
    scene.planets.resize(2);
    scene.surface_camera.enabled=true;
    scene.surface_camera.planet_index=0;
    auto& grass=scene.planets[0].foliage;
    grass.enabled=true; grass.draw_distance_m=60; grass.max_blades=200000;
    app::CommandLineOptions options;
    auto normal=rendering::offlineScenario(scene,options);
    EXPECT_EQ(normal.planets[0].foliage.draw_distance_m,60);
    EXPECT_EQ(normal.planets[1].terrain_lod.base_edge_segments,1);
    options.offlineQuality=options.renderTestMode=true;
    auto offline=rendering::offlineScenario(scene,options);
    EXPECT_EQ(offline.planets[0].foliage.draw_distance_m,1200);
    EXPECT_EQ(offline.planets[0].foliage.max_blades,2000000);
    EXPECT_EQ(offline.planets[0].terrain_lod.base_edge_segments,1);
    EXPECT_EQ(offline.planets[1].terrain_lod.base_edge_segments,10);
    EXPECT_EQ(offline.planets[1].terrain_lod.max_edge_segments,32);
    EXPECT_EQ(offline.planets[1].terrain_lod.max_triangle_budget,100000);
    EXPECT_EQ(offline.planets[1].terrain_lod.sink_depth_m,0);
    EXPECT_NO_THROW(offline.planets[1].terrain_lod.validate());
    EXPECT_EQ(scene.planets[0].foliage.draw_distance_m,60);
    EXPECT_EQ(scene.planets[0].foliage.max_blades,200000);
    EXPECT_EQ(rendering::sunSphereSegments(false),32);
    EXPECT_EQ(rendering::sunSphereSegments(true),256);
    options.foliageDistanceMultiplier=1;
    auto minimal=rendering::offlineScenario(scene,options);
    EXPECT_EQ(minimal.planets[0].foliage.draw_distance_m,60);
    EXPECT_EQ(minimal.planets[0].foliage.max_blades,200000);
    scene.planets[0].foliage.max_blades=3000000;
    options.foliageDistanceMultiplier=20;
    EXPECT_EQ(rendering::offlineScenario(scene,options).planets[0].foliage.max_blades,3000000);
}

TEST(OfflineQuality, DistantMoonActuallyBuildsMoreTrianglesWithinItsBudget) {
    config::ScenarioConfig scene;
    scene.planets.resize(1);
    app::CommandLineOptions options;
    options.offlineQuality=options.renderTestMode=true;
    const auto offline=rendering::offlineScenario(scene,options);
    const auto build=[](const config::PlanetConfig& planet) {
        rendering::TerrainSurface surface({},planet.terrain_lod,1,1000);
        return surface.buildGeometryForEye({0,0,10},{0,0,0}).triangleCount();
    };
    EXPECT_GT(build(offline.planets[0]),build(scene.planets[0]));
    EXPECT_LE(build(offline.planets[0]),100000);
    options.foliageDistanceMultiplier=21;
    EXPECT_THROW(rendering::offlineScenario(scene,options),std::invalid_argument);
}

#include <gtest/gtest.h>
#include "coordinates/PlanetLocalFrame.h"

namespace {
void expectVecNear(const glm::dvec3& actual, const glm::dvec3& expected,
                   double tolerance = 1e-10) {
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}
}

TEST(PlanetLocalFrameTest, PositionsFollowThePlanetCenterAndCardinalCoordinates) {
    coordinates::PlanetLocalFrame frame({5.0, 2.0, -3.0}, 0.5);
    expectVecNear(frame.toWorld({0.0, 0.0, 0.0}), {5.5, 2.0, -3.0});
    expectVecNear(frame.toWorld({0.0, 90.0, 0.2}), {5.0, 2.7, -3.0});
    expectVecNear(frame.toWorld({90.0, 0.0, 0.0}), {5.0, 2.0, -2.5});
    expectVecNear(frame.toWorld({-90.0, 0.0, 0.0}), {5.0, 2.0, -3.5});
}

TEST(PlanetLocalFrameTest, Longitude180FacesTheDevelopmentSun) {
    coordinates::PlanetLocalFrame frame({5.0, 0.0, 0.0}, 0.5);
    const glm::dvec3 location = frame.toWorld({0.0, 180.0, 0.2});
    expectVecNear(location, {4.3, 0.0, 0.0});
    const auto ned = frame.nedAt({0.0, 180.0, 0.2});
    expectVecNear(ned.down, {1.0, 0.0, 0.0});
    EXPECT_LT(glm::dot(glm::normalize(glm::dvec3(0.0) - location), ned.down), 0.0);
}

TEST(PlanetLocalFrameTest, WorldCoordinatesRoundTripWithAltitude) {
    coordinates::PlanetLocalFrame frame({1000000.0, -2000000.0, 3000000.0}, 6000.0);
    const coordinates::LatLonAlt input{42.5, -121.25, 12.0};
    const auto output = frame.fromWorld(frame.toWorld(input));
    EXPECT_NEAR(output.latitudeDeg, input.latitudeDeg, 1e-8);
    EXPECT_NEAR(output.longitudeDeg, input.longitudeDeg, 1e-8);
    EXPECT_NEAR(output.altitude, input.altitude, 1e-8);
}

TEST(PlanetLocalFrameTest, NedIsOrthonormalAndRightHandedAtEquatorAndPoles) {
    coordinates::PlanetLocalFrame frame({0.0, 0.0, 0.0}, 1.0);
    for (const coordinates::LatLonAlt location : {
             coordinates::LatLonAlt{0.0, 0.0, 0.0},
             coordinates::LatLonAlt{35.0, 73.0, 1.0},
             coordinates::LatLonAlt{90.0, 45.0, 0.0},
             coordinates::LatLonAlt{-90.0, -30.0, 0.0}}) {
        const auto ned = frame.nedAt(location);
        EXPECT_NEAR(glm::length(ned.north), 1.0, 1e-10);
        EXPECT_NEAR(glm::length(ned.east), 1.0, 1e-10);
        EXPECT_NEAR(glm::length(ned.down), 1.0, 1e-10);
        EXPECT_NEAR(glm::dot(ned.north, ned.east), 0.0, 1e-10);
        EXPECT_NEAR(glm::dot(ned.east, ned.down), 0.0, 1e-10);
        EXPECT_NEAR(glm::dot(ned.down, ned.north), 0.0, 1e-10);
        expectVecNear(glm::cross(ned.north, ned.east), ned.down);
        expectVecNear(ned.down, -glm::normalize(frame.toWorld(location)));
    }
}

TEST(PlanetLocalFrameTest, NedVectorsConvertBothWays) {
    coordinates::PlanetLocalFrame frame({5.0, 0.0, 0.0}, 0.5);
    const auto ned = frame.nedAt({23.0, 104.0, 0.0});
    const glm::dvec3 local{2.0, -3.0, 4.0};
    expectVecNear(ned.fromWorld(ned.toWorld(local)), local);
}

TEST(PlanetLocalFrameTest, InvalidLocationsAndCentersAreRejected) {
    EXPECT_THROW(coordinates::PlanetLocalFrame(glm::dvec3(0.0), 0.0), std::invalid_argument);
    coordinates::PlanetLocalFrame frame({5.0, 0.0, 0.0}, 0.5);
    EXPECT_THROW(frame.toWorld({91.0, 0.0, 0.0}), std::invalid_argument);
    EXPECT_THROW(frame.nedAt({0.0, 0.0, -0.5}), std::invalid_argument);
    EXPECT_THROW(frame.fromWorld(frame.center()), std::invalid_argument);
}

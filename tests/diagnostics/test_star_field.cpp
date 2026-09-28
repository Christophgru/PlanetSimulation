#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "rendering/StarField.h"

TEST(StarFieldTest, HashIsRepeatableAndSeedChangesTheField) {
    const std::array<std::int32_t, 3> cell{-712, 43, 901};
    const auto first = rendering::starCellHash(cell, 7429u);
    EXPECT_EQ(first, rendering::starCellHash(cell, 7429u));
    EXPECT_NE(first, rendering::starCellHash(cell, 7430u));
    EXPECT_GE(rendering::starHashUnit(first), 0.0);
    EXPECT_LE(rendering::starHashUnit(first), 1.0);
}

TEST(StarFieldTest, DensitySelectsAStableSparseSubsetOfCells) {
    constexpr double density = 0.01;
    int visible = 0;
    int changedBySeed = 0;
    for (int i = 0; i < 10000; ++i) {
        const std::array<std::int32_t, 3> cell{i - 5000, i * 17, -i * 31};
        const bool first = rendering::starCellVisible(cell, 7429u, density);
        const bool second = rendering::starCellVisible(cell, 7430u, density);
        visible += first ? 1 : 0;
        changedBySeed += first != second ? 1 : 0;
    }
    EXPECT_GT(visible, 50);
    EXPECT_LT(visible, 160);
    EXPECT_GT(changedBySeed, 50);
}

TEST(StarFieldTest, ZeroDensityIsEmptyAndInvalidDensityIsRejected) {
    const std::array<std::int32_t, 3> cell{1, 2, 3};
    EXPECT_FALSE(rendering::starCellVisible(cell, 42u, 0.0));
    EXPECT_THROW(rendering::starCellVisible(cell, 42u, -0.01),
                 std::invalid_argument);
    EXPECT_THROW(rendering::starCellVisible(cell, 42u, 1.01),
                 std::invalid_argument);
}

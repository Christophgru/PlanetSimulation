#include <gtest/gtest.h>
#include "rendering/diagnostics/GpuUtilization.h"

TEST(GpuUtilization, MatchesUniqueRendererAndRejectsAmbiguity) {
    EXPECT_EQ(rendering::utilizationDevice("Quadro M1000M/PCIe/SSE2", {"NVIDIA Quadro M1000M"}), 0u);
    EXPECT_EQ(rendering::utilizationDevice("NVIDIA Quadro M1000M/PCIe/SSE2", {"Other", "Quadro M1000M"}), 1u);
    EXPECT_FALSE(rendering::utilizationDevice("llvmpipe", {"NVIDIA Quadro M1000M"}));
    EXPECT_FALSE(rendering::utilizationDevice("Quadro M1000M", {"Quadro M1000M", "Quadro M1000M"}));
}
TEST(GpuUtilization, SoftwareRendererIsUnavailable) {
    rendering::NvidiaUtilization reader("Mesa/X.org", "llvmpipe");
    EXPECT_FALSE(reader.sample());
}
#if defined(__linux__)
TEST(GpuUtilization, UsesDriverPercentageAndDiscardsFailedOrInvalidReadings) {
    rendering::NvidiaUtilization reader("NVIDIA Corporation", "Quadro M1000M/PCIe/SSE2");
    EXPECT_EQ(reader.sample(), 42u);
    setenv("TEST_NVML_GPU", "0", 1); EXPECT_EQ(reader.sample(), 0u);
    setenv("TEST_NVML_GPU", "100", 1); EXPECT_EQ(reader.sample(), 100u);
    setenv("TEST_NVML_GPU", "101", 1); EXPECT_FALSE(reader.sample());
    unsetenv("TEST_NVML_GPU");
    setenv("TEST_NVML_ERROR", "3", 1); EXPECT_FALSE(reader.sample());
    unsetenv("TEST_NVML_ERROR"); EXPECT_EQ(reader.sample(), 42u);
}
TEST(GpuUtilization, InitializationFailureAndDuplicateAdaptersAreUnavailable) {
    setenv("TEST_NVML_INIT_ERROR", "9", 1);
    { rendering::NvidiaUtilization reader("NVIDIA Corporation", "Quadro M1000M"); EXPECT_FALSE(reader.sample()); }
    unsetenv("TEST_NVML_INIT_ERROR");
    setenv("TEST_NVML_COUNT", "2", 1);
    { rendering::NvidiaUtilization reader("NVIDIA Corporation", "Quadro M1000M"); EXPECT_FALSE(reader.sample()); }
    unsetenv("TEST_NVML_COUNT");
}
#endif

#pragma once
#include "rendering/character/AstronautMotion.h"
#include <cstdint>
#include <filesystem>
#include <istream>
#include <vector>

namespace rendering {
// Versioned, little-endian deployment format produced by prepare_aces.py.
// Explicit arrays keep the GPU layout independent of GLM alignment options.
struct SuitVertex {
    float position[3], normal[3], color[4];
    std::uint16_t joints[2];
    float weights[2];
};
static_assert(sizeof(SuitVertex)==52);
struct SuitBone { glm::vec3 head, tail; };
struct AcesSuit {
    static constexpr std::size_t BoneCount=13;
    std::array<SuitBone,BoneCount> bones;
    glm::vec3 bindBodyOffset;
    std::vector<SuitVertex> vertices;
    std::vector<std::uint32_t> indices;
    static AcesSuit load(std::istream& stream);
    static AcesSuit load(const std::filesystem::path& path);
    std::array<glm::mat4,BoneCount> palette(const AstronautPose& pose) const;
};
}

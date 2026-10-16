#pragma once
#include <GL/glew.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <glm/glm.hpp>

namespace rendering {
struct GrassCellRecord {
    glm::uvec4 key,geometry,span; // canonical ID/permutation, geometry hash, root ranges
    glm::vec4 metric; // expected candidate count (area * density)
};
static_assert(sizeof(GrassCellRecord)==64);
inline std::uint64_t grassCellTableSlots(std::uint64_t n) {return std::bit_ceil(std::max<std::uint64_t>(1,2*n));}
inline std::uint64_t grassCellBytes(std::uint64_t n) {return n?64*n+4*grassCellTableSlots(n)+32:0;}
// A preparation leases its source until all copy commands complete. Cancellation,
// clear and retirement cannot destroy a source in the middle of a sliced update.
struct GrassCellCache {
    GLuint roots=0,bounds=0,cells=0,table=0,summary=0;
    std::size_t count=0,tableSlots=0;
    std::array<std::uint32_t,8> totals{}; // retained, entering, dirty, departing, copied, generated, errors, matched
    ~GrassCellCache();
    GrassCellCache()=default;
    GrassCellCache(const GrassCellCache&)=delete;
    GrassCellCache& operator=(const GrassCellCache&)=delete;
};
}

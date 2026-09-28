#pragma once
#include <cstdint>
#include <optional>
#include <GL/glew.h>

namespace rendering {
inline std::optional<std::uint64_t> availableVideoMemoryBytes() {
    // NVX and ATI report free texture memory in KiB; both are optional.
    if (glewIsSupported("GL_NVX_gpu_memory_info")) {
        GLint kib = 0;
        glGetIntegerv(0x9049, &kib); // GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
        if (kib > 0) return std::uint64_t(kib) * 1024;
    }
    if (glewIsSupported("GL_ATI_meminfo")) {
        GLint kib[4]{};
        glGetIntegerv(0x87FC, kib); // GL_TEXTURE_FREE_MEMORY_ATI
        if (kib[0] > 0) return std::uint64_t(kib[0]) * 1024;
    }
    return std::nullopt;
}
} // namespace rendering

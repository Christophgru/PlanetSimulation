#pragma once

#include <png.h>
#include <span>
#include <stdexcept>
#include <string>

namespace rendering {
// Top-down, tightly packed 8-bit RGB or RGBA pixels; no color conversion.
inline void writePng(const std::string& path, int width, int height, int channels,
                     std::span<const unsigned char> pixels) {
    // Keep row sizes within the simplified API limits on older libpng releases.
    if (width <= 0 || width > 8192 || height <= 0 || height > 8192 ||
        (channels != 3 && channels != 4) ||
        pixels.size() != static_cast<std::size_t>(width) * height * channels)
        throw std::invalid_argument("Invalid PNG dimensions, channels, or pixel buffer");
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = width;
    image.height = height;
    image.format = channels == 4 ? PNG_FORMAT_RGBA : PNG_FORMAT_RGB;
    const bool written = png_image_write_to_file(&image, path.c_str(), 0,
                                                 pixels.data(), 0, nullptr) != 0;
    const std::string error = image.message;
    png_image_free(&image);
    if (!written) throw std::runtime_error("Failed to write PNG " + path + ": " + error);
}
} // namespace rendering

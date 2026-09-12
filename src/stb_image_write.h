// stb_image_write - official simplified wrapper for PNG writing
// Uses the real stb_image_write API via stbi_write_png()

#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#include <stdio.h>

extern "C" {
    // Official stb_image_write function declaration
    int stbi_write_png(const char* filename, int width, int height, int comp, const unsigned char* data);
}

// Wrapper function for convenience
static void stbi_write_png_wrapper(const char* filename, int width, int height, int comp, const unsigned char* data) {
    return stbi_write_png(filename, width, height, comp, data);
}

#endif

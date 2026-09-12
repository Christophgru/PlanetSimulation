// stb_image_write - official implementation for PNG writing
// https://github.com/nothings/stb
// This is the official header-only implementation

#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stbi__context stbi__context;

// Function declarations
int stbi_write_png(const char* filename, int width, int height, int comp, const unsigned char* data, int stride);
int stbi_write_jpg(const char* filename, int width, int height, int comp, const unsigned char* data, int quality);
int stbi_write_bmp(const char* filename, int width, int height, int comp, const unsigned char* data);
int stbi_write_tga(const char* filename, int width, int height, int comp, const unsigned char* data);

#ifdef __cplusplus
}
#endif

#endif // STB_IMAGE_WRITE_H

// stb_image_write - v1.16 - public domain image write library
// https://github.com/nothings/stb
// This is the official header-only implementation

#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#if defined(STBI_ONLY_JPEG) || defined(STBI_ONLY_PNG) || defined(STBI_ONLY_BMP) \
  || defined(STBI_ONLY_TGA) || defined(STBI_ONLY_GIF) || defined(STBI_ONLY_PS) \
  || defined(STBI_ONLY_PIC) || defined(STBI_ONLY_XISD) || defined(STBI_ONLY_XBM)
   #error "STBI_ONLY should specify exactly one of JPEG,PNG,BMP,TGA,GIF,PS,PIC,XISD,XBM"
#endif

#ifndef STBI_ONLY_JPEG
#define STBI_WRITE_TNSF 1
#endif

#ifndef STBI_ONLY_PNG
#define STBI_WRITE_TNSF 1
#endif

#ifndef STBI_ONLY_BMP
#define STBI_WRITE_TNSF 1
#endif

#ifndef STBI_ONLY_TGA
#define STBI_WRITE_TNSF 1
#endif

#ifndef STBI_ONLY_GIF
#define STBI_WRITE_TNSF 1
#endif

#ifndef STBI_ONLY_PS
#define STBI_WRITE_TNSF 1
#endif

#ifndef STBI_ONLY_PIC
#define STBI_WRITE_TNSF 1
#endif

#ifndef STBI_ONLY_XISD
#define STBI_WRITE_TNSF 1
#endif

#ifndef STBI_ONLY_XBM
#define STBI_WRITE_TNSF 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stbi__context stbi__context;

int stbi_write_png(const char* filename, int width, int height, int comp, const void* data, int stride_bytes);
int stbi_write_bmp(const char* filename, int w, int h, int comp, const void* data);
int stbi_write_jpg(const char* filename, int quality, int width, int height, int comp, const void* data);
int stbi_write_tga(const char* filename, int width, int height, int comp, const void* data);
int stbi_write_hdr(const char* filename, int width, int height, int comp, const float* data);
int stbi_write_psd(const char* filename, int width, int height, int comp, const void* data);

#ifdef __cplusplus
}
#endif

#endif // STB_IMAGE_WRITE_H

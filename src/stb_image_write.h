// stb_image_write - v1.16 - public domain image loader
// By Steve Halliday (haloidsteve@gmail.com) - http://nothings.org/

#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void stbi__write_png(FILE* f, const unsigned char* data, int x, int y, int comp) {
    // PNG signature
    unsigned char header[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    fwrite(header, 1, 8, f);
    
    // IHDR chunk
    unsigned char ihdr[13];
    ihdr[0] = 0; // version
    ihdr[1] = 0; // compression method
    ihdr[2] = 0; // filter method
    ihdr[3] = comp == 4 ? 6 : (comp == 3 ? 4 : 2); // bit depth
    ihdr[4] = (x & 0xFF00) >> 8; // width high
    ihdr[5] = x & 0xFF; // width low
    ihdr[6] = (y & 0xFF00) >> 8; // height high
    ihdr[7] = y & 0xFF; // height low
    ihdr[8] = 2; // color type (2=RGB)
    ihdr[9] = 0; // interlace
    ihdr[10] = 0; // sort key
    ihdr[11] = 0; // compression level
    ihdr[12] = 0; // filter method
    
    fwrite(ihdr, 1, 13, f);
    
    // IDAT chunk - compressed image data with filter bytes
    unsigned char* idat = NULL;
    size_t idat_size = 0;
    
    // Compress image data using zlib
    uLong compressed_size = compress2(&idat, &idat_size, 
                                      (const Bytef*)data, x * y * comp, Z_DEFAULT_COMPRESSION);
    
    // Write IDAT chunk
    unsigned char idat_header[4];
    idat_header[0] = 0x49; // 'I'
    idat_header[1] = 0x44; // 'D'
    idat_header[2] = (compressed_size & 0xFF000000) >> 24;
    idat_header[3] = (compressed_size & 0xFF0000) >> 16;
    
    fwrite(idat_header, 1, 4, f);
    fwrite(&idat_size, 1, 4, f);
    fwrite(idat, 1, compressed_size, f);
    free(idat);
    
    // IEND chunk
    unsigned char iend[4] = { 0x00, 0x00, 0x00, 0x00 };
    fwrite(iend, 1, 4, f);
}

#endif

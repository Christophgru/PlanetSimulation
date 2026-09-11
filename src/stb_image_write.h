// stb_image_write - simplified PNG writer without zlib dependency
// Writes uncompressed PNG data (compression method 0)

#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void stbi__write_png(FILE* f, const unsigned char* data, int x, int y, int comp) {
    // PNG signature
    unsigned char header[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    fwrite(header, 1, 8, f);
    
    // IHDR chunk (length + type + data + CRC)
    unsigned char ihdr[13];
    ihdr[0] = 0; // version
    ihdr[1] = 0; // compression method (0 = none for uncompressed)
    ihdr[2] = 0; // filter method
    ihdr[3] = comp == 4 ? 6 : (comp == 3 ? 4 : 2); // bit depth
    ihdr[4] = (x & 0xFF00) >> 8; // width high
    ihdr[5] = x & 0xFF; // width low
    ihdr[6] = (y & 0xFF00) >> 8; // height high
    ihdr[7] = y & 0xFF; // height low
    ihdr[8] = 2; // color type (2=RGB, 3 channels)
    ihdr[9] = 0; // interlace
    ihdr[10] = 0; // sort key
    ihdr[11] = 0; // compression level
    ihdr[12] = 0; // filter method
    
    fwrite(ihdr, 1, 13, f);
    
    // IDAT chunk - raw image data with filter bytes (no compression)
    for (int row = 0; row < y; row++) {
        unsigned char filter = 0; // no filter
        fwrite(&filter, 1, 1, f);
        
        for (int col = 0; col < x; col++) {
            int idx = (row * x + col) * comp;
            unsigned char c = data[idx];
            fwrite(&c, 1, 1, f);
        }
    }
    
    // IEND chunk
    unsigned char iend[4] = { 0x00, 0x00, 0x00, 0x00 };
    fwrite(iend, 1, 4, f);
}

#endif

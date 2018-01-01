//
// Save a screenshot as a PNG file
// uses libpng
// Written by Larry Bank
// Copyright (c) 2017 BitBank Software, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <png.h>

//
// Convert game image from RGB565 to RGB888 (RGB565 isn't supported by PNG)
// And write to a PNG file
// returns 0 for success, -1 for error
//
int SG_SavePNG(char* file_name, unsigned char *pBitmap, int iWidth, int iHeight, int iPitch)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep * row_pointers;
    unsigned char *pTempBitmap;
    int x, y;
    
    /* create file */
    FILE *fp = fopen(file_name, "wb");
    if (!fp)
    {
        printf("[write_png_file] File %s could not be opened for writing", file_name);
        return -1;
    }
    
    /* initialize stuff */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    if (!png_ptr)
    {
        printf("[write_png_file] png_create_write_struct failed");
        fclose(fp);
        return -1;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        printf("[write_png_file] png_create_info_struct failed");
        fclose(fp);
        return -1;
    }
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        printf("[write_png_file] Error during init_io");
        fclose(fp);
        return -1;
    }
    png_init_io(png_ptr, fp);
    
    // Convert incoming bitmap from RGB565 to RGB888
    pTempBitmap = (unsigned char *)malloc(iWidth * iHeight * 3);
    for (y=0; y<iHeight; y++)
    {
       unsigned char *d = &pTempBitmap[y * iWidth * 3];
       unsigned short *s = (unsigned short *)&pBitmap[y * iPitch];
       unsigned short us;
       unsigned char c;
       for (x=0; x<iWidth; x++)
       {
          us = *s++;
          c = ((us & 0xf800) >> 8) | ((us & 0xe000) >> 13); // red
          *d++ = c;
          c = ((us & 0x7e0) >> 3) | ((us & 0x600) >> 9); // green
          *d++ = c;
          c = ((us & 0x1f) << 3) | ((us & 0x1c) >> 2); // blue
          *d++ = c;
       } // for x
    } // for y

    /* write header */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        printf("[write_png_file] Error during writing header");
        fclose(fp);
        return -1;
    }
    
    png_set_IHDR(png_ptr, info_ptr, iWidth, iHeight,
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    
    png_write_info(png_ptr, info_ptr);
    
    /* write bytes */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        printf("[write_png_file] Error during writing bytes");
        fclose(fp);
        return -1;
    }
    row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * iHeight);
    for (y=0; y<iHeight; y++)
    {
        row_pointers[y] = (png_byte*) &pTempBitmap[iWidth*3*y];
    }
    png_write_image(png_ptr, row_pointers);
    /* end write */
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        printf("[write_png_file] Error during end of write");
        free(row_pointers);
        free(pTempBitmap);
        fclose(fp);
        return -1;
    }
    png_write_end(png_ptr, NULL);
    free(row_pointers);
    free(pTempBitmap);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return 0;
} /* SG_SavePNG() */


/*
 * genpng.c - Generate a PNG image showing all printable ASCII characters
 * 
 * This example demonstrates how to:
 * 1. Load a TrueType font using struetype.h
 * 2. Render all printable ASCII characters (32-126) using stt_GetCodepointBitmap
 * 3. Arrange glyphs in a grid layout with optional grid lines
 * 4. Center each glyph in its grid cell
 * 5. Convert grayscale bitmap to RGB format
 * 6. Save the result as a PNG file using svpng.h
 *
 * Usage: genpng [font_file] [output_file]
 * Defaults: genpng profont/ProFont.ttf fontgrid.png
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"
#include "svpng.h"

int main(int argc, const char *argv[])
{
    /* Parse command line arguments */
    const char *fontPath = (argc > 1) ? argv[1] : "profont/ProFont.ttf";
    const char *outputPath = (argc > 2) ? argv[2] : "fontgrid.png";
    
    /* Grid configuration - renders ASCII 32-126 (95 printable characters) */
    const int gridCols = 16;   /* 16 columns */
    const int gridRows = 6;    /* 6 rows (16*6 = 96 cells, need 95) */
    const int cellWidth = 48;  /* Width of each cell in pixels */
    const int cellHeight = 48; /* Height of each cell in pixels */
    const int fontSize = 24;   /* Font size in pixels */
    const int drawGridLines = 1; /* Draw faint grid lines */
    
    /* Calculate image dimensions */
    const int imageWidth = gridCols * cellWidth;
    const int imageHeight = gridRows * cellHeight;
    
    printf("Generating font grid: %dx%d pixels, %dx%d cells\n", 
           imageWidth, imageHeight, gridCols, gridRows);
    printf("Font: %s\n", fontPath);
    printf("Output: %s\n", outputPath);
    
    /* Load font file */
    FILE *fontFile = fopen(fontPath, "rb");
    if (!fontFile) {
        printf("Failed to open font file: %s\n", fontPath);
        exit(-1);
    }

    fseek(fontFile, 0, SEEK_END);
    long size = ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);

    unsigned char *fontBuffer = calloc(size, sizeof(unsigned char));
    fread(fontBuffer, size, 1, fontFile);
    fclose(fontFile);

    /* Initialize font */
    stt_fontinfo info;
    if (!stt_InitFont(&info, fontBuffer, size, 0)) {
        printf("Failed to initialize font\n");
        free(fontBuffer);
        exit(-1);
    }

    /* Calculate font scaling - converts font units to pixels */
    float scale = stt_ScaleForPixelHeight(&info, fontSize);
    
    /* Create grayscale image buffer (8-bit per pixel) */
    unsigned char *grayBuffer = calloc(imageWidth * imageHeight, sizeof(unsigned char));
    if (!grayBuffer) {
        printf("Failed to allocate image buffer\n");
        free(fontBuffer);
        exit(-1);
    }
    
    /* Fill with light gray background (240 = light gray) */
    memset(grayBuffer, 240, imageWidth * imageHeight);
    
    /* Render each printable ASCII character (32=' ' to 126='~') */
    for (int i = 0; i < 95; i++) {
        int codepoint = 32 + i;  /* ASCII 32-126 */
        int row = i / gridCols;  /* Grid row (0-5) */
        int col = i % gridCols;  /* Grid column (0-15) */
        
        /* Get glyph bitmap using struetype functions */
        int glyphWidth, glyphHeight, xOffset, yOffset;
        unsigned char *glyphBitmap = stt_GetCodepointBitmap(&info, scale, scale, 
                                                             codepoint, &glyphWidth, &glyphHeight, 
                                                             &xOffset, &yOffset);
        
        if (glyphBitmap) {
            /* Calculate position to center glyph in cell */
            int cellX = col * cellWidth;
            int cellY = row * cellHeight;
            int glyphX = cellX + (cellWidth - glyphWidth) / 2 + xOffset;
            int glyphY = cellY + (cellHeight - glyphHeight) / 2 + yOffset;
            
            /* Copy glyph bitmap to main image buffer */
            for (int gy = 0; gy < glyphHeight; gy++) {
                for (int gx = 0; gx < glyphWidth; gx++) {
                    int imageX = glyphX + gx;
                    int imageY = glyphY + gy;
                    
                    /* Check bounds to prevent buffer overflow */
                    if (imageX >= 0 && imageX < imageWidth && 
                        imageY >= 0 && imageY < imageHeight) {
                        unsigned char glyphPixel = glyphBitmap[gy * glyphWidth + gx];
                        /* Blend glyph with background (antialiased rendering) */
                        int bgPixel = grayBuffer[imageY * imageWidth + imageX];
                        int blended = bgPixel - (glyphPixel * (255 - bgPixel)) / 255;
                        grayBuffer[imageY * imageWidth + imageX] = blended;
                    }
                }
            }
            
            /* Free the glyph bitmap allocated by struetype */
            stt_FreeBitmap(glyphBitmap, NULL);
        }
    }
    
    /* Draw faint grid lines if enabled */
    if (drawGridLines) {
        /* Vertical grid lines */
        for (int x = 0; x <= gridCols; x++) {
            int lineX = x * cellWidth;
            if (lineX < imageWidth) {
                for (int y = 0; y < imageHeight; y++) {
                    grayBuffer[y * imageWidth + lineX] = 200; /* Light gray */
                }
            }
        }
        /* Horizontal grid lines */
        for (int y = 0; y <= gridRows; y++) {
            int lineY = y * cellHeight;
            if (lineY < imageHeight) {
                for (int x = 0; x < imageWidth; x++) {
                    grayBuffer[lineY * imageWidth + x] = 200; /* Light gray */
                }
            }
        }
    }
    
    /* Convert grayscale to RGB for PNG output (svpng expects RGB format) */
    unsigned char *rgbBuffer = malloc(imageWidth * imageHeight * 3);
    if (!rgbBuffer) {
        printf("Failed to allocate RGB buffer\n");
        free(grayBuffer);
        free(fontBuffer);
        exit(-1);
    }
    
    /* Convert each grayscale pixel to RGB by duplicating the gray value */
    for (int i = 0; i < imageWidth * imageHeight; i++) {
        unsigned char gray = grayBuffer[i];
        rgbBuffer[i * 3] = gray;     /* Red channel */
        rgbBuffer[i * 3 + 1] = gray; /* Green channel */
        rgbBuffer[i * 3 + 2] = gray; /* Blue channel */
    }
    
    /* Write PNG file using svpng.h */
    FILE *outFile = fopen(outputPath, "wb");
    if (!outFile) {
        printf("Failed to create output file: %s\n", outputPath);
        free(rgbBuffer);
        free(grayBuffer);
        free(fontBuffer);
        exit(-1);
    }
    
    /* svpng(file, width, height, rgb_data, alpha_channel) */
    svpng(outFile, imageWidth, imageHeight, rgbBuffer, 0);
    fclose(outFile);
    
    printf("Font grid saved to %s\n", outputPath);
    
    /* Cleanup */
    free(rgbBuffer);
    free(grayBuffer);
    free(fontBuffer);
    
    return 0;
}


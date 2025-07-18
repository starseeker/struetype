/*
 * genpng.c - Generate PNG images showing all available glyphs in a font
 *
 * This example demonstrates how to:
 * 1. Load a TrueType font using struetype.h
 * 2. Scan the entire Unicode range (0-0x10FFFF) to find all available glyphs
 * 3. Render ALL glyphs that exist in the font using stt_GetCodepointBitmap
 * 4. Arrange glyphs in a grid layout with optional grid lines
 * 5. Center each glyph visually in its grid cell using font metrics
 * 6. Render characters darker on light background using simple subtraction
 * 7. Convert grayscale bitmap to RGB format
 * 8. Automatically split output into multiple PNG files if needed to stay within size limits
 * 9. Smart output naming: single file as .png, multiple files as <root>-01.png, etc.
 * 10. Add footer with font name and Unicode range using the font itself
 *
 * Key rendering features:
 * - Uses font ascent/descent metrics to calculate proper baseline positioning
 * - Centers glyphs horizontally and vertically for visual consistency
 * - Ignores xOffset/yOffset for cleaner visual centering
 * - Simple subtraction blending for crisp, dark text on light background
 * - Scans entire Unicode range using stt_FindGlyphIndex to find available glyphs
 * - Target maximum image size of 2450x3200 pixels (portrait, 300dpi print ready)
 * - All pages have identical dimensions for consistent output
 * - Footer displays font name and Unicode range using the font being visualized
 *
 * Enhanced features:
 * - Smart output naming: single image without suffix, multiple with zero-padded numbers
 * - Footer strip below grid showing font name and Unicode range
 * - Footer rendered using the font itself if all characters are available
 * - Consistent page dimensions across all output files
 * - Print-ready 2450x3200 pixel output size (8.17" x 10.67" at 300dpi)
 *
 * Usage: genpng [font_file] [output_prefix]
 * Defaults: genpng profont/ProFont.ttf fontgrid
 * Output: fontgrid.png (single) or fontgrid-01.png, fontgrid-02.png, etc. (multiple)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"
#include "svpng.h"

/* Function to extract base name from font file path and create output prefix */
void get_output_prefix(const char *fontPath, const char *userPrefix, char *outputPrefix, size_t bufferSize) {
    if (userPrefix) {
        snprintf(outputPrefix, bufferSize, "%s", userPrefix);
    } else {
        /* Extract filename without extension from font path */
        char *fontPathCopy = strdup(fontPath);
        char *baseName = basename(fontPathCopy);
        char *dot = strrchr(baseName, '.');
        if (dot) *dot = '\0';  /* Remove extension */
        snprintf(outputPrefix, bufferSize, "%s", baseName);
        free(fontPathCopy);
    }
}

/* Function to collect all available glyphs in the font */
int collect_available_glyphs(stt_fontinfo *info, int **glyphs) {
    int count = 0;
    int capacity = 1000; /* Start with reasonable capacity */
    int *glyph_list = malloc(capacity * sizeof(int));
    
    if (!glyph_list) {
        printf("Failed to allocate memory for glyph collection\n");
        return 0;
    }
    
    /* Scan the entire Unicode range */
    for (int codepoint = 0; codepoint <= 0x10FFFF; codepoint++) {
        if (stt_FindGlyphIndex(info, codepoint) != 0) {
            /* Found a glyph for this codepoint */
            if (count >= capacity) {
                capacity *= 2;
                int *new_list = realloc(glyph_list, capacity * sizeof(int));
                if (!new_list) {
                    printf("Failed to reallocate memory for glyph collection\n");
                    free(glyph_list);
                    return 0;
                }
                glyph_list = new_list;
            }
            glyph_list[count++] = codepoint;
        }
    }
    
    *glyphs = glyph_list;
    printf("Found %d glyphs in font\n", count);
    return count;
}

/* Function to extract font name from the font file path */
void get_font_name(const char *fontPath, char *fontName, size_t bufferSize) {
    char *fontPathCopy = strdup(fontPath);
    char *baseName = basename(fontPathCopy);
    char *dot = strrchr(baseName, '.');
    if (dot) *dot = '\0';  /* Remove extension */
    snprintf(fontName, bufferSize, "%s", baseName);
    free(fontPathCopy);
}

/* Function to check if all characters in a string are available in the font */
int all_chars_available(stt_fontinfo *info, const char *text) {
    const char *ptr = text;
    while (*ptr) {
        int codepoint = (unsigned char)*ptr;
        if (stt_FindGlyphIndex(info, codepoint) == 0) {
            return 0; /* Character not available */
        }
        ptr++;
    }
    return 1; /* All characters available */
}

/* Function to render footer text using the font */
void render_footer(stt_fontinfo *info, unsigned char *buffer, int imageWidth, int imageHeight, 
                   int footerHeight, const char *fontName, int startCodepoint, int endCodepoint) {
    /* Create footer text */
    char footerText[256];
    snprintf(footerText, sizeof(footerText), "Font: %s   U+%04X–U+%04X", 
             fontName, startCodepoint, endCodepoint);
    
    /* Check if all characters are available */
    if (!all_chars_available(info, footerText)) {
        return; /* Skip footer if characters not available */
    }
    
    /* Calculate font scaling for footer */
    float footerScale = stt_ScaleForPixelHeight(info, 14);
    
    /* Get font metrics */
    int ascent, descent, lineGap;
    stt_GetFontVMetrics(info, &ascent, &descent, &lineGap);
    
    /* Calculate text dimensions */
    int textWidth = 0;
    const char *ptr = footerText;
    while (*ptr) {
        int advance, leftSideBearing;
        stt_GetCodepointHMetrics(info, *ptr, &advance, &leftSideBearing);
        textWidth += (int)(advance * footerScale);
        ptr++;
    }
    
    /* Position footer text (right-aligned) */
    int footerY = imageHeight - footerHeight + (footerHeight + (int)(ascent * footerScale)) / 2;
    int footerX = imageWidth - textWidth - 20; /* 20 pixel margin from right */
    
    /* Render each character */
    int currentX = footerX;
    ptr = footerText;
    while (*ptr) {
        int glyphWidth, glyphHeight, xOffset, yOffset;
        unsigned char *glyphBitmap = stt_GetCodepointBitmap(info, footerScale, footerScale,
                                                             *ptr, &glyphWidth, &glyphHeight,
                                                             &xOffset, &yOffset);
        
        if (glyphBitmap) {
            /* Calculate glyph position */
            int glyphX = currentX + xOffset;
            int glyphY = footerY + yOffset;
            
            /* Copy glyph bitmap to main image buffer */
            for (int gy = 0; gy < glyphHeight; gy++) {
                for (int gx = 0; gx < glyphWidth; gx++) {
                    int imageX = glyphX + gx;
                    int imageY = glyphY + gy;
                    
                    /* Check bounds */
                    if (imageX >= 0 && imageX < imageWidth &&
                        imageY >= 0 && imageY < imageHeight) {
                        unsigned char glyphPixel = glyphBitmap[gy * glyphWidth + gx];
                        
                        /* Render characters darker */
                        int bgPixel = buffer[imageY * imageWidth + imageX];
                        int darkened = bgPixel - glyphPixel;
                        if (darkened < 0) darkened = 0;
                        buffer[imageY * imageWidth + imageX] = darkened;
                    }
                }
            }
            
            /* Advance to next character position */
            int advance, leftSideBearing;
            stt_GetCodepointHMetrics(info, *ptr, &advance, &leftSideBearing);
            currentX += (int)(advance * footerScale);
            
            /* Free the glyph bitmap */
            stt_FreeBitmap(glyphBitmap, NULL);
        }
        ptr++;
    }
}

int main(int argc, const char *argv[])
{
    /* Parse command line arguments */
    const char *fontPath = (argc > 1) ? argv[1] : "profont/ProFont.ttf";
    const char *outputPrefix = (argc > 2) ? argv[2] : NULL;

    /* Configuration constants */
    const int cellWidth = 48;    /* Width of each cell in pixels */
    const int cellHeight = 48;   /* Height of each cell in pixels */
    const int fontSize = 24;     /* Font size in pixels */
    const int drawGridLines = 1; /* Draw faint grid lines */
    const int maxImageWidth = 2450;  /* Maximum image width (portrait orientation) */
    const int maxImageHeight = 3200; /* Maximum image height */
    const int footerHeight = 80;     /* Height reserved for footer */
    const int footerFontSize = 14;   /* Font size for footer text */
    
    /* Calculate available space for grid (excluding footer) */
    const int availableHeight = maxImageHeight - footerHeight;
    const int maxGridCols = maxImageWidth / cellWidth;   /* 51 columns */
    const int maxGridRows = availableHeight / cellHeight; /* 65 rows */
    const int maxGlyphsPerFile = maxGridCols * maxGridRows; /* 3315 glyphs per file */

    /* Get output prefix */
    char finalOutputPrefix[256];
    get_output_prefix(fontPath, outputPrefix, finalOutputPrefix, sizeof(finalOutputPrefix));

    printf("Font: %s\n", fontPath);
    printf("Output prefix: %s\n", finalOutputPrefix);
    printf("Max glyphs per file: %d (grid: %dx%d)\n", maxGlyphsPerFile, maxGridCols, maxGridRows);

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

    /* Collect all available glyphs */
    int *availableGlyphs = NULL;
    int totalGlyphs = collect_available_glyphs(&info, &availableGlyphs);
    if (totalGlyphs == 0) {
        printf("No glyphs found in font\n");
        free(fontBuffer);
        exit(-1);
    }

    /* Calculate number of files needed */
    int numFiles = (totalGlyphs + maxGlyphsPerFile - 1) / maxGlyphsPerFile;
    printf("Total glyphs: %d, will create %d file(s)\n", totalGlyphs, numFiles);

    /* Get font name for footer */
    char fontName[256];
    get_font_name(fontPath, fontName, sizeof(fontName));

    /* Calculate consistent image dimensions (all pages identical) */
    int imageWidth = maxImageWidth;
    int imageHeight = maxImageHeight;

    /* Calculate font scaling - converts font units to pixels */
    float scale = stt_ScaleForPixelHeight(&info, fontSize);

    /* Get font metrics for baseline calculation */
    int ascent, descent, lineGap;
    stt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    
    /* Calculate baseline position to center font vertically in cell */
    float baseline = (cellHeight / 2.0f) + ((ascent - descent) / 2.0f * scale) - (ascent * scale);

    /* Generate each file */
    for (int fileIndex = 0; fileIndex < numFiles; fileIndex++) {
        int startGlyph = fileIndex * maxGlyphsPerFile;
        int endGlyph = startGlyph + maxGlyphsPerFile;
        if (endGlyph > totalGlyphs) endGlyph = totalGlyphs;
        
        int glyphsInFile = endGlyph - startGlyph;
        
        /* Calculate grid dimensions for this file */
        int gridCols = (glyphsInFile > maxGridCols) ? maxGridCols : 
                       (glyphsInFile <= maxGridCols) ? glyphsInFile : maxGridCols;
        int gridRows = (glyphsInFile + gridCols - 1) / gridCols;
        
        /* Use consistent image dimensions for all files */
        int gridHeight = gridRows * cellHeight;
        
        /* Get Unicode range for this page */
        int startCodepoint = availableGlyphs[startGlyph];
        int endCodepoint = availableGlyphs[endGlyph - 1];
        
        printf("File %d: %dx%d pixels, %dx%d cells, %d glyphs, U+%04X–U+%04X\n",
               fileIndex + 1, imageWidth, imageHeight, gridCols, gridRows, glyphsInFile,
               startCodepoint, endCodepoint);

        /* Create grayscale image buffer */
        unsigned char *grayBuffer = calloc(imageWidth * imageHeight, sizeof(unsigned char));
        if (!grayBuffer) {
            printf("Failed to allocate image buffer\n");
            free(availableGlyphs);
            free(fontBuffer);
            exit(-1);
        }

        /* Fill with light gray background */
        memset(grayBuffer, 240, imageWidth * imageHeight);

        /* Render glyphs for this file */
        for (int i = 0; i < glyphsInFile; i++) {
            int codepoint = availableGlyphs[startGlyph + i];
            int row = i / gridCols;
            int col = i % gridCols;

            /* Get glyph bitmap */
            int glyphWidth, glyphHeight, xOffset, yOffset;
            unsigned char *glyphBitmap = stt_GetCodepointBitmap(&info, scale, scale,
                                                                 codepoint, &glyphWidth, &glyphHeight,
                                                                 &xOffset, &yOffset);

            if (glyphBitmap) {
                /* Calculate glyph position using baseline-centered approach */
                int cellX = col * cellWidth;
                int cellY = row * cellHeight;
                int glyphX = cellX + (cellWidth - glyphWidth) / 2;
                int glyphY = cellY + (int)baseline;

                /* Copy glyph bitmap to main image buffer */
                for (int gy = 0; gy < glyphHeight; gy++) {
                    for (int gx = 0; gx < glyphWidth; gx++) {
                        int imageX = glyphX + gx;
                        int imageY = glyphY + gy;

                        /* Check bounds */
                        if (imageX >= 0 && imageX < imageWidth &&
                            imageY >= 0 && imageY < imageHeight) {
                            unsigned char glyphPixel = glyphBitmap[gy * glyphWidth + gx];
                            
                            /* Render characters darker */
                            int bgPixel = grayBuffer[imageY * imageWidth + imageX];
                            int darkened = bgPixel - glyphPixel;
                            if (darkened < 0) darkened = 0;
                            grayBuffer[imageY * imageWidth + imageX] = darkened;
                        }
                    }
                }

                /* Free the glyph bitmap */
                stt_FreeBitmap(glyphBitmap, NULL);
            }
        }

        /* Draw grid lines if enabled */
        if (drawGridLines) {
            /* Vertical grid lines */
            for (int x = 0; x <= gridCols; x++) {
                int lineX = x * cellWidth;
                if (lineX < imageWidth) {
                    for (int y = 0; y < gridHeight; y++) {
                        grayBuffer[y * imageWidth + lineX] = 200;
                    }
                }
            }
            /* Horizontal grid lines */
            for (int y = 0; y <= gridRows; y++) {
                int lineY = y * cellHeight;
                if (lineY < gridHeight) {
                    for (int x = 0; x < imageWidth; x++) {
                        grayBuffer[lineY * imageWidth + x] = 200;
                    }
                }
            }
        }

        /* Render footer */
        render_footer(&info, grayBuffer, imageWidth, imageHeight, footerHeight, 
                     fontName, startCodepoint, endCodepoint);

        /* Convert grayscale to RGB */
        unsigned char *rgbBuffer = malloc(imageWidth * imageHeight * 3);
        if (!rgbBuffer) {
            printf("Failed to allocate RGB buffer\n");
            free(grayBuffer);
            free(availableGlyphs);
            free(fontBuffer);
            exit(-1);
        }

        for (int i = 0; i < imageWidth * imageHeight; i++) {
            unsigned char gray = grayBuffer[i];
            rgbBuffer[i * 3] = gray;
            rgbBuffer[i * 3 + 1] = gray;
            rgbBuffer[i * 3 + 2] = gray;
        }

        /* Create output filename with smart naming */
        char outputFilename[512];
        if (numFiles == 1) {
            /* Single file: no numeric suffix */
            snprintf(outputFilename, sizeof(outputFilename), "%s.png", finalOutputPrefix);
        } else {
            /* Multiple files: zero-padded numbering */
            snprintf(outputFilename, sizeof(outputFilename), "%s-%02d.png", finalOutputPrefix, fileIndex + 1);
        }

        /* Write PNG file */
        FILE *outFile = fopen(outputFilename, "wb");
        if (!outFile) {
            printf("Failed to create output file: %s\n", outputFilename);
            free(rgbBuffer);
            free(grayBuffer);
            free(availableGlyphs);
            free(fontBuffer);
            exit(-1);
        }

        svpng(outFile, imageWidth, imageHeight, rgbBuffer, 0);
        fclose(outFile);

        printf("Font grid saved to %s\n", outputFilename);

        /* Cleanup for this file */
        free(rgbBuffer);
        free(grayBuffer);
    }

    /* Final cleanup */
    free(availableGlyphs);
    free(fontBuffer);

    return 0;
}


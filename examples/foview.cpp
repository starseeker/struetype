/*
 * foview.cpp - Generate PNG and PDF images showing all available glyphs in a font
 *
 * This example demonstrates how to:
 * 1. Load a TrueType font using struetype.h
 * 2. Scan the entire Unicode range (0-0x10FFFF) to find all available glyphs
 * 3. Render ALL glyphs that exist in the font using stt_GetCodepointBitmap
 * 4. Arrange glyphs in a grid layout with optional grid lines
 * 5. Center each glyph visually in its grid cell using font metrics
 * 6. Render characters darker on light background using simple subtraction
 * 7. Convert grayscale bitmap to RGB format
 * 8. Generate appropriate output based on glyph count:
 *    - Single page: Both PNG and PDF output
 *    - Multiple pages: PDF-only output with all pages
 * 9. Smart output naming: single file as .png/.pdf, multiple as .pdf only
 * 10. Add footer with font name and Unicode range using embedded ProFont
 *
 * Key rendering features:
 * - Uses font ascent/descent metrics to calculate proper baseline positioning
 * - Centers glyphs horizontally and vertically for visual consistency
 * - Ignores xOffset/yOffset for cleaner visual centering
 * - Simple subtraction blending for crisp, dark text on light background
 * - Scans entire Unicode range using stt_FindGlyphIndex to find available glyphs
 * - Target maximum image size of 1500x2000 pixels (portrait, 300dpi print ready)
 * - Adaptive sizing: single-page output sized to fit content, multi-page uses uniform dimensions
 * - Footer displays font name and Unicode range using embedded ProFont
 *
 * Enhanced output logic:
 * - Single-page output: Generates both PNG and PDF files (e.g., "output.png" and "output.pdf")
 * - Multi-page output: Generates only PDF file with all pages (e.g., "output.pdf")
 * - Uses pdfimg.h minimal PDF writer for strict PDF 1.4 compliant backend
 * - Footer strip below grid showing font name and Unicode range
 * - Footer always rendered using embedded ProFont (always available, never skipped)
 * - Print-ready 1500x2000 pixel output size for multi-page (5" x 6.67" at 300dpi)
 *
 * Usage: foview [font_file] [output_prefix]
 * Defaults: foview profont/ProFont.ttf fontgrid
 * Output: 
 *   - Single page: fontgrid.png and fontgrid.pdf
 *   - Multiple pages: fontgrid.pdf only
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"
#include "svpng.h"
#include "pdfimg.h"
#include "toojpeg.h"
#include "cxxopts.hpp"

/* Embedded ProFont.ttf data for footer rendering */
#include "profont_embedded.h"

/* Structure to hold image data for later output */
typedef struct {
    unsigned char *grayBuffer;
    unsigned char *rgbBuffer;
    int width;
    int height;
    int startCodepoint;
    int endCodepoint;
    char fontName[256];
} ImageData;

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
    int *glyph_list = (int *)malloc(capacity * sizeof(int));
    
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
                int *new_list = (int *)realloc(glyph_list, capacity * sizeof(int));
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

/* Function to render footer text using embedded ProFont */
void render_footer(stt_fontinfo *mainInfo, unsigned char *buffer, int imageWidth, int imageHeight, 
                   int footerHeight, const char *fontName, int startCodepoint, int endCodepoint) {
    /* Create footer text with space between font name and Unicode range */
    char footerText[256];
    snprintf(footerText, sizeof(footerText), "Font: %s U+%04X-U+%04X", 
             fontName, startCodepoint, endCodepoint);
    
    /* Initialize embedded ProFont for footer rendering */
    stt_fontinfo footerFont;
    if (!stt_InitFont(&footerFont, profont_ttf_data, profont_ttf_data_len, 0)) {
        /* If ProFont fails to load, skip footer (shouldn't happen) */
        return;
    }
    
    /* Calculate font scaling for footer */
    float footerScale = stt_ScaleForPixelHeight(&footerFont, 14);
    
    /* Get font metrics */
    int ascent, descent, lineGap;
    stt_GetFontVMetrics(&footerFont, &ascent, &descent, &lineGap);
    
    /* Calculate text dimensions */
    int textWidth = 0;
    const char *ptr = footerText;
    while (*ptr) {
        int advance, leftSideBearing;
        stt_GetCodepointHMetrics(&footerFont, *ptr, &advance, &leftSideBearing);
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
        unsigned char *glyphBitmap = stt_GetCodepointBitmap(&footerFont, footerScale, footerScale,
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
            stt_GetCodepointHMetrics(&footerFont, *ptr, &advance, &leftSideBearing);
            currentX += (int)(advance * footerScale);
            
            /* Free the glyph bitmap */
            stt_FreeBitmap(glyphBitmap, NULL);
        }
        ptr++;
    }
}

int main(int argc, const char *argv[])
{
    /* Check for help request */
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usage: %s [font_file] [output_prefix]\n", argv[0]);
        printf("\nGenerate PNG and/or PDF images showing all available glyphs in a font.\n");
        printf("PDFs are generated with strict PDF 1.4 compliance for compatibility with\n");
        printf("viewers like Evince and other strict PDF readers.\n");
        printf("\nArguments:\n");
        printf("  font_file     Path to TrueType font file (default: profont/ProFont.ttf)\n");
        printf("  output_prefix Output file prefix (default: derived from font filename)\n");
        printf("\nOutput behavior:\n");
        printf("  Single page:  Creates both <prefix>.png and <prefix>.pdf\n");
        printf("  Multiple pages: Creates only <prefix>.pdf with all pages\n");
        printf("\nPDF Features:\n");
        printf("  - Strict PDF 1.4 compliance for maximum compatibility\n");
        printf("  - Proper object references and xref tables\n");
        printf("  - Correct stream lengths and PDF structure\n");
#ifdef PDFIMG_ENABLE_COMPRESSION
        printf("  - Compressed image data using Flate (zlib/deflate) compression\n");
        printf("  - /Filter /FlateDecode streams for significantly smaller file sizes\n");
#else
        printf("  - Uncompressed image data (compression can be enabled at compile time)\n");
#endif
        printf("\nExamples:\n");
        printf("  %s                                    # Uses default font, creates ProFont.png + ProFont.pdf\n", argv[0]);
        printf("  %s arial.ttf                         # Creates arial.png + arial.pdf (if single page)\n", argv[0]);
        printf("  %s arial.ttf myfont                  # Creates myfont.png + myfont.pdf (if single page)\n", argv[0]);
        printf("  %s large_font.ttf                    # Creates large_font.pdf only (if multiple pages)\n", argv[0]);
#ifndef PDFIMG_ENABLE_COMPRESSION
        printf("\nCompression Support:\n");
        printf("  To enable PDF image stream compression, compile with -DPDFIMG_ENABLE_COMPRESSION\n");
        printf("  This uses miniz for Flate compression and can reduce file sizes by 90%% or more.\n");
#endif
        return 0;
    }

    /* Parse command line arguments */
    const char *fontPath = (argc > 1) ? argv[1] : "profont/ProFont.ttf";
    const char *outputPrefix = (argc > 2) ? argv[2] : NULL;

    /* Configuration constants */
    const int cellWidth = 48;    /* Width of each cell in pixels */
    const int cellHeight = 48;   /* Height of each cell in pixels */
    const int fontSize = 24;     /* Font size in pixels */
    const int drawGridLines = 1; /* Draw faint grid lines */
    const int maxImageWidth = 1500;  /* Maximum image width (portrait orientation) */
    const int maxImageHeight = 2000; /* Maximum image height */
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

    unsigned char *fontBuffer = (unsigned char *)calloc(size, sizeof(unsigned char));
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

    /* Calculate image dimensions based on number of files needed */
    int imageWidth, imageHeight;
    
    if (numFiles == 1) {
        /* Single page: size just large enough for grid + footer */
        int actualGlyphs = totalGlyphs;
        int actualCols = (actualGlyphs > maxGridCols) ? maxGridCols : actualGlyphs;
        int actualRows = (actualGlyphs + actualCols - 1) / actualCols;
        
        imageWidth = actualCols * cellWidth;
        imageHeight = actualRows * cellHeight + footerHeight;
        
        /* Ensure minimum reasonable size */
        if (imageWidth < 200) imageWidth = 200;
        if (imageHeight < 200) imageHeight = 200;
    } else {
        /* Multi-page: use uniform maximum dimensions for all pages */
        imageWidth = maxImageWidth;
        imageHeight = maxImageHeight;
    }

    /* Calculate font scaling - converts font units to pixels */
    float scale = stt_ScaleForPixelHeight(&info, fontSize);

    /* Get font metrics for baseline calculation */
    int ascent, descent, lineGap;
    stt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    
    /* Calculate baseline position to center font vertically in cell */
    float baseline = (cellHeight / 2.0f) + ((ascent - descent) / 2.0f * scale) - (ascent * scale);

    /* Allocate array to store image data for all pages */
    ImageData *images = (ImageData *)malloc(numFiles * sizeof(ImageData));
    if (!images) {
        printf("Failed to allocate memory for image data\n");
        free(availableGlyphs);
        free(fontBuffer);
        exit(-1);
    }

    /* Generate image data for each file */
    for (int fileIndex = 0; fileIndex < numFiles; fileIndex++) {
        int startGlyph = fileIndex * maxGlyphsPerFile;
        int endGlyph = startGlyph + maxGlyphsPerFile;
        if (endGlyph > totalGlyphs) endGlyph = totalGlyphs;
        
        int glyphsInFile = endGlyph - startGlyph;
        
        /* Calculate grid dimensions for this file */
        int gridCols, gridRows;
        
        if (numFiles == 1) {
            /* Single page: use actual dimensions needed */
            gridCols = (glyphsInFile > maxGridCols) ? maxGridCols : glyphsInFile;
            gridRows = (glyphsInFile + gridCols - 1) / gridCols;
        } else {
            /* Multi-page: use maximum grid dimensions */
            gridCols = (glyphsInFile > maxGridCols) ? maxGridCols : 
                       (glyphsInFile <= maxGridCols) ? glyphsInFile : maxGridCols;
            gridRows = (glyphsInFile + gridCols - 1) / gridCols;
        }
        
        /* Use consistent image dimensions for all files */
        int gridHeight = gridRows * cellHeight;
        
        /* Get Unicode range for this page */
        int startCodepoint = availableGlyphs[startGlyph];
        int endCodepoint = availableGlyphs[endGlyph - 1];
        
        printf("File %d: %dx%d pixels, %dx%d cells, %d glyphs, U+%04X–U+%04X\n",
               fileIndex + 1, imageWidth, imageHeight, gridCols, gridRows, glyphsInFile,
               startCodepoint, endCodepoint);

        /* Create grayscale image buffer */
        unsigned char *grayBuffer = (unsigned char *)calloc(imageWidth * imageHeight, sizeof(unsigned char));
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
        unsigned char *rgbBuffer = (unsigned char *)malloc(imageWidth * imageHeight * 3);
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

        /* Store image data for later output */
        images[fileIndex].grayBuffer = grayBuffer;
        images[fileIndex].rgbBuffer = rgbBuffer;
        images[fileIndex].width = imageWidth;
        images[fileIndex].height = imageHeight;
        images[fileIndex].startCodepoint = startCodepoint;
        images[fileIndex].endCodepoint = endCodepoint;
        strcpy(images[fileIndex].fontName, fontName);
    }

    /* Now generate output based on number of pages */
    if (numFiles == 1) {
        /* Single page: Generate both PNG and PDF */
        ImageData *img = &images[0];
        
        /* Generate PNG file */
        char pngFilename[512];
        snprintf(pngFilename, sizeof(pngFilename), "%s.png", finalOutputPrefix);
        
        FILE *pngFile = fopen(pngFilename, "wb");
        if (!pngFile) {
            printf("Failed to create PNG file: %s\n", pngFilename);
        } else {
            svpng(pngFile, img->width, img->height, img->rgbBuffer, 0);
            fclose(pngFile);
            printf("Font grid saved to %s\n", pngFilename);
        }
        
        /* Generate PDF file */
        char pdfFilename[512];
        snprintf(pdfFilename, sizeof(pdfFilename), "%s.pdf", finalOutputPrefix);
        
        pdfimg_doc_t *pdf = pdfimg_create();
        if (pdf) {
            pdfimg_add_image_page(pdf, img->rgbBuffer, img->width, img->height, 
                                img->width * 3, 1, 72.0); /* 72 DPI for screen viewing */
            if (pdfimg_save(pdf, pdfFilename)) {
                printf("Font grid saved to %s\n", pdfFilename);
            } else {
                printf("Failed to save PDF file: %s\n", pdfFilename);
            }
            pdfimg_free(pdf);
        }
        
    } else {
        /* Multiple pages: Generate only PDF with all pages */
        char pdfFilename[512];
        snprintf(pdfFilename, sizeof(pdfFilename), "%s.pdf", finalOutputPrefix);
        
        pdfimg_doc_t *pdf = pdfimg_create();
        if (pdf) {
            for (int i = 0; i < numFiles; i++) {
                ImageData *img = &images[i];
                pdfimg_add_image_page(pdf, img->rgbBuffer, img->width, img->height, 
                                    img->width * 3, 1, 72.0); /* 72 DPI for screen viewing */
            }
            if (pdfimg_save(pdf, pdfFilename)) {
                printf("Multi-page font grid saved to %s (%d pages)\n", pdfFilename, numFiles);
            } else {
                printf("Failed to save PDF file: %s\n", pdfFilename);
            }
            pdfimg_free(pdf);
        }
    }

    /* Cleanup image data */
    for (int i = 0; i < numFiles; i++) {
        free(images[i].grayBuffer);
        free(images[i].rgbBuffer);
    }
    free(images);

    /* Final cleanup */
    free(availableGlyphs);
    free(fontBuffer);

    return 0;
}


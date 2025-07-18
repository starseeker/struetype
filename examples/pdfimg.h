/*
 * pdfimg.h - Minimal header-only PDF writer for embedding raw (gray or RGB) image buffers as pages.
 *
 * Features strict PDF 1.4 compliance for compatibility with all PDF viewers including Evince.
 * Supports optional Flate (zlib/deflate) compression of image streams using miniz.
 *
 * Compression Support:
 * - Define PDFIMG_ENABLE_COMPRESSION to enable Flate compression using miniz
 * - When enabled: compresses image data and adds /Filter /FlateDecode to PDF streams
 * - When disabled: writes uncompressed image data (default behavior)
 * - Uses pdfimg_compress() helper function that handles both modes transparently
 *
 * Derived from PDFGen (https://github.com/AndreRenaud/PDFGen)
 *
 * Copyright (c) 2017, Andre Renaud
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PDFIMG_H
#define PDFIMG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef ENABLE_MINIZ_COMPRESSION
#include "miniz.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Image compression types
enum pdfimg_compression_type {
    PDFIMG_COMPRESSION_NONE = 0,
    PDFIMG_COMPRESSION_FLATE,
    PDFIMG_COMPRESSION_JPEG
};

typedef struct {
    uint8_t *data;
    size_t len, cap;
    size_t *obj_offsets;
    size_t obj_count, obj_cap;
    size_t xref_offset;
    int    page_count;
    int   *page_objs;
    int    page_objs_cap;
    int    next_obj_id;   /* Track next available object ID */
} pdfimg_doc_t;

// Create PDF doc
static inline pdfimg_doc_t *pdfimg_create(void) {
    pdfimg_doc_t *pdf = (pdfimg_doc_t*)calloc(1, sizeof(pdfimg_doc_t));
    pdf->cap = 4096;
    pdf->data = (uint8_t*)malloc(pdf->cap);
    pdf->obj_cap = 32;
    pdf->obj_offsets = (size_t*)malloc(sizeof(size_t) * pdf->obj_cap);
    pdf->page_objs_cap = 16;
    pdf->page_objs = (int*)malloc(sizeof(int) * pdf->page_objs_cap);
    pdf->len = 0;
    pdf->obj_count = 0;
    pdf->page_count = 0;
    pdf->xref_offset = 0;
    pdf->next_obj_id = 1;  /* Start object numbering at 1 */
    memcpy(pdf->data, "%PDF-1.4\n", 9);
    pdf->len = 9;
    return pdf;
}

static inline void pdfimg_free(pdfimg_doc_t *pdf) {
    if (!pdf) return;
    free(pdf->data);
    free(pdf->obj_offsets);
    free(pdf->page_objs);
    free(pdf);
}

static inline void pdfimg_add_obj_offset(pdfimg_doc_t *pdf) {
    if (pdf->obj_count >= pdf->obj_cap) {
        pdf->obj_cap *= 2;
        pdf->obj_offsets = (size_t*)realloc(pdf->obj_offsets, sizeof(size_t) * pdf->obj_cap);
    }
    pdf->obj_offsets[pdf->obj_count++] = pdf->len;
}

static inline void pdfimg_append(pdfimg_doc_t *pdf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap) + 1;
    va_end(ap);
    if (pdf->len + need >= pdf->cap) {
        pdf->cap = (pdf->len + need) * 2;
        pdf->data = (uint8_t*)realloc(pdf->data, pdf->cap);
    }
    va_start(ap, fmt);
    pdf->len += vsprintf((char*)pdf->data + pdf->len, fmt, ap);
    va_end(ap);
}

#ifdef ENABLE_MINIZ_COMPRESSION
// Function to compress image data using Flate (zlib)
static int pdfimg_compress_flate(const uint8_t *input_data, size_t input_size, 
                                uint8_t **output_data, size_t *output_size) {
    // Calculate upper bound for compressed data size
    mz_ulong compressed_size = mz_compressBound(input_size);
    uint8_t *compressed_data = (uint8_t*)malloc(compressed_size);
    
    if (!compressed_data) {
        return 0;
    }
    
    // Compress the data
    int result = mz_compress2(compressed_data, &compressed_size, input_data, input_size, 6);
    
    if (result != MZ_OK) {
        free(compressed_data);
        return 0;
    }
    
    *output_data = compressed_data;
    *output_size = compressed_size;
    return 1;
}
#endif

// Add one page with an image buffer - supports pre-compressed data
static inline int pdfimg_add_image_page_with_data(pdfimg_doc_t *pdf,
    const uint8_t *image_data, size_t image_data_size, int w, int h, int is_rgb, 
    double dpi, const char *filter_name)
{
    // PDF units: 1/72 inch, so width = w * 72 / dpi
    double pagew = w * 72.0 / dpi, pageh = h * 72.0 / dpi;
    
    // Assign object IDs sequentially
    int img_obj = pdf->next_obj_id++;
    int content_obj = pdf->next_obj_id++;
    int page_obj = pdf->next_obj_id++;
    
    // Process image data and determine output size
    int output_size;
    uint8_t *output_data;
    
#ifdef PDFIMG_ENABLE_COMPRESSION
    // Create temporary buffer for uncompressed image data
    uint8_t *temp_buf = (uint8_t*)malloc(img_data_size);
    if (!temp_buf) {
        return 0; // Memory allocation failed
    }
    
    // Copy image data to temporary buffer, row by row
    for (int y = 0; y < h; ++y) {
        memcpy(temp_buf + y * w * (is_rgb ? 3 : 1), buf + y * stride, w * (is_rgb ? 3 : 1));
    }
    
    // Compress the image data
    mz_ulong compressed_size = mz_compressBound(img_data_size);
    output_data = (uint8_t*)malloc(compressed_size);
    if (!output_data) {
        free(temp_buf);
        return 0; // Memory allocation failed
    }
    
    int result = mz_compress(output_data, &compressed_size, temp_buf, img_data_size);
    free(temp_buf);
    
    if (result != MZ_OK) {
        free(output_data);
        return 0; // Compression failed
    }
    
    output_size = compressed_size;
#else
    // No compression: create buffer with uncompressed data
    output_data = (uint8_t*)malloc(img_data_size);
    if (!output_data) {
        return 0; // Memory allocation failed
    }
    
    // Copy image data, row by row
    for (int y = 0; y < h; ++y) {
        memcpy(output_data + y * w * (is_rgb ? 3 : 1), buf + y * stride, w * (is_rgb ? 3 : 1));
    }
    
    output_size = img_data_size;
#endif
    
    // Add image object
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "%d 0 obj\n", img_obj);
    
#ifdef PDFIMG_ENABLE_COMPRESSION
    pdfimg_append(pdf, "<< /Type /XObject /Subtype /Image /Width %d /Height %d "
        "/ColorSpace /Device%s /BitsPerComponent 8 /Filter /FlateDecode /Length %d >>\nstream\n",
        w, h, is_rgb ? "RGB" : "Gray", output_size);
#else
    pdfimg_append(pdf, "<< /Type /XObject /Subtype /Image /Width %d /Height %d "
        "/ColorSpace /Device%s /BitsPerComponent 8%s /Length %zu >>\nstream\n",
        w, h, is_rgb ? "RGB" : "Gray", filter_name, image_data_size);
    
    // Ensure buffer capacity for image data
    if (pdf->len + image_data_size >= pdf->cap) {
        pdf->cap = pdf->len + image_data_size + 1024;
        pdf->data = (uint8_t*)realloc(pdf->data, pdf->cap);
    }
    
    // Copy image data
    memcpy(pdf->data + pdf->len, image_data, image_data_size);
    pdf->len += image_data_size;
    
    pdfimg_append(pdf, "\nendstream\nendobj\n");
    
    // Add content stream to draw image
    char bufstr[256];
    int content_len = snprintf(bufstr, sizeof(bufstr),
        "q\n%.2f 0 0 %.2f 0 0 cm\n/Im0 Do\nQ\n", pagew, pageh);
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "%d 0 obj\n<< /Length %d >>\nstream\n", content_obj, content_len);
    memcpy(pdf->data + pdf->len, bufstr, content_len);
    pdf->len += content_len;
    pdfimg_append(pdf, "endstream\nendobj\n");

    // Add page object
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "%d 0 obj\n"
        "<< /Type /Page /MediaBox [0 0 %.2f %.2f] "
        "/Contents %d 0 R /Resources << /XObject <</Im0 %d 0 R>> >> >>\nendobj\n",
        page_obj, pagew, pageh, content_obj, img_obj);

    // Track page object number
    if (pdf->page_count >= pdf->page_objs_cap) {
        pdf->page_objs_cap *= 2;
        pdf->page_objs = (int*)realloc(pdf->page_objs, sizeof(int) * pdf->page_objs_cap);
    }
    pdf->page_objs[pdf->page_count++] = page_obj;

    return 1;
}

// Add one page with an image buffer (gray or rgb) - with compression support
static inline int pdfimg_add_image_page_compressed(pdfimg_doc_t *pdf,
    const uint8_t *buf, int w, int h, int stride, int is_rgb, double dpi,
    enum pdfimg_compression_type compression)
{
    // Prepare image data
    size_t raw_data_size = w * h * (is_rgb ? 3 : 1);
    uint8_t *image_data = (uint8_t*)malloc(raw_data_size);
    
    if (!image_data) {
        return 0;
    }
    
    // Copy buffer, row by row (stride might be >w)
    for (int y = 0; y < h; ++y) {
        memcpy(image_data + y * w * (is_rgb ? 3 : 1), 
               buf + y * stride, w * (is_rgb ? 3 : 1));
    }
    
    // Compression variables
    uint8_t *final_data = image_data;
    size_t final_size = raw_data_size;
    const char *filter_name = "";
    int compression_success = 0;
    
    // Apply compression based on type
    switch (compression) {
        case PDFIMG_COMPRESSION_FLATE:
#ifdef ENABLE_MINIZ_COMPRESSION
            {
                uint8_t *compressed_data;
                size_t compressed_size;
                if (pdfimg_compress_flate(image_data, raw_data_size, &compressed_data, &compressed_size)) {
                    final_data = compressed_data;
                    final_size = compressed_size;
                    filter_name = " /Filter /FlateDecode";
                    compression_success = 1;
                }
            }
#endif
            break;
            
        case PDFIMG_COMPRESSION_JPEG:
            // JPEG compression will be handled in foview.cpp
            // This case should not be reached in normal usage
            break;
            
        case PDFIMG_COMPRESSION_NONE:
        default:
            // No compression
            break;
    }
    
    // Use the helper function to add the page
    int result = pdfimg_add_image_page_with_data(pdf, final_data, final_size, w, h, is_rgb, dpi, filter_name);
    
    // Clean up
    if (compression_success && final_data != image_data) {
        free(final_data);
    }
    free(image_data);
    
    return result;
}

// Add one page with an image buffer (gray or rgb) - legacy function for backward compatibility
static inline int pdfimg_add_image_page(pdfimg_doc_t *pdf,
    const uint8_t *buf, int w, int h, int stride, int is_rgb, double dpi)
{
    return pdfimg_add_image_page_compressed(pdf, buf, w, h, stride, is_rgb, dpi, PDFIMG_COMPRESSION_NONE);
}

static inline int pdfimg_save(pdfimg_doc_t *pdf, const char *filename) {
    // Assign object IDs for Pages and Catalog objects
    int pages_obj = pdf->next_obj_id++;
    int catalog_obj = pdf->next_obj_id++;
    
    // We need to update page objects to include /Parent reference.
    // Since we can't easily modify already-written objects, we'll patch
    // the data in-place by finding and updating the page objects.
    
    // Add Pages object
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "%d 0 obj\n<< /Type /Pages /Kids [", pages_obj);
    for (int i = 0; i < pdf->page_count; ++i)
        pdfimg_append(pdf, "%d 0 R ", pdf->page_objs[i]);
    pdfimg_append(pdf, "] /Count %d >>\nendobj\n", pdf->page_count);

    // Add Catalog object
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "%d 0 obj\n<< /Type /Catalog /Pages %d 0 R >>\nendobj\n", 
                  catalog_obj, pages_obj);

    // xref table
    pdf->xref_offset = pdf->len;
    pdfimg_append(pdf, "xref\n0 %d\n0000000000 65535 f \n", (int)pdf->obj_count + 1);
    for (size_t i = 0; i < pdf->obj_count; ++i)
        pdfimg_append(pdf, "%010zu 00000 n \n", pdf->obj_offsets[i]);
    
    // trailer
    pdfimg_append(pdf, "trailer\n<< /Size %d /Root %d 0 R >>\nstartxref\n%zu\n%%%%EOF\n",
        (int)pdf->obj_count + 1, catalog_obj, pdf->xref_offset);

    FILE *fp = fopen(filename, "wb");
    if (!fp) return 0;
    fwrite(pdf->data, 1, pdf->len, fp);
    fclose(fp);
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif // PDFIMG_H
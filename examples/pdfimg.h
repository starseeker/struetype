/*
 * pdfimg.h - Minimal header-only PDF writer for embedding raw (gray or RGB) image buffers as pages.
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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data;
    size_t len, cap;
    size_t *obj_offsets;
    size_t obj_count, obj_cap;
    size_t xref_offset;
    int    page_count;
    int   *page_objs;
    int    page_objs_cap;
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

// Add one page with an image buffer (gray or rgb)
static inline int pdfimg_add_image_page(pdfimg_doc_t *pdf,
    const uint8_t *buf, int w, int h, int stride, int is_rgb, double dpi)
{
    // PDF units: 1/72 inch, so width = w * 72 / dpi
    double pagew = w * 72.0 / dpi, pageh = h * 72.0 / dpi;
    int img_obj = (int)pdf->obj_count + 1;
    int content_obj = img_obj + 1;
    int page_obj = content_obj + 1;

    // Add image object
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "%d 0 obj\n", img_obj);
    pdfimg_append(pdf, "<< /Type /XObject /Subtype /Image /Width %d /Height %d "
        "/ColorSpace /Device%s /BitsPerComponent 8 /Length %d >>\nstream\n",
        w, h, is_rgb ? "RGB" : "Gray", w*h*(is_rgb?3:1));
    size_t offset = pdf->len;
    if (pdf->len + w*h*(is_rgb?3:1) >= pdf->cap)
        pdf->data = (uint8_t*)realloc(pdf->data, pdf->len + w*h*(is_rgb?3:1) + 1024);
    // Copy buffer, row by row (stride might be >w)
    for (int y=0; y<h; ++y) {
        memcpy(pdf->data + pdf->len, buf + y*stride, w*(is_rgb?3:1));
        pdf->len += w*(is_rgb?3:1);
    }
    pdfimg_append(pdf, "\nendstream\nendobj\n");

    // Add content stream to draw image
    char bufstr[256];
    int n = snprintf(bufstr, sizeof(bufstr),
        "q\n%.2f 0 0 %.2f 0 0 cm\n/Im0 Do\nQ\n", pagew, pageh);
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "%d 0 obj\n<< /Length %d >>\nstream\n", content_obj, n);
    memcpy(pdf->data + pdf->len, bufstr, n);
    pdf->len += n;
    pdfimg_append(pdf, "endstream\nendobj\n");

    // Add page object
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "%d 0 obj\n"
        "<< /Type /Page /Parent 1 0 R /MediaBox [0 0 %.2f %.2f] "
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

static inline int pdfimg_save(pdfimg_doc_t *pdf, const char *filename) {
    // Add Pages object (id=1)
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "1 0 obj\n<< /Type /Pages /Kids [");
    for (int i=0; i<pdf->page_count; ++i)
        pdfimg_append(pdf, "%d 0 R ", pdf->page_objs[i]);
    pdfimg_append(pdf, "] /Count %d >>\nendobj\n", pdf->page_count);

    // Add Catalog object (id=2)
    pdfimg_add_obj_offset(pdf);
    pdfimg_append(pdf, "2 0 obj\n<< /Type /Catalog /Pages 1 0 R >>\nendobj\n");

    // xref table
    pdf->xref_offset = pdf->len;
    pdfimg_append(pdf, "xref\n0 %zu\n0000000000 65535 f \n", pdf->obj_count+1);
    for (size_t i=0; i<pdf->obj_count; ++i)
        pdfimg_append(pdf, "%010zu 00000 n \n", pdf->obj_offsets[i]);
    pdfimg_append(pdf, "trailer\n<< /Size %zu /Root 2 0 R >>\nstartxref\n%zu\n%%%%EOF\n",
        pdf->obj_count+1, pdf->xref_offset);

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
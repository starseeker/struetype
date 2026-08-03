#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"

static void
exercise_font(const unsigned char *data, int size)
{
    stt_fontinfo info = {0};
    stt_vertex *shape = NULL;
    unsigned char *bitmap;
    int glyph, count, advance, bearing;
    int width, height, xoff, yoff;
    int name_length = 0;

    if (!stt_InitFont(&info, data, size, 0))
        return;

    glyph = stt_FindGlyphIndex(&info, 'A');
    stt_GetGlyphHMetrics(&info, glyph, &advance, &bearing);
    (void)advance;
    (void)bearing;
    (void)stt_GetGlyphKernAdvance(&info, glyph, glyph);
    count = stt_GetGlyphShape(&info, glyph, &shape);
    if (count > 0 && !shape)
        abort();
    stt_FreeShape(&info, shape);

    bitmap = stt_GetGlyphBitmap(&info, 0.02f, 0.02f, glyph,
                                &width, &height, &xoff, &yoff);
    stt_FreeBitmap(bitmap, info.userdata);
    (void)stt_GetFontNameString(&info, &name_length,
                                STT_PLATFORM_ID_MICROSOFT,
                                STT_MS_EID_UNICODE_BMP,
                                STT_MS_LANG_ENGLISH,
                                1);
}

static void
exercise_rendering_apis(const unsigned char *data, int size)
{
    unsigned char *atlas;
    stt_bakedchar baked[96];
    stt_packedchar packed[96];
    stt_pack_context context;
    stt_aligned_quad quad;
    stt_fontinfo info = {0};
    unsigned char filtered[64];
    float x = 0.0f, y = 0.0f, sub_x = 1.0f, sub_y = 1.0f;
    int offset;

    atlas = (unsigned char *)malloc(512U*512U);
    if (!atlas)
        abort();
    offset = stt_GetFontOffsetForIndex(data, size, 0);
    if (offset < 0 || !stt_InitFont(&info, data, size, offset))
        abort();

    if (stt_BakeFontBitmap(data, size, offset, 20.0f, atlas, 512, 512,
                           32, 96, baked) <= 0)
        abort();
    stt_GetBakedQuad(baked, 512, 512, 33, &x, &y, &quad, 1);

    if (stt_PackBegin(NULL, atlas, 512, 512, 0, 1, NULL) != 0 ||
        stt_PackBegin(&context, atlas, 0, 512, 0, 1, NULL) != 0 ||
        stt_PackBegin(&context, atlas, 512, 512, 511, 1, NULL) != 0 ||
        !stt_PackBegin(&context, atlas, 512, 512, 0, 1, NULL))
        abort();
    stt_PackSetOversampling(&context, 0, 0);
    if (context.h_oversample != 1 || context.v_oversample != 1)
        abort();
    stt_PackSetOversampling(&context, 2, 2);
    if (!stt_PackFontRange(&context, data, size, 0, 20.0f, 32, 96,
                           packed))
        abort();
    x = y = 0.0f;
    stt_GetPackedQuad(packed, 512, 512, 33, &x, &y, &quad, 1);
    stt_PackEnd(&context);

    memset(filtered, 0, sizeof(filtered));
    stt_MakeGlyphBitmapSubpixelPrefilter(&info, filtered, 8, 8, 8,
                                         0.02f, 0.02f, 0.0f, 0.0f,
                                         0, 1, &sub_x, &sub_y,
                                         stt_FindGlyphIndex(&info, 'A'));
    if (sub_x != 0.0f || sub_y != 0.0f)
        abort();
    free(atlas);
}

int
main(int argc, char **argv)
{
    FILE *fp;
    long file_size;
    unsigned char *font, *mutated;
    size_t i;

    if (argc != 2)
        return 1;
    fp = fopen(argv[1], "rb");
    if (!fp || fseek(fp, 0, SEEK_END) != 0)
        return 1;
    file_size = ftell(fp);
    if (file_size <= 0 || file_size > 0x7fffffffL ||
        fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 1;
    }
    font = (unsigned char *)malloc((size_t)file_size);
    mutated = (unsigned char *)malloc((size_t)file_size);
    if (!font || !mutated ||
        fread(font, (size_t)file_size, 1, fp) != 1) {
        free(font);
        free(mutated);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    if (stt_GetNumberOfFonts(NULL, 0) != 0 ||
        stt_GetFontOffsetForIndex(NULL, 0, 0) != -1 ||
        stt_FindMatchingFont(NULL, 0, "none", 0) != -1 ||
        stt_GetNumberOfFonts(font, (int)file_size) != 1 ||
        stt_GetFontOffsetForIndex(font, (int)file_size, 0) != 0 ||
        stt_GetFontOffsetForIndex(font, (int)file_size, 1) != -1)
        abort();

    exercise_font(font, (int)file_size);
    exercise_rendering_apis(font, (int)file_size);

    /* Every truncation must either be rejected or remain safe to query. */
    for (i = 0; i <= (size_t)file_size; ++i)
        exercise_font(font, (int)i);

    /* Deterministically corrupt table headers and payload throughout. */
    for (i = 0; i < (size_t)file_size; i += 53) {
        size_t changed = (size_t)file_size-i < 4 ?
            (size_t)file_size-i : 4;
        memcpy(mutated, font, (size_t)file_size);
        memset(mutated+i, 0xff, changed);
        exercise_font(mutated, (int)file_size);
    }

    free(mutated);
    free(font);
    return 0;
}

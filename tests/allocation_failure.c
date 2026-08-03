#include <stdio.h>
#include <stdlib.h>

static int allocation_count;
static int fail_at;

static void *
test_malloc(size_t size, void *context)
{
    (void)context;
    if (allocation_count++ == fail_at)
        return NULL;
    return malloc(size);
}

static void
test_free(void *memory, void *context)
{
    (void)context;
    free(memory);
}

#define STT_malloc(size, context) test_malloc((size), (context))
#define STT_free(memory, context) test_free((memory), (context))
#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"

static void
exercise_failures(const unsigned char *font, int size)
{
    stt_fontinfo info = {0};
    stt_pack_context pack;
    stt_packedchar chars[96];
    stt_vertex *shape = NULL;
    unsigned char atlas[256*256];
    unsigned char *bitmap, *sdf;
    int glyph, width, height, xoff, yoff;

    if (!stt_InitFont(&info, font, size, 0))
        return;
    glyph = stt_FindGlyphIndex(&info, 'A');
    (void)stt_GetGlyphShape(&info, glyph, &shape);
    stt_FreeShape(&info, shape);
    bitmap = stt_GetGlyphBitmap(&info, 0.02f, 0.02f, glyph,
                                &width, &height, &xoff, &yoff);
    stt_FreeBitmap(bitmap, info.userdata);
    sdf = stt_GetGlyphSDF(&info, 0.02f, glyph, 2, 128, 16.0f,
                          &width, &height, &xoff, &yoff);
    stt_FreeSDF(sdf, info.userdata);
    if (stt_PackBegin(&pack, atlas, 256, 256, 0, 1, NULL)) {
        (void)stt_PackFontRange(&pack, font, size, 0, 14.0f, 32, 96,
                                chars);
        stt_PackEnd(&pack);
    }
}

int
main(int argc, char **argv)
{
    FILE *fp;
    long file_size;
    unsigned char *font;
    int i;

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
    if (!font || fread(font, (size_t)file_size, 1, fp) != 1) {
        free(font);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    for (i = 0; i < 96; ++i) {
        allocation_count = 0;
        fail_at = i;
        exercise_failures(font, (int)file_size);
    }

    free(font);
    return 0;
}

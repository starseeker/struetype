#include <stdio.h>
#include <stdlib.h>
#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"

int main(int argc, const char *argv[])
{
    FILE *fontFile;
    long size;
    unsigned char *fontBuffer;
    stt_fontinfo info = {0};

    if (argc != 2)
        return 1;
    fontFile = fopen(argv[1], "rb");
    if (!fontFile)
        return 1;
    if (fseek(fontFile, 0, SEEK_END) != 0) {
        fclose(fontFile);
        return 1;
    }
    size = ftell(fontFile); /* how long is the file ? */
    if (size <= 0 || size > 0x7fffffffL || fseek(fontFile, 0, SEEK_SET) != 0) {
        fclose(fontFile);
        return 1;
    }

    fontBuffer = calloc((size_t)size, sizeof(unsigned char));
    if (!fontBuffer || fread(fontBuffer, (size_t)size, 1, fontFile) != 1) {
        free(fontBuffer);
        fclose(fontFile);
        return 1;
    }
    fclose(fontFile);

    if (!stt_InitFont(&info, fontBuffer, (int)size, 0)) {
        free(fontBuffer);
        return 0;
    }

    free(fontBuffer);
    return 0;
}

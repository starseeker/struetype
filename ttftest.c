#include <stdio.h>
#include <stdlib.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int main(int argc, const char *argv[])
{
    FILE *fontFile = fopen(argv[1], "rb");
    fseek(fontFile, 0, SEEK_END);
    long size = ftell(fontFile); /* how long is the file ? */
    fseek(fontFile, 0, SEEK_SET); /* reset */

    unsigned char *fontBuffer = calloc(size, sizeof(unsigned char));
    fread(fontBuffer, size, 1, fontFile);
    fclose(fontFile);

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, fontBuffer, 0))
        printf("failed\n");

    free(fontBuffer);
    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#define STRUETYPE_IMPLEMENTATION
#include "struetype.h"
#include "svpng.h"

int main(int argc, const char *argv[])
{
    FILE *fontFile = fopen(argv[1], "rb");
    if (!fontFile) {
       printf("failed to open %s\n", argv[1]);
       exit(-1);
    }

    fseek(fontFile, 0, SEEK_END);
    long size = ftell(fontFile); /* how long is the file ? */
    fseek(fontFile, 0, SEEK_SET); /* reset */

    unsigned char *fontBuffer = calloc(size, sizeof(unsigned char));
    fread(fontBuffer, size, 1, fontFile);
    fclose(fontFile);

    stt_fontinfo info;
    if (!stt_InitFont(&info, fontBuffer, size, 0)) {
        printf("failed\n");
	exit(-1);
    }

    free(fontBuffer);
    return 0;
}


// test_stb_load_bmp.c
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "picasso.h"
#include <stdio.h>


#define STBI_ONLY_BMP

int main(void) {
    int width1, height1, channels1;
    int width2, height2, channels2;
    int width3, height3, channels3;
    int width4, height4, channels4;
    uint8_t *data1 = stbi_load("sample1.bmp", &width1, &height1, &channels1, 4);
    uint8_t *data2 = stbi_load("sample2.bmp", &width2, &height2, &channels2, 4);
    uint8_t *data3 = stbi_load("sample3.bmp", &width3, &height3, &channels3, 4);
    uint8_t *data4 = stbi_load("unknown.bmp", &width4, &height4, &channels4, 4);

    BMP *bmp1 = picasso_create_bmp_from_rgba(width1, height1, data1);
    BMP *bmp2 = picasso_create_bmp_from_rgba(width2, height2, data2);
    BMP *bmp3 = picasso_create_bmp_from_rgba(width3, height3, data3);
    BMP *bmp4 = picasso_create_bmp_from_rgba(width4, height4, data4);

    picasso_save_to_bmp(bmp1, "from_stb_saved1.bmp", PICASSO_PROFILE_SRGB);
    picasso_save_to_bmp(bmp2, "from_stb_saved2.bmp", PICASSO_PROFILE_SRGB);
    picasso_save_to_bmp(bmp3, "from_stb_saved3.bmp", PICASSO_PROFILE_SRGB);
    picasso_save_to_bmp(bmp4, "from_stb_saved4.bmp", PICASSO_PROFILE_SRGB);

    return 0;
}


#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_bmp

#include "stb_image.h"
#include "stb_image_write.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "picasso.h"

int main(void) {
    // === STB: Load → save ===
    int stb_width, stb_height, stb_channels;
    uint8_t *stb_data = stbi_load("../rgb32.bmp", &stb_width, &stb_height, &stb_channels, 0);
    if (!stb_data) {
        fprintf(stderr, "Failed to load image with stb: %s\n", stbi_failure_reason());
        return 1;
    }

    if (!stbi_write_bmp("stb_roundtrip.bmp", stb_width, stb_height, stb_channels, stb_data)) {
        fprintf(stderr, "Failed to save stb_roundtrip.bmp\n");
        stbi_image_free(stb_data);
        return 1;
    }

    bmp *stb_bmp = picasso_create_bmp_from_rgba(stb_width, stb_height, stb_channels, stb_data);
    if (!stb_bmp) {
        fprintf(stderr, "Failed to create bmp from stb data\n");
        stbi_image_free(stb_data);
        return 1;
    }

    picasso_save_to_bmp(stb_bmp, "stb_loaded_by_picasso.bmp", PICASSO_PROFILE_NONE);
    stbi_image_free(stb_data);
    // Optionally: picasso_free_bmp(stb_bmp);

    // === Picasso: Load → save ===
    picasso_image *picasso_loaded = picasso_load_bmp("../rgb32.bmp");
    if (!picasso_loaded) {
        fprintf(stderr, "Failed to load image with Picasso\n");
        return 1;
    }

    if (!stbi_write_bmp("picasso_saved_by_stb.bmp",
                        picasso_loaded->width,
                        picasso_loaded->height,
                        picasso_loaded->channels,
                        picasso_loaded->pixels)) {
        fprintf(stderr, "Failed to save picasso_saved_by_stb.bmp\n");
        return 1;
    }

    bmp *picasso_bmp = picasso_create_bmp_from_rgba(picasso_loaded->width,
                                                    picasso_loaded->height,
                                                    picasso_loaded->channels,
                                                    picasso_loaded->pixels);
    if (!picasso_bmp) {
        fprintf(stderr, "Failed to create bmp from Picasso-loaded image\n");
        return 1;
    }

    picasso_save_to_bmp(picasso_bmp, "picasso_roundtrip.bmp", PICASSO_PROFILE_NONE);
    // Optionally: free picasso_image and bmp here

    printf("All roundtrip tests complete.\n");
    return 0;
}

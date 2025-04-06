#include <stdio.h>
#include <string.h>
#include "picasso.h"
#include "logger.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        ERROR("Usage: %s <bmp file>", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    const char *basename = strrchr(filepath, '/');
    if (basename) basename++; else basename = filepath; // skip past last '/'

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        TRACE("File doesn't exist: %s", filepath);
        return 1;
    }
    fclose(fp);

    char output_name[512];
    snprintf(output_name, sizeof(output_name), "test_%s", basename);

    picasso_image *img = picasso_load_bmp(filepath);
    picasso_image *p_img = picasso_load_ppm("triangle.ppm");

    ppm triangle = {
        .width = p_img->width,
        .height = p_img->height,
        .maxval = 255,
        .pixels = p_img->pixels,
    };
    picasso_save_to_ppm(&triangle, "triangel2.ppm");
    if (!img) {
        INFO("Failed to load BMP: %s", filepath);
        return -1;
    }

    bmp *b = picasso_create_bmp_from_rgba(img->width, img->height, img->channels, img->pixels);
    if (b) {
        picasso_save_to_bmp(b, output_name, PICASSO_PROFILE_NONE);
        TRACE("Saved BMP to: %s", output_name);
    } else {
        ERROR("Failed to create BMP from RGBA");
    }

    return 0;
}

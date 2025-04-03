#include <stdio.h>
#include "picasso.h"
#include "logger.h"

int main(int argc, char **argv)
{
    (void)argc;
    char *filename = argv[1];
    FILE *fp = fopen(filename, "r");
    if(fp == NULL) {
        TRACE("File doesnt exist");
        return 0;
    }
    fclose(fp);

    picasso_image *bmp = bmp_load_from_file(argv[1]);


//    BMP *image = picasso_create_bmp_from_rgba(bmp->width, bmp->height, bmp->pixels);
//
//    if (picasso_save_to_bmp(image, "first_try.bmp", PICASSO_PROFILE_SRGB) < 0) {
//        ERROR("Something went wrong");
//    }
    (void)*bmp;
    return 0;
}

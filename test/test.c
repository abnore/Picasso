#include <stdio.h>
#include "picasso.h"
#include "logger.h"

int main(void)
{
    BMP *bmp = picasso_load_bmp("icon.bmp");

    BMP *image = picasso_create_bmp_from_rgba(bmp->ih.width, bmp->ih.height, bmp->pixels);

    if (picasso_save_to_bmp(image, "first_try.bmp", PICASSO_PROFILE_SRGB) < 0) {
        ERROR("Something went wrong");
    }

    return 0;
}

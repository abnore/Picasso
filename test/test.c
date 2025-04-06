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
    char new_file[512];
    char *addition = "test_";
    snprintf(new_file, sizeof(new_file), "%s%s", addition, filename);

    picasso_image *test = picasso_load_bmp(filename);
    if(test == NULL){
        INFO("Exiting");
        return -1;
    }
//    PPM *ppm = picasso_load_ppm(filename);
//    BMP *triangle = picasso_create_bmp_from_rgba(ppm->width, ppm->height, 3, ppm->pixels);
//    picasso_save_to_bmp(triangle, "triangle.bmp", 0);
    bmp *test_b= picasso_create_bmp_from_rgba(test->width, test->height, test->channels, test->pixels);
    picasso_save_to_bmp(test_b, "bmp_suite_test.bmp" , PICASSO_PROFILE_NONE);

    (void)*test;
    return 0;
}

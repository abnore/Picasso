#include "picasso.h"
#include "logger.h"

#include <errno.h>

#define return_defer(value) do { result = (value); goto defer; } while(0)

const char* color_to_string(color c)
{
#define X(x) color_to_u32(x)

    uint32_t value = X(c);
    switch (value) {
        case X(BLUE):        return "BLUE";
        case X(GREEN):       return "GREEN";
        case X(RED):         return "RED";

        case X(WHITE):       return "WHITE";
        case X(BLACK):       return "BLACK";
        case X(GRAY):        return "GRAY";
        case X(LIGHT_GRAY):  return "LIGHT_GRAY";
        case X(DARK_GRAY):   return "DARK_GRAY";

        case X(ORANGE):      return "ORANGE";
        case X(YELLOW):      return "YELLOW";
        case X(BROWN):       return "BROWN";
        case X(GOLD):        return "GOLD";

        case X(CYAN):        return "CYAN";
        case X(MAGENTA):     return "MAGENTA";
        case X(PURPLE):      return "PURPLE";
        case X(NAVY):        return "NAVY";
        case X(TEAL):        return "TEAL";

        default: return "UNKNOWN";
    }
#undef X
}

BMP *picasso_load_bmp(const char *filename)
{
    BMP *image = canopy_malloc(sizeof(BMP));
    if (!image) {
        fprintf(stderr, "Failed to allocate BMP struct\n");
        return NULL;
    }
    memset(image, 0, sizeof(BMP));

    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Unable to open file");
        canopy_free(image);
        return NULL;
    }

    // Read File Header
    fread(&image->fh, sizeof(image->fh), 1, file);
    if (image->fh.file_type != 0x4D42) {
        fprintf(stderr, "Not a valid BMP file (magic: 0x%X)\n", image->fh.file_type);
        fclose(file);
        canopy_free(image);
        return NULL;
    }

    // Read Info Header
    fread(&image->ih, sizeof(image->ih), 1, file);

    // Move to pixel data
    fseek(file, image->fh.offset_data, SEEK_SET);

    // Allocate pixel data buffer
    image->pixels = canopy_malloc(image->ih.size_image);
    if (!image->pixels) {
        fprintf(stderr, "Failed to allocate BMP pixel buffer\n");
        fclose(file);
        canopy_free(image);
        return NULL;
    }

    // Read pixel data
    fread(image->pixels, image->ih.size_image, 1, file);
    fclose(file);

    // Convert from BGRA to RGBA (macOS expects RGBA)
    for (int i = 0; i < image->ih.width * image->ih.height; ++i) {
        uint8_t *p = &image->pixels[i * 4];
        uint8_t tmp = p[0];
        p[0] = p[2];  // Swap B and R
        p[2] = tmp;
    }

    return image;
}

void picasso_flip_buffer_vertical(uint8_t *buffer, int width, int height) {
    int bytes_per_pixel = 4; // RGBA
    int row_size = width * bytes_per_pixel;

    uint8_t temp_row[row_size]; // Temporary row buffer

    for (int y = 0; y < height / 2; y++) {
        int top_index = y * row_size;
        int bottom_index = (height - y - 1) * row_size;

        // Swap the rows
        memcpy(temp_row, &buffer[top_index], row_size);
        memcpy(&buffer[top_index], &buffer[bottom_index], row_size);
        memcpy(&buffer[bottom_index], temp_row, row_size);
    }
}

PPM *picasso_load_ppm(const char *file_path)
{
    return NULL;
}

int picasso_save_to_ppm(PPM *image, const char *file_path)
{
    FILE *file = fopen(file_path, "wb");
    if (!file) return errno;

    // Write PPM header
    if (fprintf(file, "P6\n%zu %zu\n255\n", image->width, image->height) < 0) {
        fclose(file);
        return errno;
    }

    // Write pixel data (RGB, skipping alpha)
    for (size_t i = 0; i < image->width * image->height; i++) {
        uint32_t pixel = image->pixels[i];
        uint8_t rgb[3] = {
            (pixel >> 0)  & 0xFF,  // Red
            (pixel >> 8)  & 0xFF,  // Green
            (pixel >> 16) & 0xFF   // Blue
        };

        if (fwrite(rgb, sizeof(rgb), 1, file) != 1) {
            fclose(file);
            return errno;
        }
    }

    fclose(file);
    return 0;
}



// SPRITES

picasso_sprite_sheet* picasso_create_sprite_sheet(
    uint32_t* pixels,
    int sheet_width,
    int sheet_height,
    int frame_width,
    int frame_height,
    int margin_x,
    int margin_y,
    int spacing_x,
    int spacing_y)
{
    if (!pixels || frame_width <= 0 || frame_height <= 0) return NULL;

    int cols = (sheet_width  - 2 * margin_x + spacing_x) / (frame_width + spacing_x);
    int rows = (sheet_height - 2 * margin_y + spacing_y) / (frame_height + spacing_y);
    int total = cols * rows;

    picasso_sprite_sheet* sheet = canopy_malloc(sizeof(picasso_sprite_sheet));
    if (!sheet) return NULL;

    sheet->pixels         = pixels;
    sheet->sheet_width    = sheet_width;
    sheet->sheet_height   = sheet_height;
    sheet->frame_width    = frame_width;
    sheet->frame_height   = frame_height;
    sheet->margin_x       = margin_x;
    sheet->margin_y       = margin_y;
    sheet->spacing_x      = spacing_x;
    sheet->spacing_y      = spacing_y;
    sheet->frames_per_row = cols;
    sheet->frames_per_col = rows;
    sheet->frame_count    = total;

    sheet->frames = canopy_malloc(sizeof(picasso_sprite) * total);
    if (!sheet->frames) {
        canopy_free(sheet);
        return NULL;
    }

    int i = 0;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int x = margin_x + col * (frame_width + spacing_x);
            int y = margin_y + row * (frame_height + spacing_y);
            sheet->frames[i++] = (picasso_sprite){ x, y, frame_width, frame_height };
        }
    }

    return sheet;
}

void picasso_destroy_sprite_sheet(picasso_sprite_sheet* sheet)
{
    if (!sheet) return;
    canopy_free(sheet->frames);
    canopy_free(sheet);
}


void picasso_fill_canvas(color *pixels, size_t width, size_t height, color c)
{
    DEBUG("color is %08x", color_to_u32(c));
    for(int i = 0; i < width*height; ++i){
        pixels[i] = c;
    }
}

void picasso_destroy_backbuffer(picasso_backbuffer* bf)
{
    if (!bf) return;
    if (bf->pixels) {
        free(bf->pixels);
        bf->pixels = NULL;
    }
    free(bf);
}

void picasso_fill_backbuffer(picasso_backbuffer* bf, color c)
{
    if (!bf || !bf->pixels) return;
    uint32_t color_val = color_to_u32(c);
    int count = bf->width * bf->height;
    for (int i = 0; i < count; ++i) {
        bf->pixels[i] = color_val;
    }
}
picasso_backbuffer* picasso_create_backbuffer(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return NULL;
    }

    picasso_backbuffer* bf = malloc(sizeof(picasso_backbuffer));
    if (!bf) return NULL;

    bf->width = width;
    bf->height = height;
    bf->pitch = width * sizeof(uint32_t); // 4 bytes per pixel
    bf->pixels = calloc(width * height, sizeof(uint32_t));

    if (!bf->pixels) {
        free(bf);
        return NULL;
    }

    return bf;
}

void picasso_blit_bitmap(picasso_backbuffer* dst,
                         void* src_pixels, int src_w, int src_h,
                         int x, int y)
{
    if (!dst || !src_pixels || !dst->pixels) return;

    int dst_w = dst->width;
    int dst_h = dst->height;

    for (int row = 0; row < src_h; ++row)
    {
        int dst_y = y + row;
        if (dst_y < 0 || dst_y >= dst_h) continue;

        for (int col = 0; col < src_w; ++col)
        {
            int dst_x = x + col;
            if (dst_x < 0 || dst_x >= dst_w) continue;

            uint32_t* src = (uint32_t*)src_pixels;
            uint32_t* dst_pixel = &dst->pixels[dst_y * dst->width + dst_x];
            uint32_t  src_pixel = src[row * src_w + col];

            *dst_pixel = src_pixel;
        }
    }
}

void* picasso_backbuffer_pixels(picasso_backbuffer* bf)
{
    if (!bf) return NULL;
    return (void*)bf->pixels;
}

void picasso_clear_backbuffer(picasso_backbuffer* bf)
{
    if (!bf || !bf->pixels) {
        WARN("Attempted to clear NULL backbuffer");
        return;
    }

    size_t total_pixels = bf->width * bf->height;
    for (size_t i = 0; i < total_pixels; ++i) {
        bf->pixels[i] = 0x00000000;
    }
}


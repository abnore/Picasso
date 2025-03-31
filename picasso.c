#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "picasso.h"
#include "logger.h"

#include "picasso_icc_profiles.h"

static const char *picasso_icc_profile_name(picasso_icc_profile profile) {
    switch (profile) {
        case PICASSO_PROFILE_NONE: return "None";
        case PICASSO_PROFILE_ACESCG_LINEAR: return "ACESCG Linear";
        case PICASSO_PROFILE_ADOBERGB1998: return "AdobeRGB1998";
        case PICASSO_PROFILE_DCI_P3_RGB: return "DCI(P3) RGB";
        case PICASSO_PROFILE_DISPLAY_P3: return "Display P3";
        case PICASSO_PROFILE_GENERIC_CMYK_PROFILE: return "Generic CMYK Profile";
        case PICASSO_PROFILE_GENERIC_GRAY_GAMMA_2_2_PROFILE: return "Generic Gray Gamma 2.2 Profile";
        case PICASSO_PROFILE_GENERIC_GRAY_PROFILE: return "Generic Gray Profile";
        case PICASSO_PROFILE_GENERIC_LAB_PROFILE: return "Generic Lab Profile";
        case PICASSO_PROFILE_GENERIC_RGB_PROFILE: return "Generic RGB Profile";
        case PICASSO_PROFILE_GENERIC_XYZ_PROFILE: return "Generic XYZ Profile";
        case PICASSO_PROFILE_ITU_2020: return "ITU-2020";
        case PICASSO_PROFILE_ITU_709: return "ITU-709";
        case PICASSO_PROFILE_ROMM_RGB: return "ROMM RGB";
        case PICASSO_PROFILE_SRGB: return "sRGB Profile";
        default: return "Unknown";
    }
}
static bool picasso_embed_icc_profile(BMP *image, picasso_icc_profile profile, FILE *out)
{
    const uint8_t *icc_data = NULL;
    size_t icc_size = 0;

    switch (profile) {
#include "picasso_icc_switch.h"
        case PICASSO_PROFILE_NONE:
        default:
            return false;  // No ICC data written
    }

    if (!icc_data || icc_size == 0) {
        return false;
    }

    // Set header fields for ICC profile
    image->ih.profile_size = (uint32_t)icc_size;
    image->ih.profile_data = image->fh.file_size;

    // Write ICC profile to file
    size_t written = fwrite(icc_data, 1, icc_size, out);
    if (written != icc_size) {
        ERROR("Failed to write ICC profile data (%zu of %zu bytes)", written, icc_size);
        return false;
    }

    TRACE("Wrote ICC profile: %s (%zu bytes)", picasso_icc_profile_name(profile), icc_size);
    image->fh.file_size += (uint32_t)icc_size;
    return true;
}



#ifndef CUSTOM_ALLOC
void* picasso_calloc(size_t count, size_t size){
    return calloc(count, size);
}

void picasso_free(void *ptr){
    free(ptr);
}

void *picasso_malloc(size_t size){
    return malloc(size);
}

void * picasso_realloc(void *ptr, size_t size){
    return realloc(ptr, size);
}
#endif


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

static void picasso_flip_buffer_vertical(uint8_t *buffer, int width, int height)
{
    int bytes_per_pixel = 4; // RGBA
    int row_size = width * bytes_per_pixel;

    TRACE("Flipping buffer vertically (%dx%d)", width, height);

    uint8_t temp_row[row_size]; // Temporary row buffer

    for (int y = 0; y < height / 2; y++) {
        int top_index = y * row_size;
        int bottom_index = (height - y - 1) * row_size;

        // Swap the rows
        memcpy(temp_row, &buffer[top_index], row_size);
        memcpy(&buffer[top_index], &buffer[bottom_index], row_size);
        memcpy(&buffer[bottom_index], temp_row, row_size);
    }

    TRACE("Finished vertical flip");
}

BMP *picasso_load_bmp(const char *filename)
{
    BMP *image = picasso_malloc(sizeof(BMP));
    if (!image) {
        ERROR("Failed to allocate BMP struct");
        return NULL;
    }
    memset(image, 0, sizeof(BMP));
    TRACE("Allocated and zero-initialized BMP struct");

    FILE *file = fopen(filename, "rb");
    if (!file) {
        ERROR("Unable to open BMP file: %s", filename);
        picasso_free(image);
        return NULL;
    }
    TRACE("Opened BMP file: %s", filename);

    // Read File Header
    size_t read_fh = fread(&image->fh, sizeof(image->fh), 1, file);
    if (read_fh != 1) {
        ERROR("Failed to read BMP file header");
        fclose(file);
        picasso_free(image);
        return NULL;
    }
    DEBUG("BMP file type: 0x%X", image->fh.file_type);

    if (image->fh.file_type != 0x4D42) {
        ERROR("Invalid BMP magic number: expected 0x4D42, got 0x%X", image->fh.file_type);
        fclose(file);
        picasso_free(image);
        return NULL;
    }
    TRACE("BMP magic number is valid (0x4D42)");

    // Read Info Header
    size_t read_ih = fread(&image->ih, sizeof(image->ih), 1, file);
    if (read_ih != 1) {
        ERROR("Failed to read BMP info header");
        fclose(file);
        picasso_free(image);
        return NULL;
    }
    DEBUG("Width: %d, Height: %d, Image size: %u", image->ih.width, image->ih.height, image->ih.size_image);

    // Move to pixel data
    if (fseek(file, image->fh.offset_data, SEEK_SET) != 0) {
        ERROR("Failed to seek to pixel data offset: %u", image->fh.offset_data);
        fclose(file);
        picasso_free(image);
        return NULL;
    }
    TRACE("Seeked to pixel data at offset: %u", image->fh.offset_data);

    // Allocate pixel data buffer
    image->pixels = picasso_malloc(image->ih.size_image);
    if (!image->pixels) {
        ERROR("Failed to allocate pixel buffer (%u bytes)", image->ih.size_image);
        fclose(file);
        picasso_free(image);
        return NULL;
    }
    DEBUG("Allocated pixel buffer (%u bytes)", image->ih.size_image);

    // Read pixel data
    size_t read_px = fread(image->pixels, 1, image->ih.size_image, file);
    fclose(file);
    if (read_px != image->ih.size_image) {
        ERROR("Failed to read full pixel data (read %zu of %u bytes)", read_px, image->ih.size_image);
        picasso_free(image->pixels);
        picasso_free(image);
        return NULL;
    }
    TRACE("Successfully read BMP pixel data");

    // Convert BGRA -> RGBA
    for (int i = 0; i < image->ih.width * image->ih.height; ++i) {
        uint8_t *p = &image->pixels[i * 4];
        uint8_t tmp = p[0];
        p[0] = p[2];  // Swap B and R
        p[2] = tmp;
    }
    TRACE("Converted BGRA to RGBA");

    INFO("Loaded BMP image: %dx%d", image->ih.width, image->ih.height);

    if (image->ih.height > 0) {
        TRACE("Flipping BMP buffer to top-down layout");
        picasso_flip_buffer_vertical(image->pixels, image->ih.width, image->ih.height);
        image->ih.height = -image->ih.height;
        TRACE("Marked BMP as top-down (ih.height = %d)", image->ih.height);
    }
     DEBUG("First pixel RGBA = %02X %02X %02X %02X",
      image->pixels[0], image->pixels[1], image->pixels[2], image->pixels[3]);
    return image;
}
int picasso_save_to_bmp(BMP *image, const char *file_path, picasso_icc_profile profile)
{
    FILE *f = fopen(file_path, "wb");
    if (!f) {
        ERROR("Failed to open BMP file for writing: %s", file_path);
        return -1;
    }
    TRACE("Opened BMP file for writing: %s", file_path);

    int width = image->ih.width;
    int height = image->ih.height;
    bool top_down = height < 0;
    if (top_down) height = -height;

    size_t pixel_array_size = (size_t)width * height * 4;

    // Flip buffer for bottom-up BMP format
    bool flipped = false;
    if (top_down) {
        picasso_flip_buffer_vertical(image->pixels, width, height);
        image->ih.height = height; // temporary for writing
        flipped = true;
    }

    // Set profile location if needed
    if (profile != PICASSO_PROFILE_NONE) {
        image->ih.profile_data = image->fh.offset_data + pixel_array_size;
    } else {
        image->ih.profile_data = 0;
        image->ih.profile_size = 0;
    }

    // Write headers
    if (fwrite(&image->fh, sizeof(image->fh), 1, f) != 1 ||
        fwrite(&image->ih, sizeof(image->ih), 1, f) != 1) {
        ERROR("Failed to write BMP headers");
        fclose(f);
        return -1;
    }
    TRACE("Wrote BMP headers");

    // Write pixel data as BGRA
    for (size_t i = 0; i < pixel_array_size; i += 4) {
        uint8_t *rgba = &image->pixels[i];
        uint8_t bgra[4] = { rgba[2], rgba[1], rgba[0], rgba[3] };
        if (fwrite(bgra, 1, 4, f) != 4) {
            ERROR("Failed to write pixel data at index %zu", i / 4);
            fclose(f);
            return -1;
        }
    }

    // Embed ICC profile
    if (profile != PICASSO_PROFILE_NONE) {
        if (!picasso_embed_icc_profile(image, profile, f)) {
            WARN("Failed to embed ICC profile");
        } else {
            INFO("Embedded ICC profile: %s", picasso_icc_profile_name(profile));
        }
    }

    fclose(f);
    TRACE("Finished writing BMP");

    // Restore buffer to top-down
    if (flipped) {
        picasso_flip_buffer_vertical(image->pixels, width, height);
        image->ih.height = -height;
    }

    INFO("Saved BMP with ICC to %s", file_path);
    return 0;
}

BMP *picasso_create_bmp_from_rgba(int width, int height, const uint8_t *pixel_data)
{
    if (width <= 0 || !pixel_data){
        ERROR("Invalid parameters for creating BMP: %dx%d, pixel_data=%p", width, height, (void *)pixel_data);
        return NULL;
    }
    height = abs(height); // force positive internally
    BMP *bmp = picasso_malloc(sizeof(BMP));
    if (!bmp) {
        ERROR("Failed to allocate BMP struct");
        return NULL;
    }
    memset(bmp, 0, sizeof(BMP));

    int bytes_per_pixel = 4;
    int row_size = width * bytes_per_pixel;
    size_t pixel_array_size = (size_t)row_size * height;

    // --- Fill file header ---
    bmp->fh.file_type = 0x4D42; // 'BM'
    bmp->fh.reserved1 = 0;
    bmp->fh.reserved2 = 0;
    bmp->fh.offset_data = sizeof(bmp->fh) + sizeof(bmp->ih); // 14 + 124 = 138
    bmp->fh.file_size = bmp->fh.offset_data + pixel_array_size;
DEBUG("Header size = %zu, offset_data = %u, file_size = %u",
       sizeof(bmp->ih), bmp->fh.offset_data, bmp->fh.file_size);
    // --- Fill info header (BITMAPV4HEADER) ---
    bmp->ih.size = sizeof(bmp->ih);  // Must be 108 for V4
    bmp->ih.width = width;
    bmp->ih.height = -height;  // top-down
    bmp->ih.planes = 1;
    bmp->ih.bit_count = 32;
    bmp->ih.compression = 3; // BI_BITFIELDS
    bmp->ih.size_image = pixel_array_size;
    bmp->ih.x_pixels_per_meter = 3780; // ~96 DPI
    bmp->ih.y_pixels_per_meter = 3780;
    bmp->ih.colors_used = 0;
    bmp->ih.colors_important = 0;

    // V4 alpha and colorspace support
    bmp->ih.red_mask   = 0x00FF0000;
    bmp->ih.green_mask = 0x0000FF00;
    bmp->ih.blue_mask  = 0x000000FF;
    bmp->ih.alpha_mask = 0xFF000000;
    bmp->ih.cs_type = 0x73524742; // 'sRGB' in little-endian

    memset(bmp->ih.endpoints, 0, sizeof(bmp->ih.endpoints));
    bmp->ih.gamma_red = 0;
    bmp->ih.gamma_green = 0;
    bmp->ih.gamma_blue = 0;

    // BITMAPV5HEADER-specific
    bmp->ih.intent        = 0x00000004; // LCS_GM_GRAPHICS
    bmp->ih.profile_data  = 0;
    bmp->ih.profile_size  = 0;
    bmp->ih.reserved      = 0;

    // --- Copy pixel buffer ---
    bmp->pixels = picasso_malloc(pixel_array_size);
    if (!bmp->pixels) {
        ERROR("Failed to allocate BMP pixel buffer (%zu bytes)", pixel_array_size);
        picasso_free(bmp);
        return NULL;
    }

    memcpy(bmp->pixels, pixel_data, pixel_array_size);
    TRACE("Created BMP from RGBA (%dx%d) using BITMAPV4HEADER", width, height);

    return bmp;
}
/* PPM header is literal ascii - must be parsed
 * like a text file, not with headers.
 *  5036 0a33 3030 2032 3030 0a32 3535 0a
 *  P 6  \n3  0 0    2  0  0 \n2   5 5 \n
 *
typedef struct {
    size_t width;
    size_t height;
    size_t maxval;
    uint8_t *pixels;
}PPM;
*/
// Convert ASCII character to number (e.g. '6' -> 6)
#define aton(n) ((int)((n) - 0x30))
// Convert number to ASCII character (e.g. 6 -> '6')
#define ntoa(n) ((char)((n) + 0x30))

static void skip_comments(FILE *f) {
    int c;
    while ((c = fgetc(f)) == '#') {
        while ((c = fgetc(f)) != '\n' && c != EOF);
    }
    ungetc(c, f); // put the non-comment character back
}

PPM *picasso_load_ppm(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        ERROR("Failed to open file: %s", filename);
        return NULL;
    }
    TRACE("Opened file: %s", filename);

    char magic[3];
    if (fscanf(f, "%2s", magic) != 1 || strcmp(magic, "P6") != 0) {
        ERROR("Invalid PPM magic number: expected 'P6', got '%s'", magic);
        fclose(f);
        return NULL;
    }
    TRACE("Magic number OK: %s", magic);

    skip_comments(f);
    int width, height, maxval;

    if (fscanf(f, "%d", &width) != 1) {
        ERROR("Failed to read width");
        goto fail;
    }
    DEBUG("Width: %d", width);

    skip_comments(f);
    if (fscanf(f, "%d", &height) != 1) {
        ERROR("Failed to read height");
        goto fail;
    }
    DEBUG("Height: %d", height);

    skip_comments(f);
    if (fscanf(f, "%d", &maxval) != 1) {
        ERROR("Failed to read maxval");
        goto fail;
    }
    DEBUG("Maxval: %d", maxval);

    if (maxval != 255) {
        ERROR("Unsupported maxval: %d (expected 255)", maxval);
        goto fail;
    }

    // Skip single whitespace after maxval before pixel data
    fgetc(f);
    TRACE("Skipped whitespace after maxval");

    size_t pixels_size = width * height * 3;
    unsigned char *pixels = picasso_malloc(pixels_size);
    if (!pixels) {
        ERROR("Out of memory allocating pixel buffer (%zu bytes)", pixels_size);
        goto fail;
    }
    DEBUG("Allocated pixel buffer (%zu bytes)", pixels_size);

    size_t read = fread(pixels, 1, pixels_size, f);
    if (read != pixels_size) {
        ERROR("Unexpected EOF: expected %zu bytes, got %zu", pixels_size, read);
        picasso_free(pixels);
        goto fail;
    }

    TRACE("Read pixel data");
    fclose(f);

    PPM *image = picasso_malloc(sizeof(PPM));
    if (!image) {
        ERROR("Out of memory allocating PPM struct");
        picasso_free(pixels);
        return NULL;
    }

    image->width = width;
    image->height = height;
    image->maxval = maxval;
    image->pixels = pixels;

    INFO("Loaded PPM image: %dx%d", width, height);
    return image;

fail:
    ERROR("Failed to parse PPM file: %s", filename);
    fclose(f);
    return NULL;
}

int picasso_save_to_ppm(PPM *image, const char *file_path)
{
    FILE *f = fopen(file_path, "wb");
    if (f == NULL) {
        ERROR("Failed to open file for writing: %s", file_path);
        return -1;
    }
    TRACE("Opened file for writing: %s", file_path);

    fprintf(f, "P6\n%zu %zu\n255\n", image->width, image->height);
    DEBUG("Wrote PPM header: P6 %zux%zu", image->width, image->height);

    size_t total_pixels = image->width * image->height;
    TRACE("Saving %zu pixels", total_pixels);

    for (size_t i = 0; i < total_pixels; i++) {
        // Format: 0xAABBGGRR - skipping alpha
        uint32_t pixel = image->pixels[i];
        uint8_t bytes[3] = {
            (pixel >>  0) & 0xFF, // Red
            (pixel >>  8) & 0xFF, // Green
            (pixel >> 16) & 0xFF  // Blue
        };
        size_t written = fwrite(bytes, sizeof(bytes), 1, f);
        if (written != 1) {
            ERROR("Failed to write pixel %zu", i);
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    INFO("Saved PPM image to %s (%zux%zu)", file_path, image->width, image->height);
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

    picasso_sprite_sheet* sheet = picasso_malloc(sizeof(picasso_sprite_sheet));
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

    sheet->frames = picasso_malloc(sizeof(picasso_sprite) * total);
    if (!sheet->frames) {
        picasso_free(sheet);
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
    picasso_free(sheet->frames);
    picasso_free(sheet);
}



// --------------------------------------------------------
// Graphical functions
// --------------------------------------------------------

// --------------------------------------------------------
// Backbuffer operations
// --------------------------------------------------------

static inline uint32_t blend_pixel(uint32_t dst, uint32_t src)
{
    uint8_t sa = (src >> 24) & 0xFF;
    if (sa == 255) return src;
    if (sa == 0) return dst;

    uint8_t sr = src & 0xFF;
    uint8_t sg = (src >> 8) & 0xFF;
    uint8_t sb = (src >> 16) & 0xFF;

    uint8_t dr = dst & 0xFF;
    uint8_t dg = (dst >> 8) & 0xFF;
    uint8_t db = (dst >> 16) & 0xFF;

    uint8_t r = (sr * sa + dr * (255 - sa)) / 255;
    uint8_t g = (sg * sa + dg * (255 - sa)) / 255;
    uint8_t b = (sb * sa + db * (255 - sa)) / 255;

    return (0xFF << 24) | (b << 16) | (g << 8) | r;
}
picasso_backbuffer* picasso_create_backbuffer(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return NULL;
    }

    picasso_backbuffer* bf = picasso_malloc(sizeof(picasso_backbuffer));
    if (!bf) return NULL;

    bf->width = width;
    bf->height = height;
    bf->pitch = width * sizeof(uint32_t); // 4 bytes per pixel
    bf->pixels = picasso_calloc(width * height, sizeof(uint32_t));

    if (!bf->pixels) {
        picasso_free(bf);
        return NULL;
    }

    return bf;
}

void picasso_destroy_backbuffer(picasso_backbuffer* bf)
{
    if (!bf) return;
    if (bf->pixels) {
        picasso_free(bf->pixels);
        bf->pixels = NULL;
    }
    picasso_free(bf);
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

            *dst_pixel = blend_pixel(*dst_pixel, src_pixel);
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

    for (size_t i = 0; i <  bf->width * bf->height; ++i) {
        bf->pixels[i] = color_to_u32(CLEAR_BACKGROUND);
    }
}

// --------------------------------------------------------
// Graphical primitives
// --------------------------------------------------------

void picasso_fill_rect(picasso_backbuffer *bf, picasso_rect *r, color c)
{
    if(!bf || !r)
    {
        ERROR("No buffer to draw on, or no initial settings given");
        return;
    }
    //Normalize the rect to support negative drawing

    if(r->width < 0) {
        r->width = PICASSO_ABS(r->width);
        r->x -= r->width;
    }

    if(r->height < 0) {
        r->height = PICASSO_ABS(r->height);
        r->y -= r->height;
    }
    uint32_t new_pixel = color_to_u32(c);

    // Bounds checking
    int start_x = r->x;
    int start_y = r->y;
    int end_x   = r->x + r->width;
    int end_y   = r->y + r->height;

    // Clamping to framebuffer dimensions
    if( start_x < 0 )           start_x = 0;
    if( start_y < 0 )           start_y = 0;
    if (end_x > (int)bf->width)   end_x = (int)bf->width;
    if (end_y > (int)bf->height)  end_y = (int)bf->height;

    for(int y = start_y; y < end_y; ++y){
        for(int x = start_x; x < end_x; ++x){
            uint32_t *cur_pixel = &(bf->pixels[ y * bf->width + x ]);
            *cur_pixel = blend_pixel(*cur_pixel, new_pixel);
        }
    }
}

void picasso_draw_line(picasso_backbuffer *bf, int x0, int y0, int x1, int y1, color c)
{
    uint32_t new_pixel = color_to_u32(c);

    /* Bresenhams lines algorithm
     * */
    int dx = x1-x0;
    int dy = y1-y0;
    int D = 2*dy-dx;
    int y = y0;
    for(int i = x0; i < x1; ++i){
        bf->pixels[y*bf->width + i] = new_pixel;
        if (D > 0) {
            y++;
            D -= 2*dx;
        }
        D += 2*dy;
    }
}

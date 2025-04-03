#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "picasso.h"
#include "logger.h"



void picasso_image_free(picasso_image *img);

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

/* -------------------- Little Endian Byte Readers Utility -------------------- */
uint8_t picasso_read_u8(const uint8_t *p) {
    return p[0];
}

uint16_t picasso_read_u16_le(const uint8_t *p) {
    uint16_t lo = picasso_read_u8(p);
    uint16_t hi = picasso_read_u8(p + 1);
    return lo | (hi << 8);
}

uint32_t picasso_read_u32_le(const uint8_t *p) {
    uint16_t lo = picasso_read_u16_le(p);
    uint16_t hi = picasso_read_u16_le(p + 2);
    return (uint32_t)lo | ((uint32_t)hi << 16);
}
int32_t picasso_read_s32_le(const uint8_t *p) {
    return (int32_t)picasso_read_u32_le(p);// safe because casting unsigned to signed preserves bit pattern
}
/* -------------------- File Support -------------------- */

void *picasso_read_entire_file(const char *path, size_t *out_size)
{
    long size;
    void *buffer = NULL;
    size_t read;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0  ||
        (size = ftell(f)) < 0       ||
        fseek(f, 0, SEEK_SET) != 0  ||
        !(buffer = picasso_malloc((size_t)size))) {
        fclose(f);
        return NULL;
    }

    read = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        picasso_free(buffer);
        return NULL;
    }

    if (out_size) *out_size = (size_t)size;
    return buffer;
}

int picasso_write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    return written == size;
}

/* -------------------- Color Section -------------------- */
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

picasso_image *picasso_alloc_image(int width, int height, int channels)
{
    if (width <= 0 || height <= 0 || (channels != 3 && channels != 4)) return NULL;

    picasso_image *img = malloc(sizeof(picasso_image));
    if (!img) return NULL;

    img->width = width;
    img->height = height;
    img->channels = channels;
    img->pixels = malloc(width * height * channels);
    if (!img->pixels) {
        free(img);
        return NULL;
    }

    return img;
}

void picasso_free_image(picasso_image *img)
{
    if (img) {
        free(img->pixels);
        free(img);
    }
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
// Graphical functions and utilities
// --------------------------------------------------------
/* Here we are supporting negative width and height, drawing
 * in all directions! */
static void picasso__normalize_rect(picasso_rect *r)
{
    if(!r) {WARN("Tried to normalize a NULL object");return;}
    if (r->width < 0) {
        r->x += r->width;
        r->width = -r->width;
    }
    if (r->height < 0) {
        r->y += r->height;
        r->height = -r->height;
    }
}
/* Creating the bounds for looping over, removing the logic from the draw functions */
static bool picasso__clip_rect_to_bounds(picasso_backbuffer *bf, const picasso_rect *r,
                                 picasso_draw_bounds *db)
{
    if (!r || r->width == 0 || r->height == 0)
        return false;

    if (r->x >= (int)bf->width || r->y >= (int)bf->height ||
        r->x + r->width <= 0 || r->y + r->height <= 0)
        return false;

    db->x0 = (r->x > 0) ? r->x : 0;
    db->y0 = (r->y > 0) ? r->y : 0;
    db->x1 = (r->x + r->width < (int)bf->width) ? r->x + r->width : (int)bf->width;
    db->y1 = (r->y + r->height < (int)bf->height) ? r->y + r->height : (int)bf->height;

    return true;
}

static inline uint32_t picasso__blend_pixel(uint32_t dst, uint32_t src)
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
// --------------------------------------------------------
// Backbuffer operations
// --------------------------------------------------------


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

            *dst_pixel = picasso__blend_pixel(*dst_pixel, src_pixel);
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
    picasso_draw_bounds bounds = {0};
    picasso__normalize_rect(r);
    if(!picasso__clip_rect_to_bounds(bf, r, &bounds)) return;

    uint32_t new_pixel = color_to_u32(c);

    for (int y = bounds.y0; y < bounds.y1; ++y) {
        for (int x = bounds.x0; x < bounds.x1; ++x) {
            uint32_t *cur_pixel = &bf->pixels[y * bf->width + x];
            *cur_pixel = picasso__blend_pixel(*cur_pixel, new_pixel);
        }
    }
}

/* This approach might be slightly wasteful, but it works! */
void picasso_draw_rect(picasso_backbuffer *bf, picasso_rect *outer, int thickness, color c)
{
    if (!outer || !bf || thickness <= 0) return;

    picasso_draw_bounds outer_bounds = {0};
    picasso_draw_bounds inner_bounds = {0};

    // Normalize original rectangle (respecting negative width/height)
    picasso__normalize_rect(outer);

    // Compute inner rectangle
    picasso_rect inner = {
        .x = outer->x + thickness,
        .y = outer->y + thickness,
        .width = outer->width - 2 * thickness,
        .height = outer->height - 2 * thickness
    };

    // Clip to draw bounds
    if (!picasso__clip_rect_to_bounds(bf, outer, &outer_bounds)) return;

    if (!picasso__clip_rect_to_bounds(bf, &inner, &inner_bounds)) {
        inner_bounds = (picasso_draw_bounds){0};
    }

    uint32_t new_pixel = color_to_u32(c);

    for (int y = outer_bounds.y0; y < outer_bounds.y1; ++y) {
        for (int x = outer_bounds.x0; x < outer_bounds.x1; ++x) {

            bool inside_inner = (
                    y >= inner_bounds.y0 && y < inner_bounds.y1 &&
                    x >= inner_bounds.x0 && x < inner_bounds.x1
                    );

            if (inside_inner) continue;
            else {
                uint32_t *cur_pixel = &bf->pixels[y * bf->width + x];
                *cur_pixel = picasso__blend_pixel(*cur_pixel, new_pixel);
            }
        }
    }
}

void picasso_fill_circle(picasso_backbuffer *bf, int x0, int y0, int radius, color c)
{
    picasso_draw_bounds bounds = {0};
    picasso_rect circle_box = {
        .x = x0-radius, .y = y0-radius,
        .height = radius*2,
        .width = radius*2
    };
    if(!picasso__clip_rect_to_bounds(bf, &circle_box, &bounds)) return;
    uint32_t new_pixel = color_to_u32(c);
    // a^2 + b^2 = c^2
    for (int y = bounds.y0; y < bounds.y1 + 2; ++y) {
        for (int x = bounds.x0; x < bounds.x1 + 2; ++x) {
            int dx = x - x0;
            int dy = y - y0;
            if((dx*dx + dy*dy <= radius*radius + radius)){
                uint32_t *cur_pixel = &bf->pixels[y * bf->width + x];
                *cur_pixel = picasso__blend_pixel(*cur_pixel, new_pixel);
            }
        }
    }
}
void picasso_draw_circle(picasso_backbuffer *bf, int x0, int y0, int radius,int thickness, color c)
{
    picasso_draw_bounds bounds = {0};
    picasso_rect circle_box = {
        .x = x0 - radius, .y = y0 - radius,
        .width = radius * 2,
        .height = radius * 2
    };

    if (!picasso__clip_rect_to_bounds(bf, &circle_box, &bounds)) return;

    uint32_t new_pixel = color_to_u32(c);

    int outer = radius * radius;
    int inner = (radius - thickness) * (radius - thickness);

    for (int y = bounds.y0; y < bounds.y1 + 2; ++y) {
        for (int x = bounds.x0; x < bounds.x1 + 2; ++x) {
            int dx = x - x0;
            int dy = y - y0;
            int dist2 = dx * dx + dy * dy;

            if (dist2 >= inner+radius && dist2 <= outer+radius) {
                uint32_t *cur_pixel = &bf->pixels[y * bf->width + x];
                *cur_pixel = picasso__blend_pixel(*cur_pixel, new_pixel);
            }
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "picasso.h"
#include "logger.h"


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
    if (width <= 0 || height <= 0 || channels < 0 || channels > 4) return NULL;

    picasso_image *img = picasso_malloc(sizeof(picasso_image));
    if (!img) return NULL;

    img->width = width;
    img->height = height;
    img->channels = channels;
    img->row_stride = channels * width;
    img->pixels = picasso_calloc(sizeof(uint8_t), img->width * img->row_stride);
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

    // Simple logic to force the rect inside the backbuffer
    db->x0 = (r->x > 0) ? r->x : 0;
    db->y0 = (r->y > 0) ? r->y : 0;
    db->x1 = (r->x + r->width < (int)bf->width) ? r->x + r->width : (int)bf->width;
    db->y1 = (r->y + r->height < (int)bf->height) ? r->y + r->height : (int)bf->height;

    return true;
}

static inline uint32_t picasso__blend_pixel(uint32_t dst, uint32_t src)
{
    color back  = u32_to_color(dst);
    color front = u32_to_color(src);

    uint8_t sa = front.a;
    if (sa == 255) return src;
    if (sa == 0)   return dst;

    uint8_t r = (front.r * sa + back.r * (255 - sa)) / 255;
    uint8_t g = (front.g * sa + back.g * (255 - sa)) / 255;
    uint8_t b = (front.b * sa + back.b * (255 - sa)) / 255;
    uint8_t a = (front.a * 255 + back.a * (255 - sa)) / 255;

    color blended = { r, g, b, a };
    return color_to_u32(blended);
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
    bf->pitch = width; // pixels are 32bit - pitch = amount of pixels wide
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


picasso_image *picasso_image_from_backbuffer(picasso_backbuffer *bf)
{
    if (!bf || !bf->pixels) return NULL;

    picasso_image *img = picasso_alloc_image(bf->width, bf->height, 4); // 4 channels
    if (!img) return NULL;

    for (int y = 0; y < (int)bf->height; ++y) {
        for (int x = 0; x < (int)bf->width; ++x) {
            uint32_t *pixel = picasso__get_pixel_u32(bf, x, y);
            picasso__put_pixel_u8(img, x, y, pixel);
        }
    }

    return img;
}

//void picasso_blit_bitmap(picasso_backbuffer *dst, picasso_image *src, int offset_x, int offset_y)
//{
//    if (!dst || !src || !src->pixels || !dst->pixels) {
//        //Gracefulle exits - this will most likely be inside a loop, no error reporting
//        return;
//    }
//
//    foreach_pixel_u8(src, {
//        int dst_x = x + offset_x;
//        int dst_y = y + offset_y;
//        if (dst_x < 0 || dst_x >= (int)dst->width ||
//            dst_y < 0 || dst_y >= (int)dst->height)
//            continue;
//
//        color c = get_color_u8(pixel, src->channels);
//        uint32_t rgba = color_to_u32(c);
//
//        uint32_t *dst_pixel = picasso__get_pixel_u32(dst, dst_x, dst_y);
//        *dst_pixel = picasso__blend_pixel(*dst_pixel, rgba);
//    });
//}

// “Wait, I can just do this in one clean line now?” ..... Just wrap blit_rect and rename it blit..
void picasso_blit_bitmap(picasso_backbuffer *dst, picasso_image *src, int offset_x, int offset_y)
{
    picasso_rect full = { 0, 0, src->width, src->height };
    picasso_rect at   = { offset_x, offset_y, src->width, src->height };
    picasso_blit(dst, src, full, at);
}

// This became a powerhouse function, handles all blit actions now to the backbuffer
void picasso_blit(picasso_backbuffer *dst, picasso_image *src, picasso_rect src_r, picasso_rect dst_r)
{
    // Early sanity checks to bail out early if anything is invalid
    if (!dst || !src || !dst->pixels || !src->pixels) return;
    if (src_r.width <= 0 || src_r.height <= 0) return;

    picasso__normalize_rect(&src_r);
    picasso__normalize_rect(&dst_r);

    // Clamp src_r to source image bounds (safe blit)
    if (src_r.x < 0) src_r.x = 0;
    if (src_r.y < 0) src_r.y = 0;
    // if you draw at an x value, this compensates from the width so the total
    // width never exceed actual image width - same for y below
    if (src_r.x + src_r.width > src->width) {
        src_r.width = src->width - src_r.x;
    }
    if (src_r.y + src_r.height > src->height) {
        src_r.height = src->height - src_r.y;
    }

    // Bounds of the destination
    picasso_draw_bounds bounds;
    if (!picasso__clip_rect_to_bounds(dst, &dst_r, &bounds)) return;

    // Extracting the scaling factor, how fast we sample from the src to the dst. Maybe 2 pixels per pixel
    float scale_x = (float)src_r.width  / dst_r.width;
    float scale_y = (float)src_r.height / dst_r.height;


    // For every row of destination (dy) and dest col (dx)
    for (int dst_y = bounds.y0; dst_y < bounds.y1; ++dst_y)
    {
        int relative_dst_y = dst_y - dst_r.y;
        int src_y = src_r.y + (int)(relative_dst_y * scale_y);

        if( src_y < 0 || src_y >= src->height ) continue;

        for (int dst_x = bounds.x0; dst_x < bounds.x1; ++dst_x)
        {
            int relative_dst_x = dst_x - dst_r.x;
            int src_x = src_r.x + (int)(relative_dst_x * scale_x);

            if( src_x < 0 || src_x >= src->width ) continue;

            uint8_t  *src_pixel = picasso__get_pixel_u8(src, src_x, src_y);
            uint32_t *dst_pixel = picasso__get_pixel_u32(dst, dst_x, dst_y);

            color c = get_color_u8(src_pixel, src->channels);
            uint32_t rgba = color_to_u32(c);

            *dst_pixel = picasso__blend_pixel(*dst_pixel, rgba);
        }
    }
}
// get_color takes into account channels of src, color is always 4 channel valid
void picasso_copy(picasso_image *src, picasso_image *dst)
{
    for (int y = 0; y < dst->height; ++y) {
        for (int x = 0; x < dst->width; ++x) {
            size_t nx = x * src->width / dst->width;
            size_t ny = y * src->height / dst->height;

            uint8_t *dst_pixel = picasso__get_pixel_u8(dst, x, y);
            uint8_t *src_pixel = picasso__get_pixel_u8(src, nx, ny);

            color c = get_color_u8(src_pixel, src->channels);

            switch (dst->channels) {
                case 1:
                    dst_pixel[0] = (76 * c.r + 150 * c.g + 29 * c.b) >> 8;
                    break;
                case 2:
                    dst_pixel[0] = (76 * c.r + 150 * c.g + 29 * c.b) >> 8;
                    dst_pixel[1] = c.a;
                    break;
                case 3:
                    dst_pixel[0] = c.r;
                    dst_pixel[1] = c.g;
                    dst_pixel[2] = c.b;
                    break;
                case 4:
                    dst_pixel[0] = c.r;
                    dst_pixel[1] = c.g;
                    dst_pixel[2] = c.b;
                    dst_pixel[3] = c.a;
                    break;
            }
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
            uint32_t *cur_pixel = picasso__get_pixel_u32(bf, x, y);
            *cur_pixel = picasso__blend_pixel(*cur_pixel, new_pixel);
        }
    }
}


/*
 * FIX: This approach might be slightly wasteful, but it works!
 * I calculate the whole rectangle and set outer bounds, and then i calculate an inner
 * rectangle which is the thickness less in - i then set that to be skipped.
 * So in practive i am going over every pixel of the larger rect, and skipping the inside
 *
 * I might be able to do a 4-slice instead
 * */
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

    // if inner bounds doesnt make sense we just null it out making it a full rect
    if (!picasso__clip_rect_to_bounds(bf, &inner, &inner_bounds)) {
        inner_bounds = (picasso_draw_bounds){0};
    }

    uint32_t new_pixel = color_to_u32(c);

    for (int y = outer_bounds.y0; y < outer_bounds.y1; ++y) {
        for (int x = outer_bounds.x0; x < outer_bounds.x1; ++x) {

            bool inside_inner = ( y >= inner_bounds.y0 && y < inner_bounds.y1 &&
                                  x >= inner_bounds.x0 && x < inner_bounds.x1 );

            if (inside_inner) continue;
            else {
                uint32_t *cur_pixel = picasso__get_pixel_u32(bf, x, y);
                *cur_pixel = picasso__blend_pixel(*cur_pixel, new_pixel);
            }
        }
    }
}


static inline picasso_rect picasso__make_circle_bounds(int x0, int y0, int radius)
{
    int overshoot = PICASSO_CIRCLE_DEFAULT_TOLERANCE + 1;

    picasso_rect r = {
        .x = x0 - radius - overshoot,
        .y = y0 - radius - overshoot,
        .width = (radius + overshoot) * 2 + 1,
        .height = (radius + overshoot) * 2 + 1,
    };
    return r;
}

void picasso_fill_circle(picasso_backbuffer *bf, int x0, int y0, int radius, color c)
{
    // create a box around the circle that is slightly larger then the radius
    // that is all we loop over, we clip to bounds
    picasso_draw_bounds bounds = {0};
    picasso_rect circle_box = picasso__make_circle_bounds(x0, y0, radius);
    if(!picasso__clip_rect_to_bounds(bf, &circle_box, &bounds)) return;

    uint32_t new_pixel = color_to_u32(c);

    // a^2 + b^2 = c^2
    for (int y = bounds.y0; y < bounds.y1; ++y) {
        for (int x = bounds.x0; x < bounds.x1; ++x) {
            int dx = x - x0;
            int dy = y - y0;
            if( (dx*dx + dy*dy <= radius*radius + radius) )
            {
                uint32_t *cur_pixel = picasso__get_pixel_u32(bf, x, y);
                *cur_pixel = picasso__blend_pixel(*cur_pixel, new_pixel);
            }
        }
    }
}

void picasso_draw_circle(picasso_backbuffer *bf, int x0, int y0, int radius,int thickness, color c)
{
    picasso_draw_bounds bounds = {0};
    picasso_rect circle_box = picasso__make_circle_bounds(x0, y0, radius);
    if (!picasso__clip_rect_to_bounds(bf, &circle_box, &bounds)) return;

    uint32_t new_pixel = color_to_u32(c);

    int outer = radius * radius;
    int inner = (radius - thickness) * (radius - thickness);

    for (int y = bounds.y0; y < bounds.y1; ++y) {
        for (int x = bounds.x0; x < bounds.x1; ++x) {
            int dx = x - x0;
            int dy = y - y0;
            int dist2 = dx * dx + dy * dy;

            if (dist2 >= inner+radius && dist2 <= outer+radius) {
                uint32_t *cur_pixel = picasso__get_pixel_u32(bf, x, y);
                *cur_pixel = picasso__blend_pixel(*cur_pixel, new_pixel);
            }
        }
    }
}
// Helper: blend a pixel into the backbuffer using alpha (0–1)
static inline void picasso__plot_aa(picasso_backbuffer *bf, int x, int y, color c, float alpha)
{
    if (x < 0 || x >= (int)bf->width || y < 0 || y >= (int)bf->height) return;

    c.a = (uint8_t)(c.a * alpha);
    uint32_t src = color_to_u32(c);
    uint32_t *dst_pixel = &bf->pixels[y * bf->width + x];
    *dst_pixel = picasso__blend_pixel(*dst_pixel, src);
}
// Draws an anti-aliased circle centered at (cx, cy) with radius r
void picasso_draw_circle_aa(picasso_backbuffer *bf, int cx, int cy, int r, color c)
{
    int x = r;
    int y = 0;

    while (x >= y)
    {
        for (int i = -1; i <= 1; ++i) // Simple approximation of AA with offset
        {
            float fx = x + i * 0.5f;
            float fy = y + i * 0.5f;
            float d = fabsf(sqrtf(fx * fx + fy * fy) - r);
            float alpha = 1.0f - fminf(d, 1.0f); // Blend alpha based on distance

            // Symmetrical 8-way plotting with anti-aliasing
            picasso__plot_aa(bf, cx + x, cy + y, c, alpha);
            picasso__plot_aa(bf, cx + y, cy + x, c, alpha);
            picasso__plot_aa(bf, cx - y, cy + x, c, alpha);
            picasso__plot_aa(bf, cx - x, cy + y, c, alpha);
            picasso__plot_aa(bf, cx - x, cy - y, c, alpha);
            picasso__plot_aa(bf, cx - y, cy - x, c, alpha);
            picasso__plot_aa(bf, cx + y, cy - x, c, alpha);
            picasso__plot_aa(bf, cx + x, cy - y, c, alpha);
        }

        y++;
        if ((x * x + y * y) > r * r)
            x--;
    }
}

// Full version of the bresenham line algorithm, works in all quadrants
// Bresenham’s algorithm keeps track of an accumulated error value that
// represents how far off the current pixel is from the actual line. It uses
// this value to decide when to adjust in x or y direction so that the
// resulting path closely follows the desired straight line.
void picasso_draw_line(picasso_backbuffer *bf, int x0, int y0, int x1, int y1, color c)
{
    // dx and dy are the distances in x and y directions.
	// absolute value to work with positive deltas regardless of direction.
    int dx = PICASSO_ABS(x1 - x0);
    int dy = PICASSO_ABS(y1 - y0);
    // sx and sy define the direction to move in for both axes
    // If x0 is less than x1, we move right (+1), otherwise left (-1), and
    // similarly for y.
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    // err starts at the diff x and y distances.
	// This hits at when to step in the y-direction vs x-direction.
    int err = dx - dy;
    uint32_t new_pixel = color_to_u32(c);

    while (true) {
        // (basic clipping)
        if (x0 >= 0 && x0 < (int)bf->width && y0 >= 0 && y0 < (int)bf->height) {
            uint32_t *dst = &bf->pixels[y0 * bf->width + x0];
            *dst = picasso__blend_pixel(*dst, new_pixel);
        }

        if (x0 == x1 && y0 == y1)
            break;

        // We double the current error (e2) so we can test both x and y
        // adjustments without using floats.
        // If the error is large enough, step in x, and adjust the error.
        // If the error is small enough, step in y, and adjust the error.

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}
//// Breaks, only works in one quadrant
//void picasso_draw_line(picasso_backbuffer *bf, int x0, int y0, int x1, int y1, color c)
//{
//    uint32_t new_pixel = color_to_u32(c);
//
//    /* Bresenhams lines algorithm
//     * */
//    int dx = x1-x0;
//    int dy = y1-y0;
//    int D = 2*dy-dx;
//    int y = y0;
//    for(int i = x0; i < x1; ++i){
//        bf->pixels[y*bf->width + i] = new_pixel;
//        if (D > 0) {
//            y++;
//            D -= 2*dx;
//        }
//        D += 2*dy;
//    }
//}




// Draws an anti-aliased line using Xiaolin Wu's algorithm
// https://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm
void picasso_draw_line_aa(picasso_backbuffer *bf, float x0, float y0, float x1, float y1, color c)
{
    // Check if the line is steep
    int steep = fabsf(y1 - y0) > fabsf(x1 - x0);
    // If steep, swap x and y to simplify the loop
    if (steep) {
        float tmp;
        tmp = x0; x0 = y0; y0 = tmp;
        tmp = x1; x1 = y1; y1 = tmp;
    }
    // Ensure the line goes left to right
    if (x0 > x1) {
        float tmp;
        tmp = x0; x0 = x1; x1 = tmp;
        tmp = y0; y0 = y1; y1 = tmp;
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = dx == 0 ? 1 : dy / dx;

    int x_start = (int)floorf(x0);
    float y = y0 + gradient * (x_start - x0);

    // Loop from x0 to x1, calculating the corresponding y value
    for (int x = x0; x <= x1; ++x) {
        int y_int = (int)floorf(y);         // Integer part of y
        float y_frac = y - y_int;           // Fractional part (for blending)

        // Blend pixels based on coverage
        if (steep) {
            // If the line was steep, draw transposed
            picasso__plot_aa(bf, y_int,     x, c, 1.0f - y_frac);
            picasso__plot_aa(bf, y_int + 1, x, c, y_frac);
        } else {
            picasso__plot_aa(bf, x, y_int,     c, 1.0f - y_frac);
            picasso__plot_aa(bf, x, y_int + 1, c, y_frac);
        }

        y += gradient; // Move to next y
    }
}
void picasso_draw_line_thick(picasso_backbuffer *bf, int x0, int y0, int x1, int y1, int thickness, color c)
{
    int steps = sqrtf((x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0));
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / steps;
        int x = (int)(x0 + t * (x1 - x0));
        int y = (int)(y0 + t * (y1 - y0));
        picasso_fill_circle_aa(bf, x, y, thickness / 2, c);
    }
}

void picasso_fill_circle_aa(picasso_backbuffer *bf, int cx, int cy, int radius, color c)
{
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            int px = cx + x;
            int py = cy + y;

            // Euclidean distance from center
            float dist = sqrtf(x * x + y * y);

            if (dist < radius - 1.0f)
            {
                // Inside the circle core — fully opaque
                picasso__plot_aa(bf, px, py, c, 1.0f);
            }
            else if (dist <= radius)
            {
                // Edge pixels — fade out with distance to smooth edge
                float alpha = radius - dist;
                picasso__plot_aa(bf, px, py, c, alpha);
            }
            // Outside the circle — do nothing
        }
    }
}

void picasso_fill_triangle(picasso_backbuffer *bf, picasso_point3 pts, color c)
{
    // Unpack input
    int x0 = pts.x1, y0 = pts.y1;
    int x1 = pts.x2, y1 = pts.y2;
    int x2 = pts.x3, y2 = pts.y3;

    // Clamp with proper types to avoid warnings
    int min_x = PICASSO_CLAMP(PICASSO_MIN3(x0, x1, x2), 0, (int)bf->width - 1);
    int max_x = PICASSO_CLAMP(PICASSO_MAX3(x0, x1, x2), 0, (int)bf->width - 1);
    int min_y = PICASSO_CLAMP(PICASSO_MIN3(y0, y1, y2), 0, (int)bf->height - 1);
    int max_y = PICASSO_CLAMP(PICASSO_MAX3(y0, y1, y2), 0, (int)bf->height - 1);

    float denom = (float)((y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2));

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            float w0 = ((y1 - y2)*(x - x2) + (x2 - x1)*(y - y2)) / denom;
            float w1 = ((y2 - y0)*(x - x2) + (x0 - x2)*(y - y2)) / denom;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                uint32_t src = color_to_u32(c);
                uint32_t *dst = &bf->pixels[y * bf->width + x];
                *dst = picasso__blend_pixel(*dst, src);
            }
        }
    }
}

void picasso_draw_triangle_aa(picasso_backbuffer *bf, picasso_point3 pts, color fill_color, color edge_color)
{
    // 1. Fill the triangle with aliasing (solid fill, no smoothing inside)
    picasso_fill_triangle(bf, pts, fill_color);

    // 2. Draw anti-aliased edges
    picasso_draw_line_aa(bf, pts.x1, pts.y1, pts.x2, pts.y2, edge_color);
    picasso_draw_line_aa(bf, pts.x2, pts.y2, pts.x3, pts.y3, edge_color);
    picasso_draw_line_aa(bf, pts.x3, pts.y3, pts.x1, pts.y1, edge_color);
}

// ------- Bezier support TODO: place these at a suitable place later
picasso_vec2 vector_add(picasso_vec2 v1, picasso_vec2 v2)
{
    return (picasso_vec2) { .x = v1.x + v2.x, .y = v1.y + v2.y };
}
picasso_vec2 vector_sub(picasso_vec2 v1, picasso_vec2 v2)
{
    return (picasso_vec2) { .x = v1.x - v2.x, .y = v1.y - v2.y };
}
picasso_vec2 vector_scale(picasso_vec2 v1, float scale)
{
    return (picasso_vec2) { .x = v1.x * scale, .y = v1.y * scale };
}
picasso_vec2 lerp_vec2(picasso_vec2 v1, picasso_vec2 v2, float t)
{
    return vector_add(v1, vector_scale(vector_sub(v2, v1), t));
}

picasso_vec2 bezier_lerp(picasso_vec2 p0, picasso_vec2 p1, picasso_vec2 p2, float t)
{
    picasso_vec2 inter1 = lerp_vec2(p0, p1, t);
    picasso_vec2 inter2 = lerp_vec2(p1, p2, t);
    return lerp_vec2(inter1, inter2, t);
}

void draw_bezier(picasso_backbuffer *bf,
                        picasso_vec2 p0, picasso_vec2 p1, picasso_vec2 p2,
                        int resolution)
{
   picasso_vec2 prev_point = p0;

   for (int i = 0; i < resolution; ++i)
   {
       float t = (i + 1.f) / resolution;
       picasso_vec2 next_point = bezier_lerp(p0, p1, p2, t);
//       picasso_draw_thick_line_soft(bf, 100, 100, 400, 400,4, PINK);
       //picasso_draw_line_aa(bf, prev_point.x, prev_point.y, next_point.x, next_point.y, RED);
       picasso_draw_line_thick(bf, prev_point.x, prev_point.y, next_point.x, next_point.y, 4,RED);
       prev_point = next_point;
   }
}

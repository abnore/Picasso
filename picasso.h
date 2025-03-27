#ifndef PICASSO_H
#define PICASSO_H
/* Will support BMP, PPM, PNG and eventually JPG
 * */
#include "common.h"
/* -------------------- Color Section -------------------- */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
}color;

// RBGA layout expected by Cocoa and NSBitmapImageRep
// Primary Colors                     r     g     .b     a
#define CANOPY_BLUE         ((color){0x00, 0x00, 0xFF, 0x50})
#define CANOPY_GREEN        ((color){0x00, 0xFF, 0x00, 0xFF})
#define CANOPY_RED          ((color){0xFF, 0x00, 0x00, 0xFF})

// Grayscale
#define CANOPY_WHITE        ((color){0xFF, 0xFF, 0xFF, 0xFF})
#define CANOPY_BLACK        ((color){0x00, 0x00, 0x00, 0xFF})
#define CANOPY_GRAY         ((color){0x30, 0x30, 0x30, 0xFF})
#define CANOPY_LIGHT_GRAY   ((color){0x80, 0x80, 0x80, 0xFF})
#define CANOPY_DARK_GRAY    ((color){0x20, 0x20, 0x20, 0xFF})

// Warm Tones
#define CANOPY_ORANGE       ((color){0xFF, 0x80, 0x00, 0xFF})  // R: 255, G: 128, B: 0
#define CANOPY_YELLOW       ((color){0xFF, 0xFF, 0x00, 0xFF})  // R: 255, G: 255, B: 0
#define CANOPY_BROWN        ((color){0x80, 0x60, 0x20, 0xFF})  // R: 128, G: 96, B: 32
#define CANOPY_GOLD         ((color){0xFF, 0xD7, 0x00, 0xFF})  // R: 255, G: 215, B: 0

// Cool Tones
#define CANOPY_CYAN         ((color){0x00, 0xFF, 0xFF, 0xFF})  // R: 0, G: 255, B: 255
#define CANOPY_MAGENTA      ((color){0xFF, 0x00, 0xFF, 0xFF})  // R: 255, G: 0, B: 255
#define CANOPY_PURPLE       ((color){0x80, 0x00, 0x80, 0xFF})  // R: 128, G: 0, B: 128
#define CANOPY_NAVY         ((color){0x00, 0x00, 0x80, 0xFF})  // R: 0, G: 0, B: 128
#define CANOPY_TEAL         ((color){0x00, 0x80, 0x80, 0xFF})  // R: 0, G: 128, B: 128



static inline uint32_t color_to_u32(color c) {
    return ((uint32_t)c.a << 24) |
           ((uint32_t)c.b << 16) |
           ((uint32_t)c.g << 8)  |
           ((uint32_t)c.r);
}

static inline color u32_to_color(uint32_t val) {
    return (color){
        .a = (val >> 24) & 0xFF,
        .b = (val >> 16) & 0xFF,
        .g = (val >> 8)  & 0xFF,
        .r = val & 0xFF
    };
}

/* -------------------- Format Section -------------------- */

// Define BMP file header structures
#pragma pack(push,1) //https://www.ibm.com/docs/no/zos/2.4.0?topic=descriptions-pragma-pack
typedef struct {
    uint16_t file_type;                 // File type always ascii `BM` which is 0x4D42
    uint32_t file_size;                 // Size of the file (in bytes)
    uint16_t reserved1;                 // Reserved, always 0
    uint16_t reserved2;                 // Reserved, always 0
    uint32_t offset_data;               // Start position of pixel data (bytes from the beginning of the file)
} BMP_file_header;

typedef struct {
    uint32_t size;                      // Size of this header (in bytes)
    int32_t width;                      // width of bitmap in pixels
    int32_t height;                     // width of bitmap in pixels
    uint16_t planes;                    // No. of planes for the target device, this is always 1
    uint16_t bit_count;                 // No. of bits per pixel
    uint32_t compression;               // 0 or 3 - uncompressed (BI_RGB)
    uint32_t size_image;                // 0 - for uncompressed images
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;               // No. color indexes in the color table. Use 0 for the max number of colors allowed by bitCount
    uint32_t colors_important;          // No. of colors used for displaying the bitmap. If 0 all colors are required
} BMP_info_header;

typedef struct {
        BMP_file_header fh;
        BMP_info_header ih;
        uint8_t *image_data;
}BMP;

/* PPM header is literal ascii - must be parsed
 * like a text file, not with headers.
 *  5036 0a33 3030 2032 3030 0a32 3535 0a
 *  P 6  \n3  0 0    2  0  0 \n2   5 5 \n
 * */
typedef struct {
    size_t width;
    size_t height;
    size_t maxval;
    uint32_t *pixels;
}PPM;

#pragma pack(pop)

BMP *picasso_load_bmp(const char *filename);
int picasso_save_to_bmp(BMP *image, const char *filename);
void picasso_flip_buffer_vertical(uint8_t *buffer, int width, int height);

PPM *picasso_load_ppm(const char *filename);
int picasso_save_to_ppm(PPM *image, const char *file_path);

/* -------------------- Surface Section -------------------- */

typedef struct {
    int width;
    int height;
    int pitch;
    uint32_t* pixels;
} picasso_surface;

picasso_surface* picasso_create_surface(int width, int height);
void picasso_destroy_surface(picasso_surface* surface);
void picasso_clear_surface(picasso_surface* surface, uint32_t color);
void picasso_blit_surface(picasso_surface* dst, picasso_surface* src, int x, int y);
void picasso_blit_bitmap(picasso_surface* dst, void* src_pixels, int src_w, int src_h, int x, int y, bool blend);
void* picasso_get_pixels(picasso_surface* surface);

void picasso_fill_canvas(color *pixels, size_t width, size_t height, color c);

#endif // PICASSO_H

#ifndef PICASSO_H
#define PICASSO_H
/* Will support BMP, PPM, PNG and eventually JPG
 * */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* -------------------- Image Object -------------------- */
typedef struct {
    int width;
    int height;
    int channels; // 3 = RGB, 4 = RGBA
    int row_stride;
    uint8_t *pixels;
} picasso_image;

/* -------------------- Custom Allocators -------------------- */
void* picasso_calloc(size_t count, size_t size);
void picasso_free(void *ptr);
void *picasso_malloc(size_t size);
void * picasso_realloc(void *ptr, size_t size);

/* --------- Binary Readers little endian utilities ----------- */
uint8_t picasso_read_u8(const uint8_t *p);
uint16_t picasso_read_u16_le(const uint8_t *p);
uint32_t picasso_read_u32_le(const uint8_t *p);
int32_t  picasso_read_s32_le(const uint8_t *p);

/* -------------------- File Support -------------------- */
void *picasso_read_entire_file(const char *path, size_t *out_size);
int picasso_write_file(const char *path, const void *data, size_t size);

void picasso_free_image(picasso_image *img);
picasso_image *picasso_alloc_image(int width, int height, int channels);

/* -------------------- ICC Profile Support -------------------- */
typedef enum {
    PICASSO_PROFILE_NONE = 0,
    PICASSO_PROFILE_ACESCG_LINEAR,
    PICASSO_PROFILE_ADOBERGB1998,
    PICASSO_PROFILE_DCI_P3_RGB,
    PICASSO_PROFILE_DISPLAY_P3,
    PICASSO_PROFILE_GENERIC_CMYK,
    PICASSO_PROFILE_GENERIC_GRAY_GAMMA_2_2,
    PICASSO_PROFILE_GENERIC_GRAY,
    PICASSO_PROFILE_GENERIC_LAB,
    PICASSO_PROFILE_GENERIC_RGB,
    PICASSO_PROFILE_GENERIC_XYZ,
    PICASSO_PROFILE_ITU_2020,
    PICASSO_PROFILE_ITU_709,
    PICASSO_PROFILE_ROMM_RGB,
    PICASSO_PROFILE_SRGB,
} picasso_icc_profile;

/* -------------------- Utility macros -------------------- */

#define PICASSO_ABS(a)     ({ __typeof__(a) _a = (a); _a > 0 ? _a : -_a; })
#define PICASSO_MAX(a,b)   ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })
#define PICASSO_MIN(a,b)   ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a < _b ? _a : _b; })
#define PICASSO_SWAP(a,b)  ({ __typeof__(a) _a = (a); (a) = (b); (b) = _a;})


// Set alpha value in percent, where 0 is transparent and 100 is fully opaque
#define SET_ALPHA(c, percent)   ((color){(c).r,(c).g, (c).b, \
                                (uint8_t)(((percent)*255)/100) })
#define GET_RED(c)   ((uint8_t)(c).r)
#define GET_GREEN(c) ((uint8_t)(c).g)
#define GET_BLUE(c)  ((uint8_t)(c).b)
#define GET_ALPHA(c) ((uint8_t)(c).a)

#define PICASSO_CIRCLE_DEFAULT_TOLERANCE 2
/* -------------------- Color Section -------------------- */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
}color;

// https://colors.artyclick.com/color-names-dictionary/color-names/phthalo-blue-color
// RGBA layout expected by Cocoa and NSBitmapImageRep
// Primary Colors              .r    .g    .b    .a
#define BLUE         ((color){0x0C, 0x10, 0x89, 0xFF}) // 000F89 Phthalo Blue
#define GREEN        ((color){0x31, 0x85, 0x20, 0xFF}) // 318520 Medium Spring Green
#define RED          ((color){0xCC, 0x00, 0x03, 0xFF}) // CC0003 Corso Red

#define PINK         ((color){0xCE, 0x7A, 0xDF, 0xFF}) // CE7ADF Orchid
// Grayscale
#define WHITE        ((color){0xFF, 0xFF, 0xFF, 0xFF}) // max white
#define BLACK        ((color){0x00, 0x00, 0x00, 0xFF}) // opaque black
#define GRAY         ((color){0x30, 0x30, 0x30, 0xFF})
#define LIGHT_GRAY   ((color){0x80, 0x80, 0x80, 0xFF})
#define DARK_GRAY    ((color){0x20, 0x20, 0x20, 0xFF})

// Warm Tones
#define ORANGE       ((color){0xFF, 0x80, 0x00, 0xFF})  // R: 255, G: 128, B: 0
#define YELLOW       ((color){0xF6, 0xDB, 0x0E, 0xFF})  // F6DB0E Candlelight
#define BROWN        ((color){0x80, 0x60, 0x20, 0xFF})  // R: 128, G: 96, B: 32
#define GOLD         ((color){0xFF, 0xD7, 0x00, 0xFF})  // R: 255, G: 215, B: 0

// Cool Tones
#define CYAN         ((color){0x00, 0xFF, 0xFF, 0xFF})  // R: 0, G: 255, B: 255
#define MAGENTA      ((color){0xFF, 0x00, 0xFF, 0xFF})  // R: 255, G: 0, B: 255
#define PURPLE       ((color){0x80, 0x00, 0x80, 0xFF})  // R: 128, G: 0, B: 128
#define NAVY         ((color){0x00, 0x00, 0x80, 0xFF})  // R: 0, G: 0, B: 128
#define TEAL         ((color){0x00, 0x80, 0x80, 0xFF})  // R: 0, G: 128, B: 128

// Background color - for now dark gray to fit dark mode - change if you want
#define CLEAR_BACKGROUND DARK_GRAY// dark mode background

#define color_to_u32(c) (((uint32_t)(c).a << 24) |      \
                         ((uint32_t)(c).b << 16) |      \
                         ((uint32_t)(c).g << 8)  |      \
                         ((uint32_t)(c).r))

#define u32_to_color(val) ((color){                     \
    .a = (uint8_t)(((val) >> 24) & 0xFF),               \
    .b = (uint8_t)(((val) >> 16) & 0xFF),               \
    .g = (uint8_t)(((val) >> 8) & 0xFF),                \
    .r = (uint8_t)((val) & 0xFF)                        \
})

const char* color_to_string(color c);

/* -------------------- Format Section -------------------- */

#define PICASSO_MAX_DIM 1<<14 // 16,384X16,384 *4 is over 1GB - that is enough

// Define BMP file header structures
#pragma pack(push,1) //https://www.ibm.com/docs/no/zos/2.4.0?topic=descriptions-pragma-pack

//BITMAPV4HEADER up to v5 fields
//BITMAPV5HEADER after that
//https://en.wikipedia.org/wiki/BMP_file_format#Usage_of_BMP_format
typedef struct {
    uint16_t file_type;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset_data;
}bmp_fh; // file header

typedef struct {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t size_image;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t colors_important;

    // V4
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;
    uint32_t cs_type;
    int32_t endpoints[9];
    uint32_t gamma_red;
    uint32_t gamma_green;
    uint32_t gamma_blue;

    // V5
    uint32_t intent;
    uint32_t profile_data;
    uint32_t profile_size;
    uint32_t reserved;
} bmp_ih; // info header

typedef struct {
    bmp_fh fh;
    bmp_ih ih;
    uint8_t *pixels;
} bmp;

/* PPM header is literal ascii - must be parsed
 * like a text file, not with headers.
 *  5036 0a33 3030 2032 3030 0a32 3535 0a
 *  P 6  \n3  0 0    2  0  0 \n2   5 5 \n
 * */
typedef struct {
    size_t width;
    size_t height;
    size_t maxval;
    uint8_t *pixels;
}ppm;

#pragma pack(pop)


/// @brief BMP functions
picasso_image *picasso_load_bmp(const char *filename);
int picasso_save_to_bmp(bmp *image, const char *file_path, picasso_icc_profile profile);
bmp *picasso_create_bmp_from_rgba(int width, int height, int channels, const uint8_t *pixel_data);
int picasso_save_rgba_to_bmp(const char *file_path, int width, int height, int channels, const uint8_t *pixels, picasso_icc_profile profile);

/// @brief PPM functions
picasso_image *picasso_load_ppm(const char *filename);
int picasso_save_to_ppm(ppm *image, const char *file_path);


/* ------------------- SpriteSheet Section -------------------- */

/// @brief Represents a single sprite frame in a sheet.
typedef struct {
    int x;       ///< X position (in pixels) in the sheet
    int y;       ///< Y position (in pixels) in the sheet
    int width;   ///< Width of the frame
    int height;  ///< Height of the frame
} picasso_sprite;

/// @brief Represents a sprite sheet made from a bitmap or other image.
typedef struct {
    uint32_t* pixels;         ///< Raw pixel data (entire sheet)
    int sheet_width;          ///< Width of full image
    int sheet_height;         ///< Height of full image
    int frame_width;          ///< Width of each frame
    int frame_height;         ///< Height of each frame
    int spacing_x;            ///< Spacing between frames (horizontal)
    int spacing_y;            ///< Spacing between frames (vertical)
    int margin_x;             ///< Margin at edges (horizontal)
    int margin_y;             ///< Margin at edges (vertical)
    int frames_per_row;       ///< How many sprites per row
    int frames_per_col;       ///< How many sprites per column
    int frame_count;          ///< Total number of frames

    picasso_sprite* frames;   ///< Array of sprite metadata (x, y, width, height)
} picasso_sprite_sheet;

void free_sprite_sheet(picasso_sprite_sheet* sheet);
picasso_sprite_sheet* picasso_create_sprite_sheet( uint32_t* pixels, int sheet_width, int sheet_height,
                                                    int frame_width, int frame_height,
                                                    int margin_x, int margin_y,
                                                    int spacing_x, int spacing_y);

/* -------------------- Backbuffer Section -------------------- */

typedef struct {
    uint32_t* pixels;
    uint32_t width, height, pitch;
} picasso_backbuffer;

picasso_backbuffer* picasso_create_backbuffer(int width, int height);
void picasso_destroy_backbuffer(picasso_backbuffer *bf);
void picasso_clear_backbuffer(picasso_backbuffer *bf);
void picasso_blit_bitmap(picasso_backbuffer *dst, void *src_pixels, int src_w, int src_h, int x, int y);
void* picasso_backbuffer_pixels(picasso_backbuffer *bf);

/* -------------------- Graphical Raster Section -------------------- */
typedef struct {
    int x, y, width, height; // supporting negative values
} picasso_rect;


typedef struct {
    int x0, y0, x1, y1;
} picasso_draw_bounds;

void picasso_fill_rect(picasso_backbuffer *bf, picasso_rect *r, color c);
void picasso_draw_rect(picasso_backbuffer *bf, picasso_rect *outer, int thickness, color c);

void picasso_draw_line(picasso_backbuffer *bf, int x0, int y0, int x1, int y1, color c);

void picasso_draw_circle(picasso_backbuffer *bf, int x0, int y0, int radius,int thickness, color c);
void picasso_fill_circle(picasso_backbuffer *bf, int x0, int y0, int radius, color c);

#endif // PICASSO_H

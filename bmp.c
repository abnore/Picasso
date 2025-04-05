#include <stdbool.h>
#include <string.h>
#include "picasso.h"
#include "logger.h"
#include "picasso_icc_profiles.h"

#define LCS_GM_ABS_COLORIMETRIC   0x00000008
#define LCS_GM_BUSINESS           0x00000001  // Saturation
#define LCS_GM_GRAPHICS           0x00000002  // Relative colorimetric
#define LCS_GM_IMAGES             0x00000004  // Perceptual

#define bits_to_bytes(x) ((x)>>3)
#define bytes_to_bits(x) ((x)<<3)

typedef enum {
    BITMAP_INVALID     = -1,
    BITMAPCOREHEADER   = 12,  // OS/2 1.x — rarely used but stb supports it
    BITMAPINFOHEADER   = 40,  // Most common, basic 24/32-bit BMPs
    BITMAPV3INFOHEADER = 56,  // Unofficial Adds color masks. Rarely seen.
    BITMAPV4HEADER     = 108, // Adds color space and gamma (optional)
    BITMAPV5HEADER     = 124  // Adds ICC profile (optional)
} bmp_header_type;

typedef enum {
    // Native Windows color space ('Win ' in little-endian)
    LCS_WINDOWS_COLOR_SPACE = 0x57696E20,
    // Standard sRGB color space ('sRGB' in little-endian) — this is the most common.
    LCS_sRGB                = 0x73524742,
    // Custom ICC profile embedded in file ('MBED' in little-endian, displayed as 0x4D424544)
    PROFILE_EMBEDDED        = 0x4D424544,
    // ICC profile is in an external file ('LINK' in little-endian)
    PROFILE_LINKED          = 0x4C494E4B,
} bmp_cs_type;

static const char *picasso_icc_profile_name(picasso_icc_profile profile) {
    switch (profile) {
        case PICASSO_PROFILE_NONE: return "None";
        case PICASSO_PROFILE_ACESCG_LINEAR: return "ACESCG Linear";
        case PICASSO_PROFILE_ADOBERGB1998: return "AdobeRGB1998";
        case PICASSO_PROFILE_DCI_P3_RGB: return "DCI(P3) RGB";
        case PICASSO_PROFILE_DISPLAY_P3: return "Display P3";
        case PICASSO_PROFILE_GENERIC_CMYK: return "Generic CMYK Profile";
        case PICASSO_PROFILE_GENERIC_GRAY_GAMMA_2_2: return "Generic Gray Gamma 2.2 Profile";
        case PICASSO_PROFILE_GENERIC_GRAY: return "Generic Gray Profile";
        case PICASSO_PROFILE_GENERIC_LAB: return "Generic Lab Profile";
        case PICASSO_PROFILE_GENERIC_RGB: return "Generic RGB Profile";
        case PICASSO_PROFILE_GENERIC_XYZ: return "Generic XYZ Profile";
        case PICASSO_PROFILE_ITU_2020: return "ITU-2020";
        case PICASSO_PROFILE_ITU_709: return "ITU-709";
        case PICASSO_PROFILE_ROMM_RGB: return "ROMM RGB";
        case PICASSO_PROFILE_SRGB: return "sRGB Profile";
        default: return "Unknown";
    }
}

static bool picasso_embed_icc_profile(bmp *image, picasso_icc_profile profile, FILE *out)
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

void picasso_flip_buffer_vertical(uint8_t *buffer, int width, int height, int channels)
{
    int row_size = ((width * channels + 3) / 4) * 4; // include padding!

    TRACE("Flipping buffer vertically (%dx%d) channels: %d, row_size: %d", width, height, channels, row_size);

    uint8_t temp_row[row_size];

    for (int y = 0; y < height / 2; y++) {
        int top_index = y * row_size;
        int bottom_index = (height - y - 1) * row_size;

        memcpy(temp_row, &buffer[top_index], row_size);
        memcpy(&buffer[top_index], &buffer[bottom_index], row_size);
        memcpy(&buffer[bottom_index], temp_row, row_size);
    }

    TRACE("Finished vertical flip");
}

int picasso_save_to_bmp(bmp *image, const char *file_path, picasso_icc_profile profile)
{
    FILE *f = fopen(file_path, "wb");
    if (!f) {
        ERROR("Failed to open BMP file for writing: %s", file_path);
        return -1;
    }
    TRACE("Opened BMP file for writing: %s", file_path);

    int width = image->ih.width;
    int height = image->ih.height;
    int channels = bits_to_bytes(image->ih.bit_count);
    bool top_down = height < 0;
    if (top_down) height = -height;

    int row_stride = width * channels;
    int row_size = ((row_stride + 3) / 4) * 4;
    size_t pixel_array_size = (size_t)row_size * height;

    TRACE("row stride %d vs size %d, pixel array size %zu", row_stride, row_size,
            pixel_array_size);
    // Flip buffer for bottom-up BMP format
    bool flipped = false;
    if (top_down) {
        picasso_flip_buffer_vertical(image->pixels, width, height, channels);
        image->ih.height = height; // temporarily positive
        flipped = true;
    }

    // Set profile location if needed
    if (profile != PICASSO_PROFILE_NONE) {
        image->ih.profile_data = image->fh.offset_data + (uint32_t)pixel_array_size;
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
    TRACE("Writing pixel data row by row");
    TRACE("row_stride = %d, row_size = %d", row_stride, row_size);

    // Write pixel data (already BGRA padded)
    if (fwrite(image->pixels, 1, pixel_array_size, f) != pixel_array_size) {
        ERROR("Failed to write BMP pixel data");
        fclose(f);
        return -1;
    }

// Embed ICC profile if requested
    if (profile != PICASSO_PROFILE_NONE) {
        if (!picasso_embed_icc_profile(image, profile, f)) {
            WARN("Failed to embed ICC profile");
        } else {
            INFO("Embedded ICC profile: %s", picasso_icc_profile_name(profile));
        }
    }

    fclose(f);
    TRACE("Finished writing BMP");

    // Restore top-down height and buffer
    if (flipped) {
        picasso_flip_buffer_vertical(image->pixels, width, height, channels);
        image->ih.height = -height;
    }

    INFO("Saved BMP with ICC to %s", file_path);
    return 0;
}

bmp *picasso_create_bmp_from_rgba(int width, int height, int channels, const uint8_t *pixel_data)
{
    if (width <= 0 || height == 0 || !pixel_data) {
        ERROR("Invalid BMP creation params: %dx%d", width, height);
        return NULL;
    }
    bool all_alpha_zero = (channels == 4); // I only care if alpha exist
    int abs_height = PICASSO_ABS(height);
    int row_stride = width * channels;                         // tightly packed source
    int row_size   = ((row_stride + 3) / 4) * 4;               // padded BMP row size
    size_t pixel_array_size = (size_t)row_size * abs_height;

    bmp *b = picasso_malloc(sizeof(bmp));
    if (!b) return NULL;
    memset(b, 0, sizeof(bmp));

    // --- File header ---
    b->fh.file_type = 0x4D42; // 'BM'
    b->fh.offset_data = sizeof(b->fh) + sizeof(b->ih);
    b->fh.file_size = b->fh.offset_data + (uint32_t)pixel_array_size;

    // --- Info header ---
    b->ih.size = sizeof(b->ih);
    b->ih.width = width;
    b->ih.height = -abs_height; // store top-down
    b->ih.planes = 1;
    b->ih.bit_count = (uint16_t)(bytes_to_bits(channels));
    b->ih.compression = (channels == 4) ? 3 : 0;  // BI_BITFIELDS if 32-bit
    b->ih.size_image = (uint32_t)pixel_array_size;
    b->ih.x_pixels_per_meter = 3780;
    b->ih.y_pixels_per_meter = 3780;

    if (channels == 4) {
        b->ih.red_mask   = 0x00FF0000;
        b->ih.green_mask = 0x0000FF00;
        b->ih.blue_mask  = 0x000000FF;
        b->ih.alpha_mask = 0xFF000000;
    }

    b->ih.cs_type = LCS_sRGB;
    b->ih.intent = LCS_GM_IMAGES;

    // --- Allocate padded pixel buffer ---
    b->pixels = picasso_malloc(pixel_array_size);
    if (!b->pixels) {
        picasso_free(b);
        return NULL;
    }

    // --- Fill each row ---
    for (int y = 0; y < abs_height; ++y) {
        const uint8_t *src_row = pixel_data + y * row_stride;
        uint8_t *dst_row = b->pixels + y * row_size;

        for (int x = 0; x < width; ++x) {
            const uint8_t *src = src_row + x * channels;
            uint8_t *dst = dst_row + x * channels;

            // Swap RGBA → BGRA
            dst[0] = src[2]; // B
            dst[1] = src[1]; // G
            dst[2] = src[0]; // R
            if (channels == 4) {
                dst[3] = src[3]; // Respect original alpha
                if(src[3] != 0) all_alpha_zero = false;
            }
        }
        // Fill padding bytes with zeros
        int padding = row_size - row_stride;
        if (padding > 0) {
            memset(dst_row + row_stride, 0, padding);
        }
    }

    if (channels == 4 && all_alpha_zero) {
        TRACE("All alpha values were zero — replacing with opaque alpha");
        for (int y = 0; y < abs_height; ++y) {
            uint8_t *dst_row = b->pixels + y * row_size;
            for (int x = 0; x < width; ++x) {
                dst_row[x * 4 + 3] = 0xFF;
            }
        }
    }

    TRACE("BMP created (%dx%d @ %d-bit, padded rows)", width, abs_height, channels * 8);
    return b;
}

char * _print_cs_type( bmp_cs_type type)
{
    switch(type) {
        case LCS_sRGB: // = 0x73524742, // Standard sRGB color space ('sRGB' in little-endian) â€” this is the most common.
            return "LCS_sRBG = 0x73524742";
            break;
        case LCS_WINDOWS_COLOR_SPACE: // = 0x57696E20, // Native Windows color space ('Win ' in little-endian)
            return "LCS_WINDOWS_COLOR_SPACE = 0x57696E20";
            break;
        case PROFILE_EMBEDDED: // = 0x4D424544, // Custom ICC profile embedded in file ('DEBM' / 'MBED' in little-endian)
            return "PROFILE_EMBEDDED = 0x4D424544";
            break;
        case PROFILE_LINKED: // = 0x4C494E4B, // ICC profile is in an external file ('LINK')
            return "PROFILE_LINKED = 0x4C494E4B";
            break;
        default:
            return "Unknown";
            break;
    }
}
char *_print_header_type(bmp_header_type type)
{
    switch(type) {
        case BITMAPCOREHEADER:
            return "bitmapcoreheader";
            break;
        case BITMAPINFOHEADER:
            return "bitmapinfoheader";
            break;
        case BITMAPV3INFOHEADER:
            return "BITMAPV3INFOHEADER";
            break;
        case BITMAPV4HEADER:
            return "bitmapv4header";
            break;
        case BITMAPV5HEADER:
            return "bitmapv5header";
            break;
        default:
            return "Not supported";
            break;
    }
}

typedef struct {
    bmp image;
    bmp_header_type type;
    int bytes_pp, width, height, row_size, size_image;
    bool is_flipped;
    uint32_t rm, gm, bm, am;
}_bmp_load_info;

static bmp_header_type picasso__decide_bmp_format(size_t *read, bmp *b, int offset, FILE *fp)
{
    *read += fread(&b->ih, 1, offset, fp);
    if (*read != (size_t)offset + sizeof(bmp_fh)) {
        ERROR("Corrupted BMP, aborting load read %zu", *read);
        return BITMAP_INVALID;
    }

    TRACE("header type is %s", _print_header_type(b->ih.size));

    if (b->ih.width > PICASSO_MAX_DIM || b->ih.height > PICASSO_MAX_DIM) {
        ERROR("File too large, most likely corrupted");
        return BITMAP_INVALID;
    }

    TRACE("cs_type:  %s ", _print_cs_type(b->ih.cs_type));

    return (bmp_header_type)b->ih.size;
}

static bmp_header_type picasso__validate_bmp(size_t *read, bmp *b, FILE *fp)
{
    bmp_header_type type;
    *read += fread(&b->fh, 1, sizeof(bmp_fh), fp);

    if (b->fh.file_type != 0x4D42) {
        ERROR("Not a valid BMP");
        fclose(fp);
        return BITMAP_INVALID;
    }

    TRACE("file size %i", b->fh.file_size);
    TRACE("file type %i", b->fh.offset_data);

    int offset = b->fh.offset_data - *read;
    type = picasso__decide_bmp_format(read, b, offset, fp);

    if (type < 0) {
        fclose(fp);
    }

    return type;
}

static void picasso__parse_coreheader_fields(_bmp_load_info *bmp)
{
    bmp->is_flipped = bmp->image.ih.height > 0;
    bmp->width      = bmp->image.ih.width;
    bmp->height     = PICASSO_ABS(bmp->image.ih.height);
    TRACE("width              = %d", bmp->width);
    TRACE("height             = %d", bmp->height);
    TRACE("is_flipped         = %s", bmp->is_flipped ? "true" : "false");
}
static void picasso__parse_infoheader_fields(_bmp_load_info *bmp)
{
    bmp->bytes_pp = bits_to_bytes(bmp->image.ih.bit_count);
    bmp->size_image = bmp->image.ih.size_image;

    TRACE("bit_count          = %d", bmp->image.ih.bit_count);
    TRACE("compression        = %u", bmp->image.ih.compression);
    TRACE("size_image         = %u", bmp->size_image);

    // fallback if size_image is zero (allowed by spec for BI_RGB)
    if (bmp->size_image == 0) {
        int row_stride = ((bmp->width * bmp->bytes_pp + 3) / 4) * 4;
        bmp->row_size = row_stride;
        bmp->size_image = bmp->row_size * bmp->height;
    } else {
        bmp->row_size = bmp->size_image / bmp->height;
    }
}
static void picasso__parse_v5_fields(_bmp_load_info *bmp)
{
    TRACE("do nothing yet AAAAAA");
    uint32_t intent         = bmp->image.ih.intent;
    TRACE("intent : %d",bmp->image.ih.intent);
    ASSERT(intent == LCS_GM_IMAGES, "so far so good");
//    uint32_t profile_data   = bmp->image.ih.profile_data;
//    uint32_t profile_size   = bmp->image.ih.profile_size;
//    uint32_t reserved       = bmp->image.ih.reserved;
}
static void picasso__parse_v4_fields(_bmp_load_info *bmp){

    TRACE("do nothing yet AAAAAA");

    /* every row has to be divisble by 4 DWORD aligned */
    bmp->row_size = ((bmp->image.ih.bit_count * bmp->width+31)/32) * 4;
    bmp->size_image = bmp->row_size * bmp->height;
//        uint32_t cs_type;
//        int32_t endpoints[9];
//        uint32_t gamma_red;
//        uint32_t gamma_green;
//        uint32_t gamma_blue;
}

static void picasso__parse_v3_fields(_bmp_load_info *bmp)
{
    bmp->rm = bmp->image.ih.red_mask;
    bmp->gm = bmp->image.ih.green_mask;
    bmp->bm = bmp->image.ih.blue_mask;
    bmp->am = bmp->image.ih.alpha_mask;
}




/* Robust, and should handle all format now.. */
picasso_image *bmp_load_from_file(const char *filename)
{
    _bmp_load_info bmp = {0};
    size_t read = 0;
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    bmp.type = picasso__validate_bmp(&read, &bmp.image, fp);
    if (bmp.type == BITMAP_INVALID) {
        fclose(fp);
        return NULL;
    }

    picasso__parse_coreheader_fields(&bmp);
    if (bmp.type >= BITMAPINFOHEADER) picasso__parse_infoheader_fields(&bmp);
    if (bmp.type >= BITMAPV3INFOHEADER) picasso__parse_v3_fields(&bmp);
    if (bmp.type >= BITMAPV4HEADER) picasso__parse_v4_fields(&bmp);
    if (bmp.type >= BITMAPV5HEADER) picasso__parse_v5_fields(&bmp);

    TRACE("Header size: %zu (fh) + %d (ih) = %zu",
          sizeof(bmp.image.fh), bmp.type, sizeof(bmp.image.fh) + bmp.type);
    TRACE("Actual header size %zu bytes", read);
    ASSERT(bmp.bytes_pp == 3 || bmp.bytes_pp == 4, "Only support bpp of 3 or 4");

    int row_stride = bmp.width * bmp.bytes_pp;
    int row_size   = ((row_stride + 3) / 4) * 4; // padded row size

    TRACE("HEIGHT %d", bmp.height);

    picasso_image *img = picasso_malloc(sizeof(picasso_image));
    *img = (picasso_image){
        .width    = bmp.width,
        .height   = bmp.height,
        .channels = bmp.bytes_pp,
        .pixels   = picasso_malloc(row_stride * bmp.height)
    };

    uint8_t *row_buf = malloc(row_size);

    for (int y = 0; y < bmp.height; ++y) {
        if (fread(row_buf, 1, row_size, fp) != (size_t)row_size) {
            ERROR("Failed to read row %d", y);
            free(row_buf);
            picasso_free(img->pixels);
            picasso_free(img);
            fclose(fp);
            return NULL;
        }

        int dest_y = bmp.is_flipped ? (bmp.height - 1 - y) : y;
        memcpy(img->pixels + dest_y * row_stride, row_buf, row_stride);
    }

    free(row_buf);
    fclose(fp);

    // Swap BGR -> RGB in-place
    for (int y = 0; y < bmp.height; ++y) {
        int row_y = bmp.is_flipped ? y : (bmp.height - 1 - y);
        uint8_t *row = img->pixels + row_y * row_stride;
        for (int x = 0; x < bmp.width; ++x) {
            uint8_t *p = row + x * bmp.bytes_pp;
            uint8_t tmp = p[0]; p[0] = p[2]; p[2] = tmp;
        }
    }

    return img;
}

#include <stdbool.h>
#include <string.h>
#include "picasso.h"
#include "logger.h"
#include "picasso_icc_profiles.h"

typedef enum {
    BITMAPCOREHEADER   = 12,  // OS/2 1.x — rarely used but stb supports it
    BITMAPINFOHEADER   = 40,  // Most common, basic 24/32-bit BMPs
    BITMAPV4HEADER     = 108, // Adds color space and gamma (optional)
    BITMAPV5HEADER     = 124  // Adds ICC profile (optional)
} bmp_header_size;

typedef enum {
    LCS_sRGB = 0x73524742, // Standard sRGB color space ('sRGB' in little-endian) â€” this is the most common.
    LCS_WINDOWS_COLOR_SPACE = 0x57696E20, // Native Windows color space ('Win ' in little-endian)
    PROFILE_EMBEDDED = 0x4D424544, // Custom ICC profile embedded in file ('DEBM' / 'MBED' in little-endian)
    PROFILE_LINKED = 0x4C494E4B, // ICC profile is in an external file ('LINK')
} bmp_cs_type;

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
            break;
    }
}
void _print_header_type(bmp_header_size type)
{
    switch(type) {
        case BITMAPCOREHEADER:
            TRACE("bitmapcoreheader");
            break;
        case BITMAPINFOHEADER:
            TRACE("bitmapinfoheader");
            break;
        case BITMAPV4HEADER:
            TRACE("bitmapv4header");
            break;
        case BITMAPV5HEADER:
            TRACE("bitmapv5header");
            break;
        default:
            TRACE("Not supported");
            break;
    }
}

picasso_image *bmp_load_from_file(const char *filename)
{
    BMP bmp = {0};
    size_t fh_size = sizeof(bmp.fh);
    TRACE("bmp.fh size %zu", fh_size);

    FILE *fp = fopen(filename, "rb");
    size_t read = fread(&bmp.fh,1, sizeof(bmp.fh), fp);
    TRACE("read so far %zu bytes", read);

    TRACE("file type %04x", bmp.fh.file_type);
    TRACE("file size %i", bmp.fh.file_size);
    TRACE("reserved %08x", bmp.fh.reserved1+bmp.fh.reserved2);
    TRACE("file type %i", bmp.fh.offset_data);

    /* So far so good, 4D42 is BMP, fileside is next, two blanks and offset to data*/
    read += fread(&bmp.ih.size,1, sizeof(uint32_t), fp);

    TRACE("read so far %zu bytes", read);
    TRACE("size is %i", bmp.ih.size);
    _print_header_type(bmp.ih.size);
    size_t left_to_read = bmp.fh.offset_data - read;
    TRACE("read %u - %zu = %zu more bytes", bmp.fh.offset_data, read,
            left_to_read);
    read += fread(((uint8_t*)&bmp.ih) + 4, 1, left_to_read, fp);

    TRACE("width              = %d", bmp.ih.width);
    TRACE("height             = %d", bmp.ih.height);
    TRACE("bpp                = %d", bmp.ih.bit_count);
    TRACE("compression        = %u", bmp.ih.compression);
    TRACE("size_image         = %u", bmp.ih.size_image);
    TRACE("x_pixels_per_meter = %d", bmp.ih.x_pixels_per_meter);
    TRACE("y_pixels_per_meter = %d", bmp.ih.y_pixels_per_meter);
    TRACE("colors_used        = %u", bmp.ih.colors_used);
    TRACE("colors_important   = %u", bmp.ih.colors_important);

    // V4 color masks
    TRACE("red_mask           = 0x%08X", bmp.ih.red_mask);
    TRACE("green_mask         = 0x%08X", bmp.ih.green_mask);
    TRACE("blue_mask          = 0x%08X", bmp.ih.blue_mask);
    TRACE("alpha_mask         = 0x%08X", bmp.ih.alpha_mask);

    // Color space type
    TRACE("cs_type:  %s ",_print_cs_type(bmp.ih.cs_type));

    // V5 additions
    TRACE("intent             = %u", bmp.ih.intent);
    TRACE("profile_data       = %u", bmp.ih.profile_data);
    TRACE("profile_size       = %u", bmp.ih.profile_size);
    TRACE("reserved           = %u", bmp.ih.reserved);


    TRACE("read so far %zu bytes", read);

    int bpp = bmp.ih.bit_count / 8; // bytes_per_pixel
    int width = bmp.ih.width, height = bmp.ih.height;

    ASSERT(bpp == 3 || bpp == 4, "Only supprt bpp of 24 or 32");
    int image_size = bpp*
    picasso_image *image = picasso_malloc();

    fclose(fp);
    return NULL;
}

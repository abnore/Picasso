#include "picasso.h"
#include "logger.h"

#include <errno.h>

#define return_defer(value) do { result = (value); goto defer; } while(0)

BMP *picasso_load_bmp(const char *filename)
{
        BMP *image = malloc(sizeof(BMP));
        if (!image) {
            fprintf(stderr, "Failed to allocate BMP struct\n");
            return NULL;
        }
        memset(image, 0, sizeof(BMP));
        BMP_file_header *fileHeader = &image->fh;
        BMP_info_header *infoHeader = &image->ih;

        FILE *file = fopen(filename, "rb");
        if (!file) {
                perror("Unable to open file");
                return NULL;
        }

        // Read file header
        fread(fileHeader, sizeof(BMP_file_header), 1, file);
        if (fileHeader->file_type != 0x4D42) {
                printf("Not a valid BMP file\n");
                fclose(file);
                return NULL;
        }

        // Read info header
        fread(infoHeader, sizeof(BMP_info_header), 1, file);

        // Move file pointer to the beginning of bitmap data
        fseek(file, fileHeader->offset_data, SEEK_SET);

        // Allocate memory for the bitmap data
        image->image_data = (uint8_t *)malloc(infoHeader->size_image);
        if (!image->image_data) {
                printf("Memory allocation failed\n");
                fclose(file);
                return NULL;
        }

        // Read the bitmap data
        fread(image->image_data, infoHeader->size_image, 1, file);

        fclose(file);

        // Swap color channels since mac uses BGRA, not RGBA
        for (int i = 0; i < infoHeader->width * infoHeader->height; ++i)
        {
                uint8_t temp                     = image->image_data[i * 4 + 0];  // Blue
                image->image_data[i * 4 + 0]     = image->image_data[i * 4 + 2];  // Swap Red and Blue
                image->image_data[i * 4 + 2]     = temp;                          // Assign Red to the original Blue
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
	int result = 0;
	FILE *f = NULL;

	{
		f = fopen(file_path, "wb");
		if (f == NULL) return_defer(errno);

		fprintf(f, "P6\n%zu %zu 255\n", image->width, image->height);
		if (ferror(f)) return_defer(errno);

		for (size_t i = 0; i < image->width*image->height; i++) {
			// 0xAABBGGRR - skipping alpha
			uint32_t pixel = image->pixels[i];
			uint8_t bytes[3] = {
				(pixel>>(8*0)) & 0xFF, // Red
				(pixel>>(8*1)) & 0xFF, // Green
				(pixel>>(8*2)) & 0xFF  // Blue
			};
			fwrite(bytes, sizeof(bytes), 1, f);
			if (ferror(f)) return_defer(errno);
		}
	}

defer:
	if (f) fclose(f);
	return result;
}


void picasso_fill_canvas(color *pixels, size_t width, size_t height, color c)
{
    DEBUG("color is %08x", color_to_u32(c));
    for(int i = 0; i < width*height; ++i){
        pixels[i] = c;
    }
}

static inline uint32_t blend_pixel(uint32_t dst, uint32_t src) {
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

void canopy_raster_bitmap_ex(void* src_buf,
                             int src_w,  int src_h,
                             int dest_x, int dest_y,
                             int dest_w, int dest_h,
                             bool scale,
                             bool blend,
                             bool bilinear)
{

}


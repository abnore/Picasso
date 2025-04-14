#include "picasso.h"
#include "logger.h"
#include "canopy.h"

#include <string.h>

#define WIDTH 800
#define HEIGHT 600

enum {
    TTF     = 0x00010000,
    CFF_OTF = 0x4F54544F, // 'OTTO' big endian
};

typedef struct {
    uint16_t segCount;
    uint16_t* endCode;
    uint16_t* startCode;
    int16_t*  idDelta;
    uint16_t* idRangeOffset;
    uint8_t* glyphIdArrayBase;
} ttf_cmap_format4;

static inline uint16_t read_u16_be(uint8_t *p) {
    return (p[0] << 8) | p[1];
}

void picasso_advance(picasso_reader* r, size_t bytes) {
    r->ptr += bytes;
}

void picasso_seek(picasso_reader* r, size_t offset) {
    r->ptr = r->fp + offset;
}

int ttf_lookup_glyph_index(ttf_cmap_format4* cmap, uint16_t codepoint) {
    for (int i = 0; i < cmap->segCount; ++i) {
        if (codepoint >= cmap->startCode[i] && codepoint <= cmap->endCode[i]) {
            if (cmap->idRangeOffset[i] == 0) {
                return (codepoint + cmap->idDelta[i]) % 65536;
            } else {
                int offset_pos = (cmap->idRangeOffset[i] / 2) + (codepoint - cmap->startCode[i]) - (cmap->segCount - i);
                uint8_t* glyph_index_ptr = cmap->glyphIdArrayBase + offset_pos * 2;
                uint16_t glyph_index = read_u16_be(glyph_index_ptr);
                if (glyph_index == 0) return 0;
                return (glyph_index + cmap->idDelta[i]) % 65536;
            }
        }
    }
    return 0;
}

void parse_and_draw_glyph_outline(picasso_backbuffer *bf, picasso_reader *r, int16_t numberOfContours) {
    if (numberOfContours <= 0) {
        INFO("Glyph has no contours");
        return;
    }

    uint16_t endPtsOfContours[numberOfContours];
    for (int i = 0; i < numberOfContours; ++i)
        endPtsOfContours[i] = picasso_read_u16_be(r);

    uint16_t instructionLength = picasso_read_u16_be(r);
    picasso_advance(r, instructionLength);

    uint16_t numPoints = endPtsOfContours[numberOfContours - 1] + 1;

    uint8_t flags[numPoints];
    int flag_index = 0;
    while (flag_index < numPoints) {
        uint8_t flag = picasso_read_u8(r);
        flags[flag_index++] = flag;
        if (flag & 0x08) {
            uint8_t repeatCount = picasso_read_u8(r);
            for (int j = 0; j < repeatCount; ++j)
                flags[flag_index++] = flag;
        }
    }

    int16_t x_coords[numPoints], y_coords[numPoints];
    int16_t x = 0, y = 0;

    for (int i = 0; i < numPoints; ++i) {
        if (flags[i] & 0x02) {
            uint8_t dx = picasso_read_u8(r);
            x += (flags[i] & 0x10) ? dx : -dx;
        } else if (!(flags[i] & 0x10)) {
            x += (int16_t)picasso_read_u16_be(r);
        }
        x_coords[i] = x;
    }

    for (int i = 0; i < numPoints; ++i) {
        if (flags[i] & 0x04) {
            uint8_t dy = picasso_read_u8(r);
            y += (flags[i] & 0x20) ? dy : -dy;
        } else if (!(flags[i] & 0x20)) {
            y += (int16_t)picasso_read_u16_be(r);
        }
        y_coords[i] = y;
    }

    float scale = 4.0f;        // zoom factor
    int x_offset = 300;        // move right
    int y_offset = 200;        // move up

    for (int i = 0; i < numPoints; ++i) {
        int px = (int)(x_coords[i] * scale / 16.0f) + x_offset;
        int py = HEIGHT - ((int)(y_coords[i] * scale / 16.0f) + y_offset);
        picasso_fill_circle(bf, px, py, 2, RED);
    }
}

int main(void) {
    init_log(LOG_DEFAULT);
    const char *path = "Alegreya,Libre_Baskerville/Libre_Baskerville/LibreBaskerville-Regular.ttf";
    picasso_reader *r = picasso_read_entire_file(path);

    picasso_read_u32_be(r);
    uint16_t num_tables = picasso_read_u16_be(r);
    picasso_advance(r, 6);

    uint32_t cmap_offset = 0, head_offset = 0, loca_offset = 0, glyf_offset = 0;
    for (uint16_t i = 0; i < num_tables; ++i) {
        char tag[5] = {0};
        for (int j = 0; j < 4; ++j) tag[j] = picasso_read_u8(r);
        picasso_advance(r, 4);
        uint32_t offset = picasso_read_u32_be(r);
        picasso_advance(r, 4);

        if (strcmp(tag, "cmap") == 0) cmap_offset = offset;
        else if (strcmp(tag, "head") == 0) head_offset = offset;
        else if (strcmp(tag, "loca") == 0) loca_offset = offset;
        else if (strcmp(tag, "glyf") == 0) glyf_offset = offset;
    }

    picasso_seek(r, cmap_offset);
    picasso_advance(r, 2);
    uint16_t cmap_num_tables = picasso_read_u16_be(r);

    uint32_t format4_offset = 0;
    for (int i = 0; i < cmap_num_tables; ++i) {
        uint16_t platform_id = picasso_read_u16_be(r);
        uint16_t encoding_id = picasso_read_u16_be(r);
        uint32_t subtable_offset = picasso_read_u32_be(r);
        if (platform_id == 3 && encoding_id == 1)
            format4_offset = cmap_offset + subtable_offset;
    }

    picasso_seek(r, format4_offset);
    picasso_advance(r, 6);
    uint16_t segCount = picasso_read_u16_be(r) / 2;
    picasso_advance(r, 6);

    uint16_t endCode[segCount], startCode[segCount], idRangeOffset[segCount];
    int16_t idDelta[segCount];
    for (int i = 0; i < segCount; ++i) endCode[i] = picasso_read_u16_be(r);
    picasso_advance(r, 2);
    for (int i = 0; i < segCount; ++i) startCode[i] = picasso_read_u16_be(r);
    for (int i = 0; i < segCount; ++i) idDelta[i] = (int16_t)picasso_read_u16_be(r);
    for (int i = 0; i < segCount; ++i) idRangeOffset[i] = picasso_read_u16_be(r);

    ttf_cmap_format4 cmap4 = {
        .segCount = segCount,
        .endCode = endCode,
        .startCode = startCode,
        .idDelta = idDelta,
        .idRangeOffset = idRangeOffset,
        .glyphIdArrayBase = r->ptr
    };

    uint16_t glyph_index = ttf_lookup_glyph_index(&cmap4, 'P');

    picasso_seek(r, head_offset + 50);
    uint16_t indexToLocFormat = picasso_read_u16_be(r);

    uint32_t glyph_offset;
    if (indexToLocFormat == 0) {
        picasso_seek(r, loca_offset + 2 * glyph_index);
        glyph_offset = picasso_read_u16_be(r) * 2;
    } else {
        picasso_seek(r, loca_offset + 4 * glyph_index);
        glyph_offset = picasso_read_u32_be(r);
    }

    picasso_seek(r, glyf_offset + glyph_offset);
    int16_t numberOfContours = (int16_t)picasso_read_u16_be(r);
    picasso_advance(r, 8);

    canopy_init_timer();
    canopy_window *w = canopy_create_window("glyph", WIDTH, HEIGHT, CANOPY_WINDOW_STYLE_DEFAULT);
    picasso_backbuffer *bf = picasso_create_backbuffer(WIDTH, HEIGHT);

    while (!canopy_window_should_close(w)) {
        if (canopy_should_render_frame()) {
            picasso_clear_backbuffer(bf);
            picasso_seek(r, glyf_offset + glyph_offset + 10);
            parse_and_draw_glyph_outline(bf, r, numberOfContours);
            canopy_swap_backbuffer(w, (framebuffer *)bf);
            canopy_present_buffer(w);
        }
    }

    picasso_reader_free(r);
    shutdown_log();
    return 0;
}

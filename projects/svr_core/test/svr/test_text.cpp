#include <stdio.h>
#include <cstdint>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H

struct glyph_info
{
    int x0;
    int y0;
    int x1;
    int y1;

    int bearing_x;
    int bearing_y;

    int advance_x;
    int advance_y;
};

int main(int argc, char** argv)
{
    FT_Library ft;
    FT_Face face;

    FT_Init_FreeType(&ft);
    FT_Library_SetLcdFilter(ft, FT_LCD_FILTER_LIGHT);

    FT_New_Face(ft, "C:\\Windows\\Fonts\\arial.ttf", 0, &face);
    FT_Set_Pixel_Sizes(face, 0, 96);

    // The maximum allowed width for a texture.
    // A texture is allowed to grow up to this size.
    constexpr auto MAX_WIDTH = 4096;

    // How much to grow a texture in width every time a greater
    // width than the current width is encountered.
    constexpr auto WIDTH_GROW_SIZE = 128;

    // Padding to apply between the glyphs.
    constexpr auto PADDING = 0;

    // The maximum height of a single character in this font face.
    auto max_height = (face->size->metrics.height >> 6) + PADDING;

    auto pen_x = 0;
    auto pen_y = 0;

    auto tex_width = WIDTH_GROW_SIZE;
    auto tex_height = max_height;

    auto stride = 0;
    auto render_mode = FT_RENDER_MODE_LCD;

    switch (render_mode)
    {
        case FT_RENDER_MODE_NORMAL:
        {
            stride = 1;
            break;
        }

        case FT_RENDER_MODE_LCD:
        {
            stride = 3;
            break;
        }
    }

    // First pass used to calculate the correct size needed for the atlas.

    std::vector<glyph_info> glyphs;
    glyphs.reserve(0x3A - 0x30);

    for (size_t i = 0x30; i < 0x3A; ++i)
    {
        // Render the glyph to retrieve the correct sizes.

        FT_Load_Char(face, i, FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT);
        FT_Render_Glyph(face->glyph, render_mode);

        auto bmp = &face->glyph->bitmap;

        // The actual width in pixels.
        auto width = bmp->width / stride;

        // The actual height in pixels.
        auto height = bmp->rows;

        // Grow the texture in width until we reach the next resize point.
        // The texture will only be allowed to grow in height when
        // the maximum width has been reached.
        if (pen_x + width >= tex_width)
        {
            if (pen_x >= MAX_WIDTH)
            {
                // Restart on a new row.

                pen_x = 0;
                tex_height += max_height;
            }

            else
            {
                tex_width += WIDTH_GROW_SIZE;
            }
        }

        pen_x += width + PADDING;
    }

    auto pixels = (unsigned char*)malloc(tex_width * tex_height * stride);

    pen_x = 0;
    pen_y = 0;

    // Second pass used to lay out the glyphs in the calculated rectangle.

    for (size_t i = 0x30; i < 0x3A; ++i)
    {
        FT_Load_Char(face, i, FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT);
        FT_Render_Glyph(face->glyph, render_mode);

        auto bmp = &face->glyph->bitmap;

        // The actual width in pixels.
        auto width = bmp->width / stride;

        // The actual height in pixels.
        auto height = bmp->rows;

        if (pen_x + width >= tex_width)
        {
            // Continue on the second row.

            pen_x = 0;
            pen_y += max_height;
        }

        for (int row = 0; row < bmp->rows; ++row)
        {
            for (int col = 0; col < width; ++col)
            {
                auto get_bmp_comp = [&](size_t comp)
                {
                    return bmp->buffer[col * stride + row * bmp->pitch + comp];
                };

                auto set_result_comp = [&](size_t comp, unsigned char value)
                {
                    auto x = pen_x + col;
                    auto y = pen_y + row;

                    pixels[(y * tex_width + x) * stride + comp] = value;
                };

                for (size_t i = 0; i < stride; i++)
                {
                    set_result_comp(i, get_bmp_comp(i));
                }
            }
        }

        pen_x += width + PADDING;

        glyph_info glyph;

        // Save the metrics for this glyph so we can map it out
        // later when constructing text.

        glyph.x0 = pen_x;
        glyph.y0 = pen_y;
        glyph.x1 = pen_x + width;
        glyph.y1 = pen_y + height;

        glyph.bearing_x = face->glyph->bitmap_left;
        glyph.bearing_y = face->glyph->bitmap_top;
        glyph.advance_x = face->glyph->advance.x >> 6;
        glyph.advance_y = face->glyph->advance.y >> 6;

        glyphs.push_back(glyph);
    }

    FT_Done_FreeType(ft);

    stbi_write_png("font_output.png", tex_width, tex_height, stride, pixels, tex_width * stride);

    free(pixels);

    return 0;
}

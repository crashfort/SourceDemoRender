#include "proc_priv.h"

bool ProcState::velo_init()
{
    return true;
}

void ProcState::velo_free_static()
{
}

void ProcState::velo_free_dynamic()
{
    svr_maybe_release(&velo_font_face);
}

// Try to find the font in the system.
bool ProcState::velo_create_font_face()
{
    bool ret = false;

    HRESULT hr;
    IDWriteFontCollection* coll = NULL;
    IDWriteFontFamily* font_fam = NULL;
    IDWriteFont* font = NULL;

    wchar stupid_buf[128];
    svr_to_utf16(movie_profile.velo_font, strlen(movie_profile.velo_font), stupid_buf, SVR_ARRAY_SIZE(stupid_buf));

    vid_dwrite_factory->GetSystemFontCollection(&coll, FALSE);

    UINT font_index;
    BOOL font_exists;
    coll->FindFamilyName(stupid_buf, &font_index, &font_exists);

    if (!font_exists)
    {
        game_log("ERROR: The specified velo font %s is not installed in the system\n", movie_profile.velo_font);
        goto rfail;
    }

    hr = coll->GetFontFamily(font_index, &font_fam);

    if (FAILED(hr))
    {
        game_log("ERROR: Could not get the font family of font %s\n", movie_profile.velo_font);
        goto rfail;
    }

    hr = font_fam->GetFirstMatchingFont(movie_profile.velo_font_weight, DWRITE_FONT_STRETCH_NORMAL, movie_profile.velo_font_style, &font);

    if (FAILED(hr))
    {
        game_log("ERROR: Could not find the combination of font parameters (weight, stretch, style) in the font %s\n", movie_profile.velo_font);
        goto rfail;
    }

    hr = font->CreateFontFace(&velo_font_face);

    if (FAILED(hr))
    {
        game_log("ERROR: Could not create a font face of font %s\n", movie_profile.velo_font);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    svr_maybe_release(&coll);
    svr_maybe_release(&font_fam);
    svr_maybe_release(&font);

    return ret;
}

void ProcState::velo_setup_tab_metrix()
{
    // Emulation of tabular font feature where every number is monospaced.
    // For the full feature, the font itself also changes its shaping for the glyphs to be wider, but that is not important.
    // This also prevents the text from jittering when it changes during the centering logic. Typically caused by the 1 character sometimes being thinner than other characters.

    DWRITE_FONT_METRICS font_metrix;
    velo_font_face->GetMetrics(&font_metrix);

    UINT32 tab_cp = L'0';

    UINT16 tab_idx;
    velo_font_face->GetGlyphIndicesW(&tab_cp, 1, &tab_idx);
    velo_font_face->GetDesignGlyphMetrics(&tab_idx, 1, &velo_tab_metrix, FALSE);

    float scale = (float)movie_profile.velo_font_size / (float)font_metrix.designUnitsPerEm;

    float t = velo_tab_metrix.topSideBearing * scale;
    float b = velo_tab_metrix.bottomSideBearing * scale;
    float aw = velo_tab_metrix.advanceWidth * scale;
    float ah = velo_tab_metrix.advanceHeight * scale;

    velo_tab_height = ah - b - t;
    velo_tab_advance_x = aw;
}

void ProcState::velo_setup_glyph_idxs()
{
    UINT32 cps[] = { L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9' };
    velo_font_face->GetGlyphIndicesW(cps, SVR_ARRAY_SIZE(cps), velo_number_glyph_idxs);
}

bool ProcState::velo_start()
{
    bool ret = false;
    HRESULT hr;

    if (!velo_create_font_face())
    {
        goto rfail;
    }

    velo_draw_pos = velo_get_pos();

    velo_setup_tab_metrix();
    velo_setup_glyph_idxs();

    ret = true;
    goto rexit;

rfail:

rexit:

    return ret;
}

void ProcState::velo_end()
{
}

void ProcState::velo_draw()
{
    float length = velo_get_length();
    s32 speed = (s32)(sqrtf(length) + 0.5f);

    char buf[128];
    s32 text_length = SVR_SNPRINTF(buf, "%d", speed);

    UINT16* idxs = SVR_ALLOCA_NUM(UINT16, text_length);
    float* advances = SVR_ALLOCA_NUM(float, text_length);

    // Map the glyph indexes.
    for (s32 i = 0; i < text_length; i++)
    {
        s32 idx = buf[i] - '0';
        idxs[i] = velo_number_glyph_idxs[idx];
    }

    // Emulation of tabular font feature where every number is monospaced.
    // For the full feature, the font itself also changes its shaping for the glyphs to be wider, but that is not important.
    // This also prevents the text from jittering when it changes during the centering logic. Typically caused by the 1 character sometimes being thinner than other characters.

    for (s32 i = 0; i < text_length; i++)
    {
        advances[i] = velo_tab_advance_x;
    }

    // Base horizontal positioning from the center.
    // Vertical positioning is done from the baseline.

    float w = text_length * velo_tab_advance_x;
    float h = velo_tab_height;

    s32 real_w = (s32)ceilf(w);
    s32 real_h = (s32)ceilf(h);
    s32 shift_x = real_w / 2;

    SvrVec2I pos = velo_draw_pos;

    if (movie_profile.velo_anchor == VELO_ANCHOR_CENTER)
    {
        pos.x -= shift_x;
    }

    if (movie_profile.velo_anchor == VELO_ANCHOR_RIGHT)
    {
        pos.x -= real_w;
    }

    vid_d2d1_context->BeginDraw();
    vid_d2d1_context->SetTarget(encoder_d2d1_share_tex);

    if (movie_profile.velo_font_border_size > 0)
    {
        ID2D1PathGeometry* geom;
        vid_d2d1_factory->CreatePathGeometry(&geom);

        ID2D1GeometrySink* sink;
        geom->Open(&sink);

        velo_font_face->GetGlyphRunOutline(movie_profile.velo_font_size, idxs, advances, NULL, text_length, FALSE, FALSE, sink);

        sink->Close();

        vid_d2d1_context->SetTransform(D2D1::Matrix3x2F::Translation(pos.x, pos.y));

        // Draw the fill.
        vid_d2d1_solid_brush->SetColor(vid_fill_d2d1_color(movie_profile.velo_font_color));
        vid_d2d1_context->FillGeometry(geom, vid_d2d1_solid_brush);

        // Draw the border.
        vid_d2d1_solid_brush->SetColor(vid_fill_d2d1_color(movie_profile.velo_font_border_color));
        vid_d2d1_context->DrawGeometry(geom, vid_d2d1_solid_brush, movie_profile.velo_font_border_size);

        geom->Release();
        sink->Release();
    }

    // Use more specialized path with no border.
    else
    {
        DWRITE_GLYPH_RUN run = {};
        run.fontFace = velo_font_face;
        run.fontEmSize = movie_profile.velo_font_size;
        run.glyphCount = text_length;
        run.glyphIndices = idxs;
        run.glyphAdvances = advances;

        vid_d2d1_solid_brush->SetColor(vid_fill_d2d1_color(movie_profile.velo_font_color));
        vid_d2d1_context->DrawGlyphRun(vid_fill_d2d1_pt(pos), &run, vid_d2d1_solid_brush);
    }

    vid_d2d1_context->EndDraw();
    vid_d2d1_context->SetTarget(NULL);
}

void ProcState::velo_give(float* source)
{
    memcpy(velo_vector, source, sizeof(float) * 3);
}

// Percentage alignments based from the center of the screen.
SvrVec2I ProcState::velo_get_pos()
{
    s32 scr_pos_x = movie_width / 2;
    s32 scr_pos_y = movie_height / 2;

    scr_pos_x += (movie_profile.velo_align.x / 200.0f) * movie_width;
    scr_pos_y += (movie_profile.velo_align.y / 200.0f) * movie_height;

    return SvrVec2I { scr_pos_x, scr_pos_y };
}

float ProcState::velo_get_length()
{
    float length = 0.0f;

    switch (movie_profile.velo_length)
    {
        case VELO_LENGTH_XY:
        {
            length = velo_vector[0] * velo_vector[0] + velo_vector[1] * velo_vector[1];
            break;
        }

        case VELO_LENGTH_XYZ:
        {
            length = velo_vector[0] * velo_vector[0] + velo_vector[1] * velo_vector[1] + velo_vector[2] * velo_vector[2];
            break;
        }
        
        case VELO_LENGTH_Z:
        {
            length = velo_vector[2] * velo_vector[2];
            break;
        }
    }

    return length;
}

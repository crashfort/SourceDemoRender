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

    for (s32 i = 0; i < 10; i++)
    {
        svr_maybe_release(&velo_number_paths[i]);
    }
}

void ProcState::velo_setup_tab_metrix()
{
    // Emulation of tabular font feature where every number is monospaced.
    // For the full feature, the font itself also changes its shaping for the glyphs to be wider, but that is not important.
    // This also prevents the text from jittering when it changes during the centering logic. Typically caused by the 1 character sometimes being thinner than other characters.

    DWRITE_FONT_METRICS font_metrix;
    velo_font_face->GetMetrics(&font_metrix);

    UINT32 tab_cp = L'0';
    DWRITE_GLYPH_METRICS tab_metrix;
    UINT16 tab_idx;

    velo_font_face->GetGlyphIndicesW(&tab_cp, 1, &tab_idx);
    velo_font_face->GetDesignGlyphMetrics(&tab_idx, 1, &tab_metrix, FALSE);

    float scale = (float)movie_profile.velo_font_size / (float)font_metrix.designUnitsPerEm;

    float t = tab_metrix.topSideBearing * scale;
    float b = tab_metrix.bottomSideBearing * scale;
    float aw = tab_metrix.advanceWidth * scale;
    float ah = tab_metrix.advanceHeight * scale;

    velo_tab_height = ah - b - t;
    velo_tab_advance_x = aw;
}

void ProcState::velo_setup_glyph_idxs()
{
    const UINT32 cps[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    velo_font_face->GetGlyphIndicesW(cps, SVR_ARRAY_SIZE(cps), velo_number_glyph_idxs);
}

bool ProcState::velo_start()
{
    bool ret = false;
    HRESULT hr;

    velo_font_face = vid_create_font_face(movie_profile.velo_font, movie_profile.velo_font_weight, movie_profile.velo_font_style);

    if (velo_font_face == NULL)
    {
        goto rfail;
    }

    velo_draw_pos = velo_get_pos();

    velo_setup_tab_metrix();
    velo_setup_glyph_idxs();

    // Cache all number geometries.
    if (movie_profile.velo_font_border_size > 0)
    {
        for (s32 i = 0; i < 10; i++)
        {
            ID2D1PathGeometry* geom;
            vid_d2d1_factory->CreatePathGeometry(&geom);

            ID2D1GeometrySink* sink;
            geom->Open(&sink);

            UINT16 glyph_idx = velo_number_glyph_idxs[i];
            velo_font_face->GetGlyphRunOutline(movie_profile.velo_font_size, &glyph_idx, NULL, NULL, 1, FALSE, FALSE, sink);

            sink->Close();

            svr_release(sink);
            sink = NULL;

            velo_number_paths[i] = geom;
        }
    }

    velo_latest_vectors = {};
    velo_prev_vectors = {};
    memset(velo_vectors, 0, sizeof(velo_vectors));

    velo_lagcomp_write = 0;
    velo_lagcomp_read = 0;

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
    SvrVec3 front = velo_latest_vectors;

    if (movie_lagcomp_interp > 0.0f)
    {
        front = velo_prev_vectors;

        velo_vectors[velo_lagcomp_write & PROC_LAGCOMP_MASK] = velo_latest_vectors;
        velo_lagcomp_write++;

        if (movie_lagcomp_queued_time >= movie_lagcomp_interp)
        {
            front = velo_vectors[velo_lagcomp_read & PROC_LAGCOMP_MASK];
            velo_lagcomp_read++;

            velo_prev_vectors = front;
        }
    }

    float length = velo_get_length(front);
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

    // Vertical positioning is done from the baseline.

    float w = text_length * velo_tab_advance_x;
    float h = velo_tab_height;

    SvrVec2 pos = velo_draw_pos;

    if (movie_profile.velo_anchor == PROC_VELO_ANCHOR_CENTER)
    {
        pos.x = velo_draw_pos.x - (w / 2.0f);
    }

    if (movie_profile.velo_anchor == PROC_VELO_ANCHOR_RIGHT)
    {
        pos.x -= w;
    }

    vid_d2d1_context->BeginDraw();
    vid_d2d1_context->SetTarget(encoder_d2d1_share_tex);

    if (movie_profile.velo_font_border_size > 0)
    {
        float advance_x = 0.0f;

        for (s32 i = 0; i < text_length; i++)
        {
            s32 idx = buf[i] - '0';
            ID2D1PathGeometry* geom = velo_number_paths[idx];

            vid_d2d1_context->SetTransform(D2D1::Matrix3x2F::Translation(pos.x + advance_x, pos.y));

            // Draw the fill.
            vid_d2d1_solid_brush->SetColor(vid_convert(movie_profile.velo_font_color));
            vid_d2d1_context->FillGeometry(geom, vid_d2d1_solid_brush);

            // Draw the border.
            vid_d2d1_solid_brush->SetColor(vid_convert(movie_profile.velo_font_border_color));
            vid_d2d1_context->DrawGeometry(geom, vid_d2d1_solid_brush, movie_profile.velo_font_border_size);

            advance_x += advances[i];
        }

        vid_d2d1_context->SetTransform(D2D1::Matrix3x2F::Identity());
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

        vid_d2d1_solid_brush->SetColor(vid_convert(movie_profile.velo_font_color));
        vid_d2d1_context->DrawGlyphRun(vid_convert(pos), &run, vid_d2d1_solid_brush);

        // vid_d2d1_solid_brush->SetColor(D2D1::ColorF(1.0f, 0.0f, 0.0f, 0.2f));
        // vid_d2d1_context->FillRectangle(D2D1::RectF(pos.x, pos.y - h, pos.x + w, pos.y), vid_d2d1_solid_brush);
    }

    vid_d2d1_context->EndDraw();
    vid_d2d1_context->SetTarget(NULL);
}

void ProcState::velo_give(SvrVec3 source)
{
    velo_latest_vectors = source;
}

// Percentage alignments based from the center of the screen.
SvrVec2 ProcState::velo_get_pos()
{
    float scr_pos_x = movie_width / 2.0f;
    float scr_pos_y = movie_height / 2.0f;

    scr_pos_x += (movie_profile.velo_align.x / 200.0f) * movie_width;
    scr_pos_y += (movie_profile.velo_align.y / 200.0f) * movie_height;

    return SvrVec2 { scr_pos_x, scr_pos_y };
}

float ProcState::velo_get_length(SvrVec3 vec)
{
    float length = 0.0f;

    switch (movie_profile.velo_length)
    {
        case PROC_VELO_LENGTH_XY:
        {
            length = (vec.x * vec.x) + (vec.y * vec.y);
            break;
        }

        case PROC_VELO_LENGTH_XYZ:
        {
            length = (vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z);
            break;
        }
        
        case PROC_VELO_LENGTH_Z:
        {
            length = (vec.z * vec.z);
            break;
        }
    }

    return length;
}

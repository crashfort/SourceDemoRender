#include "proc_priv.h"
#include "proc_state.h"

// Should probably also allow every button bit to be input as an image for customization options.
// Every image would then just be drawn on top of each other.

bool ProcState::input_init()
{
    bool ret = false;

    char path[256];
    SVR_SNPRINTF(path, "%s\\data\\input\\arrow.png", svr_resource_path);

    s32 size_x;
    s32 size_y;
    stbi_uc* image_data = stbi_load(path, &size_x, &size_y, NULL, 4);

    if (image_data == NULL)
    {
        svr_log("ERROR: Could not load input arrow image %s\n", path);
        goto rfail;
    }

    D2D1_BITMAP_PROPERTIES1 bmp_props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    HRESULT hr = vid_d2d1_context->CreateBitmap(D2D1::SizeU(size_x, size_y), image_data, size_x * 4, bmp_props, &input_dir_arrow);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create input arrow bitmap (%#x)\n", hr);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:
    if (image_data)
    {
        free(image_data);
    }

rexit:
    return ret;
}

void ProcState::input_free_static()
{
    if (input_dir_arrow)
    {
        svr_release(input_dir_arrow);
        input_dir_arrow = NULL;
    }
}

void ProcState::input_free_dynamic()
{
    svr_maybe_release(&input_font_face);
}

bool ProcState::input_start()
{
    bool ret = false;

    IDWriteFontFace* font_face = vid_create_font_face("Segoe UI", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);

    if (font_face == NULL)
    {
        goto rfail;
    }

    font_face->QueryInterface(IID_PPV_ARGS(&input_font_face));

    input_draw_pos = input_get_pos();

    input_scale = movie_profile.input_scale / 100.0f;
    input_row_height = 24.0f * input_scale;
    input_arrow_size = 64.0f * input_scale;
    input_diamond_diameter = 72.0f * input_scale;
    input_extras_radius = 56.0f * input_scale;
    input_font_size = 24.0f * input_scale;

    input_jump_string = input_setup_string("JUMP");
    input_duck_string = input_setup_string("DUCK");

    input_latest_buttons = {};
    input_prev_buttons = {};
    memset(input_buttons, 0, sizeof(input_buttons));

    input_lagcomp_write = 0;
    input_lagcomp_read = 0;

    ret = true;
    goto rexit;

rfail:
rexit:
    svr_maybe_release(&font_face);
    return ret;
}

void ProcState::input_end()
{
}

void ProcState::input_draw()
{
    SvrButtons front = input_latest_buttons;

    if (movie_lagcomp_interp > 0.0f)
    {
        front = input_prev_buttons;

        input_buttons[input_lagcomp_write & PROC_LAGCOMP_MASK] = input_latest_buttons;
        input_lagcomp_write++;

        if (movie_lagcomp_queued_time >= movie_lagcomp_interp)
        {
            front = input_buttons[input_lagcomp_read & PROC_LAGCOMP_MASK];
            input_lagcomp_read++;

            input_prev_buttons = front;
        }
    }

    vid_d2d1_context->BeginDraw();
    vid_d2d1_context->SetTarget(encoder_d2d1_share_tex);

    SvrVec2 mod_pos = input_draw_pos;

    float diamond_radius = input_diamond_diameter / 2.0f;

    mod_pos.x += input_extras_radius;

    SvrVec2 move_forward_pos = { mod_pos.x, mod_pos.y };
    SvrVec2 yaw_left_pos = { mod_pos.x - input_extras_radius, mod_pos.y + diamond_radius };
    SvrVec2 move_left_pos = { mod_pos.x - diamond_radius, mod_pos.y + diamond_radius };
    SvrVec2 move_right_pos = { mod_pos.x + diamond_radius, mod_pos.y + diamond_radius };
    SvrVec2 yaw_right_pos = { mod_pos.x + input_extras_radius, mod_pos.y + diamond_radius };
    SvrVec2 move_back_pos = { mod_pos.x, mod_pos.y + diamond_radius * 2.0f };

    // Positioning stuff so we can center this thing.
    float box_left = yaw_left_pos.x;
    float box_top = move_forward_pos.y;
    float box_right = yaw_right_pos.x + input_arrow_size;
    float box_bottom = move_back_pos.y + input_arrow_size;
    float box_width = box_right - box_left;
    float box_height = box_bottom - box_top;

    // Easiest to just create this here and pass it on so we don't have to adjust every arrow.
    // Base the profile position around the diamond center.
    SvrVec2 box_offset = {};
    box_offset.x -= box_width / 2.0f;
    box_offset.y -= box_height / 2.0f;

    // vid_d2d1_solid_brush->SetColor(D2D1::ColorF(1.0f, 0.0f, 0.0f, 0.5f));
    // vid_d2d1_context->FillRectangle(D2D1::RectF(box_left + box_offset.x, box_top + box_offset.y, box_right + box_offset.x, box_bottom + box_offset.y), vid_d2d1_solid_brush);
    // vid_d2d1_context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(input_draw_pos.x, input_draw_pos.y), input_extras_radius, input_extras_radius), vid_d2d1_solid_brush);
    // vid_d2d1_context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(input_draw_pos.x, input_draw_pos.y), diamond_radius, diamond_radius), vid_d2d1_solid_brush);

    // Orientation order: left, right, up, down.

    input_draw_one_input_arrow(front.in_yaw_left, { 1.0f, 0.0f, 0.0f, 0.0f }, yaw_left_pos, box_offset);
    input_draw_one_input_arrow(front.in_move_left, { 1.0f, 0.0f, 0.0f, 0.0f }, move_left_pos, box_offset);

    input_draw_one_input_arrow(front.in_yaw_right, { 0.0f, 1.0f, 0.0f, 0.0f }, yaw_right_pos, box_offset);
    input_draw_one_input_arrow(front.in_move_right, { 0.0f, 1.0f, 0.0f, 0.0f }, move_right_pos, box_offset);

    input_draw_one_input_arrow(front.in_forward, { 0.0f, 0.0f, 1.0f, 0.0f }, move_forward_pos, box_offset);
    input_draw_one_input_arrow(front.in_back, { 0.0f, 0.0f, 0.0f, 1.0f }, move_back_pos, box_offset);

    // Text just goes under the diamond.

    mod_pos.x = input_draw_pos.x;
    mod_pos.x -= box_offset.x;
    mod_pos.y += diamond_radius * 4.5f;

    input_draw_one_input_string(front.in_jump, &input_jump_string, mod_pos, box_offset);
    mod_pos.y += input_row_height;
    input_draw_one_input_string(front.in_duck, &input_duck_string, mod_pos, box_offset);

    // vid_d2d1_solid_brush->SetColor(D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f));
    // vid_d2d1_context->DrawLine(D2D1::Point2F(movie_width / 2.0f, 0.0f), D2D1::Point2F(movie_width / 2.0f, movie_height), vid_d2d1_solid_brush);

    vid_d2d1_context->EndDraw();
    vid_d2d1_context->SetTarget(NULL);
}

void ProcState::input_draw_one_input_arrow(bool state, SvrVec4 orientation, SvrVec2 pos, SvrVec2 offset)
{
    float half_arrow_size = input_arrow_size / 2.0f;

    pos.x += offset.x;
    pos.y += offset.y;

    // The source image faces left.
    // Orientation order: left, right, up, down.
    SvrVec4 angles = { 0.0f, 180.0f, 90.0f, -90.0f };

    float rotation = (angles.x * orientation.x) + (angles.y * orientation.y) + (angles.z * orientation.z) + (angles.w * orientation.w);

    D2D1_RECT_F dest_rect = D2D1::RectF(pos.x, pos.y, pos.x + input_arrow_size, pos.y + input_arrow_size);

    if (state)
    {
        vid_d2d1_solid_brush->SetColor(vid_convert(movie_profile.input_active_color));
    }

    else
    {
        vid_d2d1_solid_brush->SetColor(vid_convert(movie_profile.input_inactive_color));
    }

    D2D1_POINT_2F center = D2D1::Point2F(pos.x + half_arrow_size, pos.y + half_arrow_size);
    D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Rotation(rotation, center);

    vid_d2d1_context->SetTransform(transform);
    vid_d2d1_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED); // Must be done for opacity mask.

    vid_d2d1_context->FillOpacityMask(input_dir_arrow, vid_d2d1_solid_brush, &dest_rect);

    vid_d2d1_context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    vid_d2d1_context->SetTransform(D2D1::Matrix3x2F::Identity());
}

void ProcState::input_draw_one_input_string(bool state, ProcInputString* display, SvrVec2 pos, SvrVec2 offset)
{
    pos.x += offset.x;
    pos.y += offset.y;

    DWRITE_GLYPH_RUN run = {};
    run.fontFace = input_font_face;
    run.fontEmSize = input_font_size;
    run.glyphCount = display->length;
    run.glyphIndices = display->glyph_idxs;

    pos.x = pos.x - (display->width / 2.0f);

    if (state)
    {
        vid_d2d1_solid_brush->SetColor(vid_convert(movie_profile.input_active_color));
    }

    else
    {
        vid_d2d1_solid_brush->SetColor(vid_convert(movie_profile.input_inactive_color));
    }

    vid_d2d1_context->DrawGlyphRun(vid_convert(pos), &run, vid_d2d1_solid_brush);
}

void ProcState::input_give(SvrButtons buttons)
{
    input_latest_buttons = buttons;
}

SvrVec2 ProcState::input_get_pos()
{
    float scr_pos_x = movie_width / 2.0f;
    float scr_pos_y = movie_height / 2.0f;

    scr_pos_x += (movie_profile.input_align.x / 200.0f) * movie_width;
    scr_pos_y += (movie_profile.input_align.y / 200.0f) * movie_height;

    return SvrVec2 { scr_pos_x, scr_pos_y };
}

ProcInputString ProcState::input_setup_string(const char* display)
{
    ProcInputString ret = {};
    ret.length = strlen(display);

    assert(ret.length <= SVR_ARRAY_SIZE(ret.glyph_idxs));
    assert(ret.length <= SVR_ARRAY_SIZE(ret.advances));

    UINT32* cps = SVR_ALLOCA_NUM(UINT32, ret.length);
    UINT16* idxs = SVR_ALLOCA_NUM(UINT16, ret.length);
    INT32* fdu_advances = SVR_ALLOCA_NUM(INT32, ret.length);
    INT32* fdu_adjustments = SVR_ALLOCA_NUM(INT32, ret.length);

    for (s32 i = 0; i < ret.length; i++)
    {
        cps[i] = display[i];
    }

    input_font_face->GetGlyphIndicesW(cps, ret.length, idxs);
    input_font_face->GetDesignGlyphAdvances(ret.length, idxs, fdu_advances, FALSE);
    input_font_face->GetKerningPairAdjustments(ret.length, idxs, fdu_adjustments);

    DWRITE_FONT_METRICS font_metrix;
    input_font_face->GetMetrics(&font_metrix);

    float font_scale = input_font_size / (float)font_metrix.designUnitsPerEm;
    float text_width = 0.0f;

    for (s32 i = 0; i < ret.length; i++)
    {
        ret.advances[i] = (fdu_advances[i] + fdu_adjustments[i]) * font_scale;
        ret.width += ret.advances[i];
        ret.glyph_idxs[i] = idxs[i];
    }

    return ret;
}

#pragma once
#include "svr_common.h"

// Proc is the layer below the public API. This is where the magic happens.

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct SvrWaveSample;

bool proc_init(const char* resource_path, ID3D11Device* d3d11_device);
bool proc_start(ID3D11Device* d3d11_device, ID3D11DeviceContext* d3d11_context, const char* dest, const char* profile, ID3D11ShaderResourceView* game_content_srv);
void proc_frame(ID3D11DeviceContext* d3d11_context, ID3D11ShaderResourceView* game_content_srv, ID3D11RenderTargetView* game_content_rtv);
void proc_give_velocity(float* xyz);
bool proc_is_velo_enabled();
bool proc_is_audio_enabled();
void proc_give_audio(SvrWaveSample* samples, s32 num_samples);
void proc_end();
s32 proc_get_game_rate();

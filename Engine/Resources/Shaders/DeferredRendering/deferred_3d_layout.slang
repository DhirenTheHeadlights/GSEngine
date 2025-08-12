#pragma once
import common;

public static const int descriptor_layout_type = 1;
#define __HAS_LAYOUT_MODULE 1

[[vk::binding(0, 0)]] Sampler2D g_albedo;     
[[vk::binding(1, 0)]] Sampler2D g_normal;     
[[vk::binding(2, 0)]] Sampler2D g_depth;      

[[vk::binding(3, 0)]]
cbuffer CameraParams{
    float4x4 inv_pv;
    float3   view_pos;
    float    _pad_cam0;
};

[[vk::binding(4, 0)]] StructuredBuffer<Light> lights_ssbo;
[[vk::binding(5, 0)]] Sampler2D extraTex0;
[[vk::binding(6, 0)]] Sampler2D extraTex1;

[[vk::binding(0, 1)]]
cbuffer DeferredVS_Push
{
    float4x4 push_vs_mat;
};

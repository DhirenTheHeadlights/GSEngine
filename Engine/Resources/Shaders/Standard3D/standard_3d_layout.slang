#pragma once

public static const int descriptor_layout_type = 2;
#define __HAS_LAYOUT_MODULE 1

[[vk::binding(0, 0)]]
cbuffer CameraUBO {
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(2, 1)]] Sampler2D diffuseSampler;
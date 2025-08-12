#pragma once

public static const int descriptor_layout_type = 3;
#define __HAS_LAYOUT_MODULE 1

[[vk::binding(0, 1)]] Sampler2D spriteTexture;
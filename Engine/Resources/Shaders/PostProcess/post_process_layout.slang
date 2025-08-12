#pragma once

public static const int descriptor_layout_type = 4;
#define __HAS_LAYOUT_MODULE 1

[[vk::binding(0, 0)]] Sampler2D inputTex0;
[[vk::binding(1, 0)]] Sampler2D inputTex1;

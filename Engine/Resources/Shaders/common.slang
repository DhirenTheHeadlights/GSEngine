#pragma once
public static const int MAX_LIGHTS = 10;

public enum class LightType
{
    Directional = 0,
    Point       = 1,
    Spot        = 2
};

public struct Light
{
    LightType light_type;
    float3 position;
    float3 direction;
    float3 color;
    float  intensity;
    float  constant;
    float  linear;
    float  quadratic;
    float  cut_off;
    float  outer_cut_off;
    float  ambient_strength;
};

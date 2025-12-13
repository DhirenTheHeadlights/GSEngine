public static const int MAX_LIGHTS = 10;

public enum class LightType {
    Directional = 0,
    Point       = 1,
    Spot        = 2
};

public struct Light {
    public LightType light_type;
    public float3 position;
    public float3 direction;
    public float3 color;
    public float  intensity;
    public float  constant;
    public float  linear;
    public float  quadratic;
    public float  cut_off;
    public float  outer_cut_off;
    public float ambient_strength;
    public int shadow_index;
};

public enum class LayoutKind : int {
    Deferred3D  = 0,
    Forward3D   = 1,
    PostProcess = 2,
    Standard2D  = 3,
    Standard3D  = 4,
};

[__AttributeUsage(_AttributeTargets.Function)]
public struct LayoutAttribute {
    LayoutKind kind;
};

#define LAYOUT_KIND_AND_NAME(K) K, #K

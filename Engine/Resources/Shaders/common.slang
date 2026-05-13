__exported import generated.forward_types;

public static const int MAX_POINT_SHADOW_LIGHTS = 4;

public enum class LayoutKind : int {
    Deferred3D  = 0,
    Forward3D   = 1,
    PostProcess = 2,
    Standard2D  = 3,
    Standard3D  = 4,
    VBDPhysics  = 5,
};

[__AttributeUsage(_AttributeTargets.Function)]
public struct LayoutAttribute {
    LayoutKind kind;
};

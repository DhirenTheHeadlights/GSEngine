[[vk::binding(0, 0)]]
public cbuffer CameraUBO {
    public float4x4 view;
    public float4x4 proj;
};

public [[vk::binding(2, 1)]] Sampler2D diffuseSampler;
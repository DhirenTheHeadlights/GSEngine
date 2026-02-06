[[vk::binding(0, 0)]]
public cbuffer CameraUBO {
    public float4x4 view;
    public float4x4 proj;
};

public [[vk::binding(2, 1)]] Sampler2D diffuseSampler;

[[vk::binding(3, 1)]]
public StructuredBuffer<float4x4> skinMatrices;

public struct InstanceData {
    public float4x4 model_matrix;
    public float4x4 normal_matrix;
    public uint skin_offset;
    public uint joint_count;
    public uint _pad0;
    public uint _pad1;
};

[[vk::binding(4, 1)]]
public StructuredBuffer<InstanceData> instanceData;
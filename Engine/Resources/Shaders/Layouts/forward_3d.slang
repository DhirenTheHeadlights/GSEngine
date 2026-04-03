import common;
import meshlet_common;

public[[vk::binding(0, 0)]]
cbuffer CameraUBO {
    public float4x4 view;
    public float4x4 proj;
    public float4x4 inv_view;
};

public[[vk::binding(1, 0)]] StructuredBuffer<Light> lights_ssbo;

public[[vk::binding(2, 0)]] RaytracingAccelerationStructure tlas;

public static const uint TILE_SIZE = 16;
public static const uint MAX_LIGHTS_PER_TILE = 64;

public[[vk::binding(3, 0)]] StructuredBuffer<uint> light_index_list;
public[[vk::binding(4, 0)]] StructuredBuffer<uint2> tile_light_table;

public[[vk::binding(0, 1)]] StructuredBuffer<Vertex> vertices;
public[[vk::binding(1, 1)]] StructuredBuffer<MeshletDescriptor> meshlets;
public[[vk::binding(2, 1)]] StructuredBuffer<uint> meshletVertexIndices;
public[[vk::binding(3, 1)]] ByteAddressBuffer meshletTriangles;
public[[vk::binding(4, 1)]] StructuredBuffer<MeshletBounds> meshletBounds;
public[[vk::binding(5, 1)]] StructuredBuffer<InstanceData> instanceData;
public[[vk::binding(6, 1)]] Sampler2D diffuseSampler;

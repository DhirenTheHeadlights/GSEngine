import common;

public [[vk::binding(0, 1)]] Texture2D albedo_spec;
public [[vk::binding(1, 1)]] Texture2D diffuse_texture;
public [[vk::binding(2, 1)]] StructuredBuffer<Light> lights;
public [[vk::binding(3, 1)]] SamplerState g_sampler;
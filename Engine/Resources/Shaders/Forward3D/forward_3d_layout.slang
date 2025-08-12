import common;

[[vk::binding(0, 1)]] Texture2D albedo_spec;
[[vk::binding(1, 1)]] Texture2D diffuse_texture;
[[vk::binding(2, 1)]] StructuredBuffer<Light> lights;
[[vk::binding(3, 1)]] SamplerState g_sampler;
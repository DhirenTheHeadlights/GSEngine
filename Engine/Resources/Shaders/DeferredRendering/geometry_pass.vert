#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
} model;

layout(location = 0) out vec3 frag_position;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec2 frag_tex_coord;

void main() {
    vec4 world_position = model.model * vec4(in_position, 1.0);
    frag_position = world_position.xyz;
    frag_normal = normalize(mat3(model.model) * in_normal);
    frag_tex_coord = in_tex_coord;

    gl_Position = camera.proj * camera.view * world_position;
}

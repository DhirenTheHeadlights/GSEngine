#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(constant_id = 99) const int descriptor_layout_type = 1;

layout(location = 0) out vec2 uv;

void main() {
    uv = vec2( (gl_VertexIndex << 1) & 2,
               (gl_VertexIndex     ) & 2 );
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
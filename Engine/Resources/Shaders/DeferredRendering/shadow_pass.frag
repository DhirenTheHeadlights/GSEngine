#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (constant_id = 99 ) const int descriptor_layout_type = 2;

void main() {
    // No output needed; depth values are automatically written to the depth buffer
}

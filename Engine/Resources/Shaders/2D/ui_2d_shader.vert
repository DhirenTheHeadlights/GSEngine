#version 450

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inTexCoord;

layout (push_constant) uniform PushConstants {
    vec2 position;
    vec2 size;
    vec4 color;
    vec4 uv_rect;
} push;

layout (location = 0) out vec2 fragTexCoord;
layout (location = 1) out vec4 fragColor;

void main() {
    vec2 scaledPos = inPosition * push.size + push.position;
    gl_Position = vec4(scaledPos, 0.0, 1.0);
    
    fragTexCoord = push.uv_rect.xy + inTexCoord * push.uv_rect.zw;
    fragColor = push.color;
}

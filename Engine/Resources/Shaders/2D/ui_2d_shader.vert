#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 projection;
uniform mat4 uModel;

out vec2 vTexCoord;

void main() {
    gl_Position = projection * uModel * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}

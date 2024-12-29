#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;  

uniform mat4 uProjection;          
uniform mat4 uModel;                 

out vec2 vTexCoord;

void main() {
    vTexCoord = aUV;
    gl_Position = uProjection * uModel * vec4(aPos, 1.0);
}

#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform bool uUseColor;
uniform vec4 uColor;
uniform sampler2D uTexture;

void main() {
    if(uUseColor)
        FragColor = uColor;
    else
        FragColor = texture(uTexture, TexCoord);
}

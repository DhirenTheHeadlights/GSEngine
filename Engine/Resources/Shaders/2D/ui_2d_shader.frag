#version 330 core
in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform bool uUseColor;
uniform vec4 uColor;

out vec4 FragColor;

void main() {
    if (uUseColor) {
        FragColor = uColor;
    } else {
        float alpha = texture(uTexture, vTexCoord).r;   // Assuming font atlas is single-channel (red)
        FragColor = vec4(uColor.rgb, uColor.a * alpha);
    }
}

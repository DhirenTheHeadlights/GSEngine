#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uMSDF;
uniform vec4 uColor;

uniform float uRange;

float median3(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    // For a multi-channel SDF, each channel encodes distance; we take the median.
    vec3 sample = texture(uMSDF, vTexCoord).rgb;
    float dist = median3(sample.r, sample.g, sample.b);

    // The function fwidth() gives us the approximate size of one screen pixel
    // in the distance field's range for anti-aliased edges.
    float pixelWidth = fwidth(dist);

    // Shift the threshold from 0.5. The MSDF data is centered at 0.5.
    // We'll smooth-step around 0.5 to get crisp edges.
    float alpha = smoothstep(0.5 - pixelWidth, 0.5 + pixelWidth, dist);

    // Multiply alpha with the color's alpha channel
    FragColor = vec4(uColor.rgb, uColor.a * alpha);
}

#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;

uniform bool horizontal;
//uniform float weight[5] = float[] (0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);
uniform int blur_amount;
uniform float blur_radius;
uniform float bloom_intensity;

// Function to calculate Gaussian weights
float gaussianWeight(int x, float sigma) {
    return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * 3.14159) * sigma);
}


void main() {             
     float sigma = float(blur_amount) * 0.5;
     vec2 tex_offset = 1.0 / textureSize(image, 0) * blur_radius; // Gets size of single texel
     vec3 result = texture(image, TexCoords).rgb * gaussianWeight(0, sigma);
     float total_weight = gaussianWeight(0, sigma);

     if (horizontal) {
         for(int i = 1; i < blur_amount; ++i) {
            float weight = gaussianWeight(i, sigma);
            result += texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight;
            result += texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight;
            total_weight += 2.0 * weight;
         }
     }
     else {
         for(int i = 1; i < blur_amount; ++i)
         {
             float weight = gaussianWeight(i, sigma);
             result += texture(image, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight;
             result += texture(image, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight;
             total_weight += 2.0 * weight;
         }
     }
     result /= total_weight;                
     result *= bloom_intensity; 
     FragColor = vec4(result, 1.0);
}
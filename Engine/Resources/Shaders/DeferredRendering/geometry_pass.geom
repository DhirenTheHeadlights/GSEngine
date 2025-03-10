#version 450

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

layout (binding = 0) uniform ShadowUBO {
    mat4 shadow_matrices[6];
} shadow_ubo;

layout (location = 0) out vec4 frag_position; // Per vertex

void main() {
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face; // Render to each cubemap face
        for (int i = 0; i < 3; ++i) {
            frag_position = gl_in[i].gl_Position;
            gl_Position = shadow_ubo.shadow_matrices[face] * frag_position;
            EmitVertex();
        }
        EndPrimitive();
    }
}

struct cube_face {
    float3 right;
    float3 up;
    float3 forward;
};

cube_face cube_face_basis(uint face) {
    cube_face f;
    switch (face) {
        case 0u: f.forward = float3(1, 0, 0); f.right = float3(0, 0, -1); f.up = float3(0, 1, 0); break;
        case 1u: f.forward = float3(-1, 0, 0); f.right = float3(0, 0, 1); f.up = float3(0, 1, 0); break;
        case 2u: f.forward = float3(0, 1, 0); f.right = float3(1, 0, 0); f.up = float3(0, 0, -1); break;
        case 3u: f.forward = float3(0, -1, 0); f.right = float3(1, 0, 0); f.up = float3(0, 0, 1); break;
        case 4u: f.forward = float3(0, 0, 1); f.right = float3(1, 0, 0); f.up = float3(0, 1, 0); break;
        default: f.forward = float3(0, 0, -1); f.right = float3(-1, 0, 0); f.up = float3(0, 1, 0); break;
    }
    return f;
}

uint major_cube_face(float3 d) {
    float3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z) {
        return d.x >= 0.0 ? 0u : 1u;
    }
    if (a.y >= a.z) {
        return d.y >= 0.0 ? 2u : 3u;
    }
    return d.z >= 0.0 ? 4u : 5u;
}

float4x4 reversed_perspective_view_projection(float3 eye, cube_face basis, float tan_half, float near_plane) {
    float s = 1.0 / tan_half;
    return float4x4(
        float4(basis.right * s, -dot(basis.right, eye) * s),
        float4(-basis.up * s, dot(basis.up, eye) * s),
        float4(0.0, 0.0, 0.0, near_plane),
        float4(basis.forward, -dot(basis.forward, eye))
    );
}

float4x4 reversed_orthographic_view_projection(float3 center, cube_face basis, float half_extent, float near_z, float far_z) {
    float range = far_z - near_z;
    return float4x4(
        float4(basis.right / half_extent, -dot(basis.right, center) / half_extent),
        float4(-basis.up / half_extent, dot(basis.up, center) / half_extent),
        float4(-basis.forward / range, (far_z + dot(basis.forward, center)) / range),
        float4(0.0, 0.0, 0.0, 1.0)
    );
}

cube_face basis_along(float3 forward) {
    cube_face f;
    f.forward = normalize(forward);
    float3 up_hint = abs(f.forward.y) < 0.99 ? float3(0, 1, 0) : float3(1, 0, 0);
    f.right = normalize(cross(up_hint, f.forward));
    f.up = cross(f.forward, f.right);
    return f;
}

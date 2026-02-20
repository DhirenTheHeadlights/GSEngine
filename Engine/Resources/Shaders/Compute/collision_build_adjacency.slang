static const uint MAX_BODIES = 500;
static const uint MAX_CONTACTS = 2000;
static const uint MAX_COLLISION_PAIRS = 8192;
static const uint MAX_COLORS = 32;
static const uint FLAG_LOCKED = 1;
static const uint SLEEP_THRESHOLD = 300;

struct push_constants {
    uint body_count;
    uint contact_count;
    uint motor_count;
    uint color_offset;
    uint color_count;
    float h_squared;
    float dt;
    float rho;
    float linear_damping;
    float velocity_sleep_threshold;
    uint substep;
    uint iteration;
};

struct GpuBody {
    float4 position;
    float4 predicted_position;
    float4 inertia_target;
    float4 old_position;
    float4 velocity;
    float4 predicted_velocity;
    float4 orientation;
    float4 predicted_orientation;
    float4 angular_inertia_target;
    float4 old_orientation;
    float4 angular_velocity;
    float4 predicted_angular_velocity;
    float4 motor_target;
    float mass;
    uint flags;
    uint sleep_counter;
    float _pad0;
    float4 inv_inertia_col0;
    float4 inv_inertia_col1;
    float4 inv_inertia_col2;
    float4 half_extents;
    float4 aabb_min;
    float4 aabb_max;
};

struct GpuContact {
    uint body_a;
    uint body_b;
    float initial_separation;
    float lambda;
    float4 normal;
    float4 r_a;
    float4 r_b;
    float friction_coeff;
    float mass_a;
    float mass_b;
    uint feature_packed;
};

struct GpuWarmStart {
    uint body_a;
    uint body_b;
    uint feature_packed;
    float lambda;
};

[[vk::binding(0, 0)]] RWStructuredBuffer<GpuBody> body_data;
[[vk::binding(1, 0)]] RWStructuredBuffer<GpuContact> contact_data;
[[vk::binding(2, 0)]] StructuredBuffer<uint> motor_data;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint> color_data;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> body_contact_map;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> solve_state;
[[vk::binding(6, 0)]] RWStructuredBuffer<uint> collision_pairs;
[[vk::binding(7, 0)]] RWStructuredBuffer<uint> collision_state;
[[vk::binding(8, 0)]] StructuredBuffer<GpuWarmStart> warm_starts;

[[vk::push_constant]]
ConstantBuffer<push_constants> pc;

[shader("compute")]
[numthreads(1, 1, 1)]
void cs_main(uint3 dispatch_id : SV_DispatchThreadID) {
    uint num_contacts = min(collision_state[0], MAX_CONTACTS);
    uint body_count = pc.body_count;

    for (uint bi = 0; bi < body_count; bi++) {
        body_contact_map[bi] = 0;
        body_contact_map[MAX_BODIES + bi] = 0;
    }

    for (uint ci = 0; ci < num_contacts; ci++) {
        GpuContact c = contact_data[ci];
        body_contact_map[MAX_BODIES + c.body_a]++;
        body_contact_map[MAX_BODIES + c.body_b]++;
    }

    uint running_offset = 0;
    for (uint bi = 0; bi < body_count; bi++) {
        body_contact_map[bi] = running_offset;
        running_offset += body_contact_map[MAX_BODIES + bi];
    }

    uint slot_offsets[MAX_BODIES];
    for (uint bi = 0; bi < body_count; bi++) {
        slot_offsets[bi] = 0;
    }

    for (uint ci = 0; ci < num_contacts; ci++) {
        GpuContact c = contact_data[ci];
        uint off_a = body_contact_map[c.body_a] + slot_offsets[c.body_a];
        body_contact_map[MAX_BODIES * 2 + off_a] = ci;
        slot_offsets[c.body_a]++;

        uint off_b = body_contact_map[c.body_b] + slot_offsets[c.body_b];
        body_contact_map[MAX_BODIES * 2 + off_b] = ci;
        slot_offsets[c.body_b]++;
    }

    uint body_color[MAX_BODIES];
    for (uint bi = 0; bi < body_count; bi++) {
        body_color[bi] = 0xFFFFFFFF;
    }

    uint color_group_count[MAX_COLORS];
    uint color_group_bodies[MAX_BODIES];
    for (uint c = 0; c < MAX_COLORS; c++) {
        color_group_count[c] = 0;
    }

    for (uint bi = 0; bi < body_count; bi++) {
        bool locked = (body_data[bi].flags & FLAG_LOCKED) != 0;
        if (locked) continue;

        uint cc = body_contact_map[MAX_BODIES + bi];
        if (cc == 0) continue;

        uint neighbor_colors = 0;
        uint off = body_contact_map[bi];
        for (uint k = 0; k < cc; k++) {
            uint ci = body_contact_map[MAX_BODIES * 2 + off + k];
            GpuContact con = contact_data[ci];
            uint other = (con.body_a == bi) ? con.body_b : con.body_a;
            if (body_color[other] < MAX_COLORS) {
                neighbor_colors |= (1u << body_color[other]);
            }
        }

        uint chosen = 0;
        while (chosen < MAX_COLORS && (neighbor_colors & (1u << chosen)) != 0) {
            chosen++;
        }

        if (chosen < MAX_COLORS) {
            body_color[bi] = chosen;
        }
    }

    uint max_contact_color = 0;
    for (uint bi = 0; bi < body_count; bi++) {
        if (body_color[bi] < MAX_COLORS && body_color[bi] + 1 > max_contact_color) {
            max_contact_color = body_color[bi] + 1;
        }
    }

    uint motor_only_color = max_contact_color;

    uint motor_map_offset = MAX_BODIES * 2 + MAX_CONTACTS * 2;
    for (uint bi = 0; bi < body_count; bi++) {
        bool locked = (body_data[bi].flags & FLAG_LOCKED) != 0;
        if (locked) continue;

        uint cc = body_contact_map[MAX_BODIES + bi];
        bool has_motor = body_contact_map[motor_map_offset + bi] != 0xFFFFFFFF;

        if (cc == 0 && !has_motor) continue;

        if (body_color[bi] == 0xFFFFFFFF) {
            body_color[bi] = motor_only_color;
        }
    }

    uint max_used_color = 0;
    for (uint bi = 0; bi < body_count; bi++) {
        if (body_color[bi] < MAX_COLORS) {
            color_group_count[body_color[bi]]++;
            if (body_color[bi] + 1 > max_used_color) {
                max_used_color = body_color[bi] + 1;
            }
        }
    }

    collision_state[6] = max_used_color;

    uint color_offsets[MAX_COLORS];
    uint idx = 0;
    for (uint c = 0; c < max_used_color; c++) {
        color_offsets[c] = idx;
        idx += color_group_count[c];
    }

    uint fill_offsets[MAX_COLORS];
    for (uint c = 0; c < max_used_color; c++) {
        fill_offsets[c] = 0;
    }

    for (uint bi = 0; bi < body_count; bi++) {
        uint c = body_color[bi];
        if (c >= max_used_color) continue;
        uint pos = color_offsets[c] + fill_offsets[c];
        color_group_bodies[pos] = bi;
        fill_offsets[c]++;
    }

    for (uint c = 0; c < max_used_color; c++) {
        color_data[c] = color_offsets[c];
        color_data[MAX_COLORS + c] = color_group_count[c];
    }
    for (uint c = max_used_color; c < MAX_COLORS; c++) {
        color_data[MAX_COLORS + c] = 0;
    }
    for (uint i = 0; i < idx; i++) {
        color_data[MAX_COLORS * 2 + i] = color_group_bodies[i];
    }
}

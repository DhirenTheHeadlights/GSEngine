import common;
import vbd_shared;

public[[vk::binding(0, 0)]] RWStructuredBuffer<GpuBody> body_data;
public[[vk::binding(1, 0)]] RWStructuredBuffer<GpuContact> contact_data;
public[[vk::binding(2, 0)]] StructuredBuffer<GpuMotor> motor_data;
public[[vk::binding(3, 0)]] RWStructuredBuffer<uint> color_data;
public[[vk::binding(4, 0)]] RWStructuredBuffer<uint> body_contact_map;
public[[vk::binding(5, 0)]] RWStructuredBuffer<float4> solve_state;
public[[vk::binding(6, 0)]] RWStructuredBuffer<uint> collision_pairs;
public[[vk::binding(7, 0)]] RWStructuredBuffer<uint> collision_state;
public[[vk::binding(8, 0)]] StructuredBuffer<GpuWarmStart> warm_starts;
public[[vk::binding(9, 0)]] RWStructuredBuffer<GpuJoint> joint_data;

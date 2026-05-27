export module gs:skeleton_spawn;

import std;
import gse;

export namespace gs {
	struct skeleton_handle {
		std::vector<gse::id> bone_ids;
		std::vector<gse::id> joint_ids;
		std::vector<gse::id> muscle_ids;
	};

	auto spawn_skeleton(
		gse::scene& s,
		const gse::physics::skeleton& skel,
		const gse::vec3<gse::position>& root_position,
		const gse::quat& root_orientation = gse::quat(
			1.f,
			0.f,
			0.f,
			0.f
		)
	) -> skeleton_handle;
}

namespace gs {
	struct bone_box_archetype {
		gse::physics::transform_component transform;
		gse::physics::motion_component motion;
		gse::physics::collision_component collision;
		gse::primitive_box_spec spec;
	};

	struct bone_sphere_archetype {
		gse::physics::transform_component transform;
		gse::physics::motion_component motion;
		gse::physics::collision_component collision;
		gse::primitive_sphere_spec spec;
	};

	auto spawn_bone(
		gse::scene& scene,
		const std::string& name,
		const gse::physics::bone& b,
		const gse::vec3<gse::position>& world_position,
		const gse::quat& world_orientation
	) -> gse::id;
}

auto gs::spawn_bone(gse::scene& scene, const std::string& name, const gse::physics::bone& b, const gse::vec3<gse::position>& world_position, const gse::quat& world_orientation) -> gse::id {
	const auto bone_inertia = gse::physics::moment_of_inertia_of(b.shape, b.mass);

	const gse::physics::motion_component motion{
		.body =
			gse::physics::dynamic_body{
				.mass = b.mass,
				.moment_of_inertia = bone_inertia,
			},
	};

	const gse::physics::transform_component transform{
		.position = world_position,
		.orientation = world_orientation,
	};

	gse::id result{};
	std::visit(
		[&]<typename Shape>(const Shape& shape) {
			if constexpr (std::is_same_v<Shape, gse::physics::box_shape>) {
				result = scene.spawn(
					name,
					bone_box_archetype{
						.transform = transform,
						.motion = motion,
						.collision = {
							.shape = shape
						},
						.spec = {
							.material = {
								.base_color = gse::vec3f(
									0.85f,
									0.7f,
									0.55f
								),
								.roughness = 0.6f,
							},
							.size = shape.size,
						},
					}
				);
			}
			else if constexpr (std::is_same_v<Shape, gse::physics::sphere_shape>) {
				result = scene.spawn(
					name,
					bone_sphere_archetype{
						.transform = transform,
						.motion = motion,
						.collision = {
							.shape = shape
						},
						.spec = {
							.material = {
								.base_color = gse::vec3f(
									0.85f,
									0.7f,
									0.55f
								),
								.roughness = 0.6f,
							},
							.radius = shape.radius,
						},
					}
				);
			}
			else if constexpr (std::is_same_v<Shape, gse::physics::capsule_shape>) {
				const auto axial = 2.f * shape.half_height + 2.f * shape.radius;
				const auto width = 2.f * shape.radius;
				const gse::vec3<gse::length> bbox(width, axial, width);
				result = scene.spawn(
					name,
					bone_box_archetype{
						.transform = transform,
						.motion = motion,
						.collision = {
							.shape = shape
						},
						.spec = {
							.material = {
								.base_color = gse::vec3f(
									0.85f,
									0.7f,
									0.55f
								),
								.roughness = 0.6f,
							},
							.size = bbox,
						},
					}
				);
			}
		},
		b.shape
	);
	return result;
}

auto gs::spawn_skeleton(gse::scene& s, const gse::physics::skeleton& skel, const gse::vec3<gse::position>& root_position, const gse::quat& root_orientation) -> skeleton_handle {
	skeleton_handle handle;
	handle.bone_ids.resize(skel.bones.size());

	std::vector<gse::vec3<gse::position>> world_pos(skel.bones.size());
	std::vector<gse::quat> world_orient(skel.bones.size());

	for (std::size_t i = 0; i < skel.bones.size(); ++i) {
		const auto& b = skel.bones[i];
		if (b.parent_index == gse::physics::no_bone) {
			world_pos[i] = root_position + b.local_offset;
			world_orient[i] = root_orientation * b.local_rotation;
		}
		else {
			const auto& parent_pos = world_pos[b.parent_index];
			const auto& parent_rot = world_orient[b.parent_index];
			world_pos[i] = parent_pos + gse::rotate_vector(parent_rot, b.local_offset);
			world_orient[i] = parent_rot * b.local_rotation;
		}
	}

	for (std::size_t i = 0; i < skel.bones.size(); ++i) {
		const auto& b = skel.bones[i];
		const auto bone_name = std::format("{}.{}", skel.name, b.name);
		handle.bone_ids[i] = spawn_bone(s, bone_name, b, world_pos[i], world_orient[i]);
	}

	handle.joint_ids.resize(skel.joints.size());
	for (std::size_t j = 0; j < skel.joints.size(); ++j) {
		const auto& joint = skel.joints[j];
		handle.joint_ids[j] = s.build(std::format(
										  "{}.joint_{}",
										  skel.name,
										  j
									  ))
			.with<gse::physics::joint_spec>({
				.entity_a = handle.bone_ids[joint.bone_a],
				.entity_b = handle.bone_ids[joint.bone_b],
				.config = joint.config,
			})
			.identify();
	}

	handle.muscle_ids.resize(skel.muscles.size());
	for (std::size_t m = 0; m < skel.muscles.size(); ++m) {
		const auto& muscle = skel.muscles[m];
		handle.muscle_ids[m] = s.build(std::format(
										   "{}.muscle_{}",
										   skel.name,
										   m
									   ))
			.with<gse::physics::joint_spec>({
				.entity_a = handle.bone_ids[muscle.bone_a],
				.entity_b = handle.bone_ids[muscle.bone_b],
				.config = gse::physics::muscle_joint{
					.anchor_a = muscle.anchor_a,
					.anchor_b = muscle.anchor_b,
					.rest_length = muscle.rest_length,
					.max_force = muscle.max_force,
				},
			})
			.with<gse::physics::muscle_component>({})
			.identify();
	}

	return handle;
}

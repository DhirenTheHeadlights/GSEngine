export module gse.graphics.camera;

import std;

import gse.physics.math;
import gse.graphics.debug;
import gse.platform.glfw.window;

export namespace gse {
	class camera {
	public:
		camera(const vec3<length>& initial_position = { 0.f }) : m_position(initial_position) {}

		auto process_mouse_movement(const vec2<length>& offset) -> void;
		auto update_camera_vectors() -> void;

		auto set_position(const vec3<length>& position) -> void { this->m_position = position; }

		auto get_view_matrix() const -> mat4;
		auto get_projection_matrix() const -> mat4;
		auto get_position() const -> vec3<length>;
		auto get_camera_direction_relative_to_origin(const unitless::vec3& direction) const -> unitless::vec3;
	private:
		vec3<length> m_position;
		unitless::vec3 m_front{ 0.0f, 0.0f, -1.0f };
		unitless::vec3 m_up{ 0.0f, 1.0f, 0.0f };
		unitless::vec3 m_right{ 1.0f, 0.0f, 0.0f };
		unitless::vec3 m_world_up{ 0.0f, 1.0f, 0.0f };

		angle m_yaw = degrees(-90.0f);
		angle m_pitch = degrees(0.0f);

		float m_movement_speed = 2.5f;
		float m_mouse_sensitivity = 0.1f;

		length m_near_plane = meters(0.1f);
		length m_far_plane = meters(10000.0f);
	};
}

auto gse::camera::process_mouse_movement(const vec2<length>& offset) -> void {
	const vec2 transformed_offset = offset * m_mouse_sensitivity;
	m_yaw += degrees(transformed_offset.x.as_default_unit());
	m_pitch -= degrees(transformed_offset.y.as_default_unit());
	m_pitch = std::clamp(m_pitch, degrees(-89.0f), degrees(89.0f));
	update_camera_vectors();
}

auto gse::camera::update_camera_vectors() -> void {
	const vec3<length> new_front(
		std::cos(m_yaw.as<units::radians>()) * std::cos(m_pitch.as<units::radians>()),
		std::sin(m_pitch.as<units::radians>()),
		std::sin(m_yaw.as<units::radians>()) * std::cos(m_pitch.as<units::radians>())
	);

	m_front = normalize(new_front);
	m_right = normalize(cross(m_front, m_world_up));
	m_up = normalize(cross(m_right, m_front));
}

auto gse::camera::get_view_matrix() const -> mat4 {
	return look_at(m_position, m_position + m_front, m_up);
}

auto gse::camera::get_projection_matrix() const -> mat4 {
	const unitless::vec2 view_port_size = { static_cast<float>(window::get_frame_buffer_size().x), static_cast<float>(window::get_frame_buffer_size().y) };
	const float aspect_ratio = view_port_size.x / view_port_size.y;
	return perspective(degrees(45.0f), aspect_ratio, m_near_plane, m_far_plane);
}

auto gse::camera::get_position() const -> vec3<length> {
	return m_position;
}

auto gse::camera::get_camera_direction_relative_to_origin(const unitless::vec3& direction) const -> unitless::vec3 {
	return m_right * direction.x + m_up * direction.y + m_front * direction.z;
}


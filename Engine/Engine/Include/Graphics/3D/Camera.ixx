module;

#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"

export module gse.graphics.camera;

import glm;
import std;

import gse.physics.math;
import gse.graphics.debug;

export namespace gse {
	class camera {
	public:
		camera(const vec3<length>& initial_position = { 0.f }) : m_position(initial_position) {}

		auto move_relative_to_origin(const unitless::vec3& direction, float distance, float delta_time) -> void;

		auto process_mouse_movement(const vec2<length>& offset) -> void;
		auto update_camera_vectors() -> void;

		auto set_position(const vec3<length>& position) -> void { this->m_position = position; }

		auto get_view_matrix() const -> glm::mat4;
		auto get_projection_matrix() -> glm::mat4;
		auto get_position() const -> vec3<length>;
		auto get_camera_direction_relative_to_origin(const unitless::vec3& direction) const -> unitless::vec3;
	private:
		vec3<length> m_position;
		unitless::vec3 m_front{ 0.0f, 0.0f, -1.0f };
		unitless::vec3 m_up{ 0.0f, 1.0f, 0.0f };
		unitless::vec3 m_right{ 1.0f, 0.0f, 0.0f };
		unitless::vec3 m_world_up{ 0.0f, 1.0f, 0.0f };

		float m_yaw = -90.0f;
		float m_pitch = 0.0f;

		float m_movement_speed = 2.5f;
		float m_mouse_sensitivity = 0.1f;
	};
}

import gse.platform.glfw.window;

auto gse::camera::move_relative_to_origin(const unitless::vec3& direction, const float distance, const float delta_time) -> void {
	const auto norm_direction = normalize(direction);
	const unitless::vec3 camera_direction =
		m_right * norm_direction.x +
		m_up * norm_direction.y +
		m_front * norm_direction.z;
	m_position += vec3<length>(camera_direction * distance * m_movement_speed * delta_time);
}

auto gse::camera::process_mouse_movement(const vec2<length>& offset) -> void {
	std::cout << "Mouse movement: " << offset.x.as_default_unit() << offset.y.as_default_unit() << std::endl;
	const vec2 transformed_offset = offset * m_mouse_sensitivity;
	m_yaw += transformed_offset.x.as_default_unit();
	m_pitch -= transformed_offset.y.as_default_unit();
	if (m_pitch > 89.0f) m_pitch = 89.0f;
	if (m_pitch < -89.0f) m_pitch = -89.0f;
	update_camera_vectors();
}

auto gse::camera::update_camera_vectors() -> void {
	const vec3<length> new_front(
		std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch)),
		std::sin(glm::radians(m_pitch)),
		std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch))
	);

	m_front = normalize(new_front);
	m_right = normalize(cross(m_front, m_world_up));
	m_up = normalize(cross(m_right, m_front));
}

auto gse::camera::get_view_matrix() const -> glm::mat4 {
	return to_glm_mat(look_at(m_position, m_position + m_front, m_up));
}

auto gse::camera::get_projection_matrix() -> glm::mat4 {
	const unitless::vec2 view_port_size = { static_cast<float>(window::get_frame_buffer_size().x), static_cast<float>(window::get_frame_buffer_size().y) };
	const float aspect_ratio = view_port_size.x / view_port_size.y;
	return to_glm_mat(perspective(degrees(45.0f), aspect_ratio, meters(0.1f), meters(10000000.f)));
}

auto gse::camera::get_position() const -> vec3<length> {
	return m_position;
}

auto gse::camera::get_camera_direction_relative_to_origin(const unitless::vec3& direction) const -> unitless::vec3 {
	return m_right * direction.x + m_up * direction.y + m_front * direction.z;
}


export module gse.graphics:camera;

import std;

import gse.physics.math;
import gse.platform;

export namespace gse {
	class camera {
	public:
		camera(const vec3<length>& initial_position = { 0.f });

		auto process_mouse_movement(const vec2<length>& offset) -> void;
		auto update_orientation() -> void;

		auto set_position(const vec3<length>& position) -> void ;
		auto move(const vec3<length>& offset) -> void ;

		auto view() const -> mat4;
		auto projection(unitless::vec2 viewport) const -> mat4;
		auto position() const -> vec3<length>;
		auto orientation() const -> quat;
		auto direction_relative_to_origin(const unitless::vec3& direction) const -> unitless::vec3;
	private:
		vec3<length> m_position;
		quat m_orientation;

		angle m_yaw = degrees(-90.0f);
		angle m_pitch = degrees(0.0f);

		float m_movement_speed = 2.5f;
		float m_mouse_sensitivity = 0.1f;

		length m_near_plane = meters(0.1f);
		length m_far_plane = meters(10000.0f);
	};
}

gse::camera::camera(const vec3<length>& initial_position): m_position(initial_position) {
	update_orientation();
}

auto gse::camera::process_mouse_movement(const vec2<length>& offset) -> void {
	const vec2 transformed_offset = offset * m_mouse_sensitivity;
	m_yaw += degrees(transformed_offset.x.as_default_unit());
	m_pitch += degrees(transformed_offset.y.as_default_unit());
	m_pitch = std::clamp(m_pitch, degrees(-89.0f), degrees(89.0f));
}

auto gse::camera::update_orientation() -> void {
	const quat yaw_rotation = from_axis_angle({ 0.f, 1.f, 0.f }, m_yaw);
	const quat pitch_rotation = from_axis_angle({ 1.f, 0.f, 0.f }, m_pitch);
	m_orientation = normalize(yaw_rotation * pitch_rotation);
}

auto gse::camera::set_position(const vec3<length>& position) -> void { this->m_position = position; }

auto gse::camera::move(const vec3<length>& offset) -> void {
	m_position += offset;
}

auto gse::camera::view() const -> mat4 {
	const auto rotation = mat4(conjugate(m_orientation));
	const mat4 translation = translate(mat4(1.0f), -m_position);
	return rotation * translation;
}

auto gse::camera::projection(const unitless::vec2 viewport) const -> mat4 {
	const float aspect_ratio = viewport.x / viewport.y;
	return perspective(degrees(45.0f), aspect_ratio, m_near_plane, m_far_plane);
}

auto gse::camera::position() const -> vec3<length> {
	return m_position;
}

auto gse::camera::orientation() const -> quat {
	return m_orientation;
}

auto gse::camera::direction_relative_to_origin(const unitless::vec3& direction) const -> unitless::vec3 {
	const auto u = m_orientation.imaginary_part();
	const auto t = 2.f * cross(u, direction);
	return direction + m_orientation.s * t + cross(u, t);
}

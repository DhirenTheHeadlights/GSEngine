#include "Graphics/3D/Camera.h"

#include "Physics/Vector/Math.h"
#include "Platform/GLFW/Window.h"

void gse::camera::move_relative_to_origin(const vec3<>& direction, const float distance, const float delta_time) {
	const auto normDirection = normalize(direction).as_default_units();
	const vec3<length> cameraDirection =
		m_right * normDirection.x +
		m_up * normDirection.y +
		m_front * normDirection.z;
	m_position += cameraDirection * distance * m_movement_speed * delta_time;
}

void gse::camera::process_mouse_movement(glm::vec2& offset) {
    offset *= m_mouse_sensitivity;
    m_yaw += offset.x;
    m_pitch -= offset.y;
    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;
    update_camera_vectors();
}
 
void gse::camera::update_camera_vectors() {
	const vec3<length> newFront(
		cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)),
		sin(glm::radians(m_pitch)),
		sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch))
	);
    m_front = normalize(newFront);
    m_right = normalize(cross(m_front, m_world_up));
	m_up = normalize(cross(m_right, m_front));
}

glm::mat4 gse::camera::get_view_matrix() const {
	return lookAt(m_position.as<Meters>(), (m_position + m_front).as<Meters>(), m_up.as<Meters>());
}

glm::mat4 gse::camera::get_projection_matrix() {
	const glm::vec2 viewPortSize = window::get_frame_buffer_size();
	const float aspectRatio = viewPortSize.x / viewPortSize.y;
	return glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 10000.f);
}

gse::vec3<gse::length> gse::camera::get_position() const {
	return m_position;
}

gse::vec3<> gse::camera::get_camera_direction_relative_to_origin(const vec3<>& direction) const {
	return { m_right.as<Meters>() * direction.as_default_units().x + m_up.as<Meters>() * direction.as_default_units().y + m_front.as<Meters>() * direction.as_default_units().z };
}

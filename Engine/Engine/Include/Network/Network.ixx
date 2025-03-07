module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

export module gse.network;

import gse.core.object_registry;
import gse.graphics.render_component;
import gse.network.socket;
import gse.platform.perma_assert;
import gse.physics;




export namespace gse::network {
	auto initialize() -> void;
	auto shutdown() -> void;
	auto bind_socket() -> void;
	auto send_components(address target_address, udp_socket socket) -> void;
}

auto gse::network::initialize() -> void {
	WSADATA wsa_data;
	if (const auto result = WSAStartup(MAKEWORD(2, 2), &wsa_data); result != 0) {
		std::cerr << "WSAStartup failed: " << result << '\n';
	}
}

auto gse::network::shutdown() -> void {
	if (const auto result = WSACleanup(); result != 0) {
		std::cerr << "WSACleanup failed: " << result << '\n';
	}
}

template <typename T> 
auto write_to_buffer(uint8_t*& ptr, const T& data) {
	memcpy(ptr, &data, sizeof(T));
	ptr += sizeof(T);
}

template <typename T, int N>
auto write_to_buffer(uint8_t*& ptr, const gse::unitless::vec_t<T, N>& data) {
	memcpy(ptr, data.data(), sizeof(T) * N);
	ptr += sizeof(T) * N;
}

template <typename T, int N>
auto write_to_buffer(uint8_t*& ptr, const gse::vec_t<T, N>& data) {
    write_to_buffer(ptr, data.template as<T::default_unit>());
}

auto gse::network::send_components(address target_address, udp_socket socket) -> void {
	uint8_t buffer[256];
	for (const auto& component : gse::registry::get_components<gse::render_component>()) {
		memset(buffer, 0, sizeof(buffer));
		serialize_render_component(buffer, component);
		packet new_packet{ buffer, sizeof(buffer) };
		socket.send_data(new_packet, target_address);

	}
	for (const auto& component : gse::registry::get_components<gse::physics::motion_component>()) {
		memset(buffer, 0, sizeof(buffer));
		serialize_motion_component(buffer, component);		
		packet new_packet{ buffer, sizeof(buffer) };
		socket.send_data(new_packet, target_address);
	}
	for (const auto& component : gse::registry::get_components<gse::physics::collision_component>()) {
		memset(buffer, 0, sizeof(buffer));
		serialize_collision_component(buffer, component);		
		packet new_packet{ buffer, sizeof(buffer) };
		socket.send_data(new_packet, target_address);
	}

}

auto serialize_motion_component(uint8_t* buffer, const gse::physics::motion_component& motion) -> void{
    uint8_t* ptr = buffer;
    *ptr++ = 1; // Packet type: Motion Update
	write_to_buffer(ptr, sizeof(gse::physics::motion_component));
    write_to_buffer(ptr, motion.parent_id);
	write_to_buffer(ptr, motion.current_position);
	write_to_buffer(ptr, motion.current_velocity);
	write_to_buffer(ptr, motion.current_acceleration);
	write_to_buffer(ptr, motion.current_torque);
	write_to_buffer(ptr, motion.orientation);
	write_to_buffer(ptr, motion.angular_velocity);
	write_to_buffer(ptr, motion.angular_acceleration);
	write_to_buffer(ptr, motion.max_speed);
	write_to_buffer(ptr, motion.mass);
	write_to_buffer(ptr, motion.most_recent_y_collision);
	write_to_buffer(ptr, motion.moment_of_inertia);
	write_to_buffer(ptr, motion.affected_by_gravity);
	write_to_buffer(ptr, motion.moving);
	write_to_buffer(ptr, motion.airborne);
	write_to_buffer(ptr, motion.self_controlled);
}

auto serialize_collision_component(uint8_t* buffer, const gse::physics::collision_component& collision) -> void {
	uint8_t* ptr = buffer;
	*ptr++ = 2; // Packet type: Collision Update
	write_to_buffer(ptr, sizeof(gse::physics::collision_component));
	write_to_buffer(ptr, collision.parent_id);
	write_to_buffer(ptr, collision.bounding_box);
	write_to_buffer(ptr, collision.oriented_bounding_box);
	write_to_buffer(ptr, collision.collision_information);
	write_to_buffer(ptr, collision.resolve_collisions);
}

auto serialize_render_component(uint8_t* buffer, const gse::render_component& render) -> void {
	uint8_t* ptr = buffer;
	*ptr++ = 3; // Packet type: Render Update
	write_to_buffer(ptr, sizeof(gse::render_component));
	write_to_buffer(ptr, render.parent_id);
	write_to_buffer(ptr, render.models);
	write_to_buffer(ptr, render.bounding_box_meshes);
	write_to_buffer(ptr, render.center_of_mass);
	write_to_buffer(ptr, render.render);
	write_to_buffer(ptr, render.render_bounding_boxes);
}

//void SerializeRegistry(uint8_t* buffer, const gse::registry::object_registry& registry) {
//	uint8_t* ptr = buffer;
//	*ptr++ = 4; // Packet type: Registry Update
//	write_to_buffer(ptr, sizeof(gse::registry::object_registry));
//	write_to_buffer(ptr, registry.g_entity_lists);
//	write_to_buffer(ptr, registry.g_string_to_index_map);
//	write_to_buffer(ptr, registry.g_entities);
//	write_to_buffer(ptr, registry.g_free_indices);
//	write_to_buffer(ptr, registry.g_registered_inactive_entities);
//	write_to_buffer(ptr, registry.g_hooks);
//	write_to_buffer(ptr, registry.g_registered_inactive_hooks);
//	write_to_buffer(ptr, registry.g_component_containers);
//}
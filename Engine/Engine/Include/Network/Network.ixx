module;

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

export module gse.network;

import gse.core.object_registry;
import gse.core.component;
import gse.graphics.render_component;
import gse.network.socket;
import gse.platform.perma_assert;
import gse.physics.collision_component;
import gse.physics.math;
import gse.physics.motion_component;





export namespace gse::network {
	auto initialize() -> void;
	auto shutdown() -> void;
	auto bind_socket() -> void;
	auto send_components(address target_address, udp_socket socket) -> void;
	auto receive_components(address target_address, udp_socket socket) -> void;
}

namespace gse::network {
	template <typename T> auto write_to_buffer(uint8_t*& ptr, const T& data) -> void;
	template <typename T, int N> auto write_to_buffer(uint8_t*& ptr, const gse::unitless::vec_t<T, N>& data) -> void;
	template <typename T, int N> auto write_to_buffer(uint8_t*& ptr, const gse::vec_t<T, N>& data) -> void;
	template <typename T, typename Field> auto read_from_buffer(uint8_t* buffer, T& component, Field T::* field) -> void;
	auto serialize_motion_component(uint8_t* buffer, const gse::physics::motion_component& motion, uint8_t(&buffer_debug)[256]) -> void;
	auto deserialize_component(uint8_t* buffer) -> void;
	auto serialize_collision_component(uint8_t* buffer, const gse::physics::collision_component& collision) -> void;
	auto serialize_render_component(uint8_t* buffer, const gse::render_component& render) -> void;
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
auto gse::network::write_to_buffer(uint8_t*& ptr, const T& data) -> void {
	memcpy(ptr, &data, sizeof(T));
	ptr += sizeof(T);
}

template <typename T, int N>
auto gse::network::write_to_buffer(uint8_t*& ptr, const gse::unitless::vec_t<T, N>& data) -> void {
	memcpy(ptr, value_ptr(data), sizeof(T) * N);
	ptr += sizeof(T) * N;
}

template <typename T, int N>
auto gse::network::write_to_buffer(uint8_t*& ptr, const gse::vec_t<T, N>& data) -> void {
    write_to_buffer(ptr, data.template as<typename T::default_unit>());
}

template <typename T, typename Field>
auto gse::network::read_from_buffer(uint8_t* buffer, T& component, Field T::* member) -> void {
	size_t offset = reinterpret_cast<size_t>(&(reinterpret_cast<T*>(0)->*member));
	Field extracted_value;
	std::memcpy(&extracted_value, buffer + offset, sizeof(Field));
	if (extracted_value != component.*member) {
		component.*member = extracted_value;
	}
}



auto gse::network::send_components(address target_address, udp_socket socket) -> void {
	uint8_t buffer[256];
	for (const auto& component : gse::registry::get_components<gse::render_component>()) {
		std::fill(std::begin(buffer), std::end(buffer), 0);
		serialize_render_component(buffer, component);
		packet new_packet{ buffer, sizeof(buffer) };
		deserialize_component(buffer);
		//socket.send_data(new_packet, target_address);

	}
	for (const auto& component : gse::registry::get_components<gse::physics::motion_component>()) {
		std::fill(std::begin(buffer), std::end(buffer), 0);
		serialize_motion_component(buffer, component, buffer);		
		packet new_packet{ buffer, sizeof(buffer) };
		deserialize_component(buffer);
		//socket.send_data(new_packet, target_address);
	}
	for (const auto& component : gse::registry::get_components<gse::physics::collision_component>()) {
		std::fill(std::begin(buffer), std::end(buffer), 0);
		serialize_collision_component(buffer, component);		
		packet new_packet{ buffer, sizeof(buffer) };
		deserialize_component(buffer);
		//socket.send_data(new_packet, target_address);
	}

}

auto gse::network::receive_components(address target_address, udp_socket socket) -> void {
	uint8_t buffer[256];
	packet incoming_packet{ buffer, sizeof(buffer) };
	socket.receive_data(incoming_packet, target_address);
	deserialize_component(buffer);
}

auto gse::network::serialize_motion_component(uint8_t* buffer, const gse::physics::motion_component& motion, uint8_t(&buffer_debug)[256]) -> void{
    uint8_t* ptr = buffer;
    *ptr++ = 1; // Packet type: Motion Update
	write_to_buffer(ptr, static_cast<uint8_t>(sizeof(gse::physics::motion_component) + 1));
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

auto gse::network::deserialize_component(uint8_t* ptr) -> void {
	switch (ptr[0]) {
	case (1): {
		gse::physics::motion_component& motion = gse::registry::get_component<gse::physics::motion_component>(ptr[2]);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::parent_id);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::current_position);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::current_velocity);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::current_acceleration);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::current_torque);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::orientation);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::angular_velocity);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::angular_acceleration);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::max_speed);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::mass);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::most_recent_y_collision);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::moment_of_inertia);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::affected_by_gravity);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::moving);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::airborne);
		read_from_buffer<gse::physics::motion_component>(ptr, motion, &gse::physics::motion_component::self_controlled);
		break;
		}

	case (2): {
		gse::physics::collision_component& collision = gse::registry::get_component<gse::physics::collision_component>(ptr[2]);
		read_from_buffer<gse::physics::collision_component>(ptr, collision, &gse::physics::collision_component::parent_id);
		//read_from_buffer<gse::physics::collision_component>(ptr, collision, &gse::physics::collision_component::bounding_box);
		//read_from_buffer<gse::physics::collision_component>(ptr, collision, &gse::physics::collision_component::oriented_bounding_box);
		read_from_buffer<gse::physics::collision_component>(ptr, collision, &gse::physics::collision_component::collision_information);
		read_from_buffer<gse::physics::collision_component>(ptr, collision, &gse::physics::collision_component::resolve_collisions);
		break;
		}


	case (3): {
		//gse::render_component& render = gse::registry::get_component<gse::render_component>(ptr[1]);
		//read_from_buffer<gse::render_component>(ptr, render, &gse::render_component::parent_id);
		//read_from_buffer<gse::render_component>(ptr, render, &gse::render_component::center_of_mass);
		break;
		}
	}
}

auto gse::network::serialize_collision_component(uint8_t* buffer, const gse::physics::collision_component& collision) -> void {
	uint8_t* ptr = buffer;
	*ptr++ = 2; // Packet type: Collision Update
	write_to_buffer(ptr, static_cast < uint8_t>(sizeof(gse::physics::collision_component) + 1));
	write_to_buffer(ptr, collision.parent_id);
	write_to_buffer(ptr, collision.bounding_box);
	write_to_buffer(ptr, collision.oriented_bounding_box);
	write_to_buffer(ptr, collision.collision_information);
	write_to_buffer(ptr, collision.resolve_collisions);
}

auto gse::network::serialize_render_component(uint8_t* buffer, const gse::render_component& render) -> void {
	uint8_t* ptr = buffer;
	*ptr++ = 3; // Packet type: Render Update
	write_to_buffer(ptr, static_cast < uint8_t>(sizeof(gse::render_component) + 1));
	write_to_buffer(ptr, render.parent_id);
	write_to_buffer(ptr, render.models);
	write_to_buffer(ptr, render.bounding_box_meshes);
	write_to_buffer(ptr, render.center_of_mass);
	write_to_buffer(ptr, render.render);
	write_to_buffer(ptr, render.render_bounding_boxes);
} 

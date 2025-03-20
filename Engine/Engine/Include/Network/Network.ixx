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
import gse.network.transfer_templates;
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
	auto serialize_motion_component(uint8_t* buffer, const gse::physics::motion_component& motion) -> void;
	auto deserialize_component(uint8_t* buffer) -> void;
	auto serialize_collision_component(uint8_t* buffer, const gse::physics::collision_component& collision) -> void;
	auto serialize_render_component(uint8_t* buffer, const gse::render_component& render) -> void;

	bool network_down = false;
}

auto gse::network::initialize() -> void {
	WSADATA wsa_data;
	if (const auto result = WSAStartup(MAKEWORD(2, 2), &wsa_data); result != 0) {
		std::cerr << "WSAStartup failed: " << result << '\n';
	}
}

auto gse::network::shutdown() -> void {
	network_down = true;
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
	if (network_down) {
		return;
	}

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
		serialize_motion_component(buffer, component);		
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

auto gse::network::serialize_motion_component(uint8_t* buffer, const gse::physics::motion_component& motion) -> void{
		uint8_t* ptr = buffer;
		*ptr++ = 1; // Packet type: Motion Update

		motion_component_transfer transfer_data{
			motion.parent_id,
			motion.current_position,
			motion.current_velocity,
			motion.current_acceleration,
			motion.current_torque,
			motion.orientation,
			motion.angular_velocity,
			motion.angular_acceleration,
			motion.max_speed,
			motion.mass,
			motion.most_recent_y_collision,
			motion.moment_of_inertia,
			motion.affected_by_gravity,
			motion.moving,
			motion.airborne,
			motion.self_controlled
		};

		write_to_buffer(ptr, transfer_data);
}


auto gse::network::serialize_collision_component(uint8_t* buffer, const gse::physics::collision_component& collision) -> void {
	uint8_t* ptr = buffer;
	*ptr++ = 2; // Packet type: Collision Update

	collision_component_transfer transfer_data{
		collision.parent_id,
		collision.bounding_box,
		collision.oriented_bounding_box,
		collision.collision_information,
		collision.resolve_collisions
	};

	write_to_buffer(ptr, transfer_data);
}

auto gse::network::serialize_render_component(uint8_t* buffer, const gse::render_component& render) -> void {
	uint8_t* ptr = buffer;
	*ptr++ = 3; // Packet type: Render Update
	render_component_transfer transfer_data{
		render.parent_id,
		render.models,
		render.render,
		render.render_bounding_boxes
	};

	write_to_buffer(ptr, transfer_data);
} 

auto gse::network::deserialize_component(uint8_t* ptr) -> void {
	switch (ptr[0]) {
	case (1): {
		motion_component_transfer transfer_data;
		std::memcpy(&transfer_data, ptr + 1, sizeof(transfer_data));

		gse::physics::motion_component& motion = gse::registry::get_component<gse::physics::motion_component>(transfer_data.parent_id);
		motion.current_position = transfer_data.current_position;
		motion.current_velocity = transfer_data.current_velocity;
		motion.current_acceleration = transfer_data.current_acceleration;
		motion.current_torque = transfer_data.current_torque;
		motion.orientation = transfer_data.orientation;
		motion.angular_velocity = transfer_data.angular_velocity;
		motion.angular_acceleration = transfer_data.angular_acceleration;
		motion.max_speed = transfer_data.max_speed;
		motion.mass = transfer_data.mass;
		motion.most_recent_y_collision = transfer_data.most_recent_y_collision;
		motion.moment_of_inertia = transfer_data.moment_of_inertia;
		motion.affected_by_gravity = transfer_data.affected_by_gravity;
		motion.moving = transfer_data.moving;
		motion.airborne = transfer_data.airborne;
		motion.self_controlled = transfer_data.self_controlled;
		break;
		}

	case (2): {
		collision_component_transfer transfer_data;
		std::memcpy(&transfer_data, ptr + 1, sizeof(transfer_data));

		gse::physics::collision_component& collision = gse::registry::get_component<gse::physics::collision_component>(transfer_data.parent_id);
		collision.bounding_box = transfer_data.bounding_box;
		collision.oriented_bounding_box = transfer_data.oriented_bounding_box;
		collision.collision_information = transfer_data.collision_information;
		collision.resolve_collisions = transfer_data.resolve_collisions;
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
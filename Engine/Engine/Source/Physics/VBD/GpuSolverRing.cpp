module gse.physics:vbd_gpu_solver_ring_impl;

import std;

import :vbd_gpu_solver;
import :vbd_constraints;

import gse.core;
import gse.log;
import gse.concurrency;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;

namespace gse::vbd {
	constexpr gpu::buffer_usage ring_usage{ gpu::buffer_flag::transfer_src, gpu::buffer_flag::transfer_dst };
}

auto gse::vbd::gpu_solver::ensure_ring(gpu::device& device, const std::uint32_t history) -> void {
	const std::uint32_t wanted = std::min(history, ring_max_history);
	if (wanted == 0 || !m_ring.empty()) {
		return;
	}

	m_ring.resize(wanted);
	for (std::size_t i = 0; i < m_ring.size(); ++i) {
		ring_slot& slot = m_ring[i];
		slot.bodies = device.create_buffer(
			{
				.size = m_capacities.ring_max_bodies * sizeof(body_state),
				.stride = sizeof(body_state),
				.usage = ring_usage,
				.device_local = true
			},
			"vbd.ring.bodies"
		);
		slot.joints = device.create_buffer(
			{
				.size = m_capacities.max_joints * sizeof(joint_constraint),
				.stride = sizeof(joint_constraint),
				.usage = ring_usage,
				.device_local = true
			},
			"vbd.ring.joints"
		);
		slot.contacts = device.create_buffer(
			{
				.size = m_capacities.ring_max_contacts * sizeof(contact_constraint),
				.stride = sizeof(contact_constraint),
				.usage = ring_usage,
				.device_local = true
			},
			"vbd.ring.contacts"
		);
		slot.contact_counts = device.create_buffer(
			{
				.size = m_capacities.ring_max_bodies * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = ring_usage,
				.device_local = true
			},
			"vbd.ring.contact_counts"
		);
		slot.contact_offsets = device.create_buffer(
			{
				.size = m_capacities.ring_max_bodies * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = ring_usage,
				.device_local = true
			},
			"vbd.ring.contact_offsets"
		);
		slot.contact_adjacency = device.create_buffer(
			{
				.size = m_capacities.ring_max_contacts * 2 * sizeof(std::uint32_t),
				.stride = sizeof(std::uint32_t),
				.usage = ring_usage,
				.device_local = true
			},
			"vbd.ring.contact_adjacency"
		);
	}
	m_ring_history = wanted;
	log::println(log::category::physics, "vbd gpu rollback ring: {} slots, {} bodies and {} contacts per slot", wanted, m_capacities.ring_max_bodies, m_capacities.ring_max_contacts);
}

auto gse::vbd::gpu_solver::ring_history() const -> std::uint32_t {
	return m_ring_history;
}

auto gse::vbd::gpu_solver::stage_ring_copy(per_frame_data& f, const std::uint64_t tick, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	if (m_ring_history == 0) {
		co_return;
	}
	if (m_body_count > m_capacities.ring_max_bodies) {
		if (!m_ring_overflow_reported) {
			m_ring_overflow_reported = true;
			log::println(log::level::warning, log::category::physics, "vbd gpu rollback ring holds {} bodies per slot but the scene has {}; the ring is off for this scene", m_capacities.ring_max_bodies, m_body_count);
		}
		co_return;
	}

	ring_slot& slot = m_ring[static_cast<std::size_t>(tick % m_ring_history)];

	auto rec = co_await gpu::pass<vbd_ring_copy_stage>(pass_out).on(gpu::queue_type::compute).in_chain<vbd_solve_chain>(chain_index);
	rec.copy_buffer(f.body_buffer, slot.bodies, m_body_count * sizeof(body_state));
	if (m_joint_count > 0) {
		rec.copy_buffer(f.joint_buffer, slot.joints, m_joint_count * sizeof(joint_constraint));
	}
	rec.copy_buffer(f.contact_buffer, slot.contacts, m_capacities.ring_max_contacts * sizeof(contact_constraint));
	rec.copy_buffer(f.contact_counts_buffer, slot.contact_counts, m_body_count * sizeof(std::uint32_t));
	rec.copy_buffer(f.contact_offsets_buffer, slot.contact_offsets, m_body_count * sizeof(std::uint32_t));
	rec.copy_buffer(f.contact_adjacency_buffer, slot.contact_adjacency, m_capacities.ring_max_contacts * 2 * sizeof(std::uint32_t));

	slot.tick = tick;
	slot.valid = true;
}

auto gse::vbd::gpu_solver::stage_ring_restore(per_frame_data& f, per_frame_data& other, const std::uint64_t tick, const std::uint32_t chain_index, const pass_channel pass_out) -> async::task<> {
	if (m_ring_history == 0) {
		co_return;
	}

	const ring_slot& slot = m_ring[static_cast<std::size_t>(tick % m_ring_history)];
	if (!slot.valid || slot.tick != tick) {
		log::println(log::level::warning, log::category::physics, "vbd gpu rollback to tick {} refused: the ring slot holds tick {}", tick, slot.valid ? slot.tick : 0);
		co_return;
	}

	log::println(log::category::physics, "vbd gpu rollback: restoring tick {} into the next batch of {} tick(s)", tick, m_ticks);

	auto rec = co_await gpu::pass<vbd_ring_restore_stage>(pass_out).on(gpu::queue_type::compute).in_chain<vbd_solve_chain>(chain_index);
	rec.copy_buffer(slot.bodies, f.body_buffer, m_body_count * sizeof(body_state));
	if (m_joint_count > 0) {
		rec.copy_buffer(slot.joints, f.joint_buffer, m_joint_count * sizeof(joint_constraint));
	}
	rec.copy_buffer(slot.contacts, other.contact_buffer, m_capacities.ring_max_contacts * sizeof(contact_constraint));
	rec.copy_buffer(slot.contacts, other.warm_start_buffer, m_capacities.ring_max_contacts * sizeof(contact_constraint));
	rec.copy_buffer(slot.contact_counts, other.contact_counts_buffer, m_body_count * sizeof(std::uint32_t));
	rec.copy_buffer(slot.contact_offsets, other.contact_offsets_buffer, m_body_count * sizeof(std::uint32_t));
	rec.copy_buffer(slot.contact_adjacency, other.contact_adjacency_buffer, m_capacities.ring_max_contacts * 2 * sizeof(std::uint32_t));
}

module gse.audio;

import std;

import gse.assets;
import gse.concurrency;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.log;
import gse.math;
import gse.physics;
import gse.time;

auto gse::bake(const std::filesystem::path& src, audio_clip::baked& out) -> bool {
	std::ifstream file(src, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		log::println(log::level::warning, log::category::assets, "Failed to open audio source: {}", src.generic_display_string());
		return false;
	}
	const std::streamoff size = file.tellg();
	if (!std::in_range<std::size_t>(size)) {
		log::println(log::level::warning, log::category::assets, "Audio source {} is {} bytes, which does not fit this build's address space", src.generic_display_string(), size);
		return false;
	}
	file.seekg(0);
	out.bytes.storage.resize(static_cast<std::size_t>(size));
	file.read(reinterpret_cast<char*>(out.bytes.storage.data()), size);
	return true;
}

auto gse::audio_clip::samples() const -> std::span<const float> {
	return m_samples;
}

auto gse::audio::init(const shared_view<launch::data> launch_s, data& d) -> async::task<> {
	d.mode = launch_s.audio;
	d.output = std::make_unique<mixer>();
	d.slots.resize(voice_capacity);
	d.free_slots.resize(voice_capacity);
	std::ranges::iota(d.free_slots | std::views::reverse, 0u);
	d.frames_per_step = static_cast<std::uint32_t>(std::lround(sample_rate() * system_clock::fixed_dt()));

	log::println(log::category::general, "audio: listening through {}, head radius {:.4f}", d.listening, head_radius());

	if (d.mode == launch::audio_output::device) {
		d.device = open_device(*d.output);
	}
	else {
		log::println(log::category::general, "audio: offline render, {} frames per fixed step, no device opened", d.frames_per_step);
	}

	return {};
}

auto gse::audio::silent(const data& d) -> bool {
	return d.mode == launch::audio_output::device && !d.device;
}

auto gse::audio::blocked_share(data& d, const scene_listener& listener, const std::array<id, 2>& unheard, const vec3<current_position>& emitter_position) -> float {
	constexpr std::uint32_t rim_rays = 8;
	constexpr float nearly_vertical = 0.9f;
	const length source_radius = meters(0.5f);
	const length surface_clearance = centimeters(1.f);

	const vec3<position> ear = listener.transform.position;
	const vec3<position> center = emitter_position;
	const auto line = center - ear;
	if (magnitude(line) <= length{}) {
		return 0.f;
	}

	const vec3f toward = normalize(line);
	const vec3f reference = std::abs(toward.y()) < nearly_vertical ? vec3f(0.f, 1.f, 0.f) : vec3f(1.f, 0.f, 0.f);
	const vec3f across = normalize(cross(toward, reference));
	const vec3f above = cross(toward, across);
	const std::array ignored{ listener.owner, listener.body, unheard[0], unheard[1] };

	aabb reach_box;
	for (std::size_t axis = 0; axis < 3; ++axis) {
		reach_box.min[axis] = std::min(ear[axis], center[axis]) - source_radius;
		reach_box.max[axis] = std::max(ear[axis], center[axis]) + source_radius;
	}
	physics::targets_within(d.ray_targets, reach_box, d.targets_in_reach);
	const std::span<const physics::ray_target> targets = d.targets_in_reach;

	std::uint32_t blocked = physics::nearest_hit(targets, ear, center, ignored) ? 1 : 0;
	for (std::uint32_t ray = 0; ray < rim_rays; ++ray) {
		const angle around = degrees(360.f) * (static_cast<float>(ray) / static_cast<float>(rim_rays));
		const vec3f outward = across * cos(around) + above * sin(around);
		const auto surface = physics::nearest_hit(targets, center, center + outward * source_radius, ignored);
		const length reach = surface ? std::max(surface->distance - surface_clearance, length{}) : source_radius;
		const vec3<position> rim = center + outward * reach;
		blocked += physics::nearest_hit(targets, ear, rim, ignored) ? 1 : 0;
	}
	return static_cast<float>(blocked) / static_cast<float>(rim_rays + 1);
}

auto gse::audio::locate(data& d, const id emitter, const std::optional<scene_listener>& listener, const read<physics::transform_component>& transforms) -> std::optional<located_emitter> {
	if (!emitter.exists()) {
		return std::nullopt;
	}
	const auto* source = transforms.find(emitter);
	if (!listener || source == nullptr) {
		log::println(
			log::level::warning,
			log::category::general,
			"audio: a sound at emitter {} was dropped because {}",
			emitter,
			!listener ? "the scene has no listener with a transform" : "the emitter has no transform"
		);
		return std::nullopt;
	}
	const std::array unheard{ emitter, id{} };
	const float occluded = blocked_share(d, *listener, unheard, source->position);
	return located_emitter{
		.owner = emitter,
		.unheard = unheard,
		.position = source->position,
		.occluded = occluded,
		.at = place(listener->transform, { .position = source->position, .occluded = occluded }, d.listening),
	};
}

auto gse::audio::locate(data& d, const physics::contact_event& contact, const scene_listener& listener) -> located_emitter {
	const std::array unheard{ contact.body_a, contact.body_b };
	const float occluded = blocked_share(d, listener, unheard, contact.point);
	return {
		.unheard = unheard,
		.position = contact.point,
		.occluded = occluded,
		.at = place(listener.transform, { .position = contact.point, .occluded = occluded }, d.listening),
	};
}

auto gse::audio::contact_voices(const data& d) -> std::uint32_t {
	return static_cast<std::uint32_t>(std::ranges::count_if(d.slots, [](const voice_slot& slot) {
		return slot.active && slot.placed && !slot.emitter.exists();
	}));
}

auto gse::audio::delivered_impulse(const physics::contact_event& contact, const read<physics::motion_component>& motions) -> impulse {
	const auto mass_at = [&](const id body) {
		const auto* motion = motions.find(body);
		return motion != nullptr ? physics::mass_of(*motion) : mass{};
	};
	return reduced_mass(mass_at(contact.body_a), mass_at(contact.body_b)) * contact.relative_speed;
}

auto gse::audio::sounds(const physics::contact_event& contact, const write<contact_sound_component>& contact_sounds, const write<acoustic_material_component>& materials, const read<physics::motion_component>& motions) -> bool {
	const auto heard_on = [&](const id body) {
		return contact_sounds.find(body) != nullptr || materials.find(body) != nullptr;
	};
	return (heard_on(contact.body_a) || heard_on(contact.body_b)) && audible(contact.relative_speed, delivered_impulse(contact, motions));
}

auto gse::audio::modal_voices(const data& d) -> std::uint32_t {
	return static_cast<std::uint32_t>(std::ranges::count_if(d.slots, [](const voice_slot& slot) {
		return slot.active && slot.material != nullptr && !slot.rubbing.exists();
	}));
}

auto gse::audio::rub_voices(const data& d) -> std::uint32_t {
	return static_cast<std::uint32_t>(std::ranges::count_if(d.slots, [](const voice_slot& slot) {
		return slot.active && slot.rubbing.exists();
	}));
}

auto gse::audio::rub_body(data& d, const rubbed_body& rubbed, const contact_scene& scene, std::uint32_t& started) -> std::string_view {
	const auto& [contact, body, other, transform, heard, ramp_frames] = rubbed;
	const std::uint64_t held_until = scene.physics_step + rub_release_steps;
	const auto update_for = [&](const std::uint32_t index) {
		return rub_voice{
			.slot = index,
			.level = heard.level,
			.cutoff = heard.cutoff,
			.wobble = heard.wobble,
			.wobble_depth = heard.wobble_depth,
			.ramp_frames = ramp_frames,
		};
	};

	const auto sounding = std::ranges::find_if(d.slots, [&](const voice_slot& slot) {
		return slot.active && slot.emitter == body && slot.rubbing == other;
	});
	if (sounding != d.slots.end()) {
		const auto index = static_cast<std::uint32_t>(std::distance(d.slots.begin(), sounding));
		if (!d.output->submit(update_for(index))) {
			return "dropped";
		}
		if (heard.level <= 0.f) {
			return "too slow";
		}
		sounding->rung_out_step = held_until;
		return "rubbed";
	}
	if (heard.level <= 0.f) {
		return "too slow";
	}

	const auto material = asset::try_get<acoustic_material>(scene.assets, scene.materials.find(body)->material);
	if (!material.valid()) {
		return "material not loaded";
	}
	if (!scene.listener) {
		return "no listener";
	}
	if (started == contact_starts_per_frame) {
		return "over the frame limit";
	}
	if (rub_voices(d) >= rub_voice_limit) {
		return "over the voice limit";
	}

	auto resolved = material.resolve();
	const auto* collider = scene.colliders.find(body);
	const auto* shape = collider != nullptr ? std::get_if<physics::box_shape>(&collider->shape.value) : nullptr;
	const acoustic_box* box = shape != nullptr ? resolved->box_of(shape->size) : nullptr;

	const auto source = locate(d, body, scene.listener, scene.transforms);
	start_rubbing command{
		.least_decay = contact_damping(),
		.at = source->at,
	};
	if (box != nullptr && !box->rings.storage.empty()) {
		command.rings = box->rings.storage;
		command.drive = rub_drive(*box, transform, contact.normal);
	}
	const voice_handle handle = start_voice(d, command, nullptr, source);
	if (handle.generation == 0) {
		return "dropped";
	}
	auto& slot = d.slots[handle.index];
	slot.material = std::move(resolved);
	slot.rubbing = other;
	slot.rung_out_step = held_until;
	d.output->submit(update_for(handle.index));
	++started;
	return "started";
}

auto gse::audio::rub_fade_frames() -> std::uint32_t {
	const time fade = milliseconds(5.f);
	return static_cast<std::uint32_t>(std::lround(sample_rate() * fade));
}

auto gse::audio::fade_rubs(data& d, const std::uint64_t held_before) -> void {
	const std::uint32_t ramp_frames = rub_fade_frames();
	for (std::uint32_t index = 0; index < voice_capacity; ++index) {
		const auto& slot = d.slots[index];
		if (slot.active && slot.rubbing.exists() && slot.rung_out_step < held_before) {
			d.output->submit(fade_rub{ .slot = index, .ramp_frames = ramp_frames });
		}
	}
}

auto gse::audio::rub_contacts(data& d, const std::span<const physics::moving_contact> moving, const contact_scene& scene) -> void {
	const std::uint32_t ramp_frames = rub_fade_frames();
	const std::uint64_t held_until = scene.physics_step + rub_release_steps;

	std::uint32_t started = 0;
	for (const auto& contact : moving) {
		const auto* on_a = scene.materials.find(contact.body_a);
		const auto* sound = on_a != nullptr ? on_a : scene.materials.find(contact.body_b);
		if (sound == nullptr) {
			continue;
		}
		const id body = on_a != nullptr ? contact.body_a : contact.body_b;
		const id other = on_a != nullptr ? contact.body_b : contact.body_a;
		const auto* transform = scene.transforms.find(body);
		if (transform == nullptr) {
			continue;
		}

		const vec3<position> center = transform->position;
		const rub heard = rub_of(contact, magnitude(contact.point - center), sound->gain);
		const std::string_view outcome = rub_body(
			d,
			{
				.contact = contact,
				.body = body,
				.other = other,
				.transform = *transform,
				.heard = heard,
				.ramp_frames = ramp_frames,
			},
			scene,
			started
		);

		if (d.trace_contacts && heard.level > 0.f) {
			log::println(
				log::category::general,
				"audio: rub step {} bodies {} {} slip {:.5f} roll {:.5f} load {:.3f} kind {} level {:.6f} cutoff {:.1f:Hz} outcome {}, sink frame {}",
				contact.step,
				body,
				other,
				contact.slip_speed,
				contact.roll_speed,
				contact.load,
				heard.kind,
				heard.level,
				heard.cutoff,
				outcome,
				d.sink.samples.size() / output_channels
			);
		}
	}

	fade_rubs(d, held_until - physics::contact_tracker::release_steps);
	d.peak_rub_voices = std::max(d.peak_rub_voices, rub_voices(d));
}

auto gse::audio::trace_modes(const id body, const acoustic_material& material, const acoustic_box& box) -> void {
	const auto& properties = material.properties();
	log::println(
		log::category::general,
		"audio: modal voice body {} material {} box {:.4f} density {:.2f} young modulus {:.4f:GPa} poisson ratio {:.4f} mass damping {:.5f} stiffness damping {:.4f:ns}, {} modes",
		body,
		material.id(),
		box.size,
		properties.mass_density,
		properties.young_modulus,
		properties.poisson_ratio,
		properties.mass_damping,
		properties.stiffness_damping,
		box.rings.storage.size()
	);
	for (const auto [index, ring, axis_share] : std::views::zip(std::views::iota(0), box.rings.storage, box.axis_shares.storage)) {
		log::println(
			log::category::general,
			"audio: modal mode {} body {} frequency {:.4f:Hz} decay {:.5f} axis share {:.5f}",
			index,
			body,
			ring.pitch,
			ring.decay,
			axis_share
		);
	}
}

auto gse::audio::ring_body(data& d, const heard_contact& heard, const id body, const contact_scene& scene, std::uint32_t& started) -> std::string_view {
	const auto& [contact, delivered] = heard;
	const auto& sound = *scene.materials.find(body);
	const auto material = asset::try_get<acoustic_material>(scene.assets, sound.material);
	if (!material.valid()) {
		return "material not loaded";
	}

	const auto* transform = scene.transforms.find(body);
	const auto* collider = scene.colliders.find(body);
	const auto* shape = collider != nullptr ? std::get_if<physics::box_shape>(&collider->shape.value) : nullptr;
	if (transform == nullptr || shape == nullptr) {
		return "not a box";
	}

	auto resolved = material.resolve();
	const acoustic_box* box = resolved->box_of(shape->size);
	if (box == nullptr || box->rings.storage.empty()) {
		if (d.trace_contacts) {
			log::println(
				log::category::general,
				"audio: body {} has no baked modes for a box of {:.4f}; add a [Box.*] section with that size to material {}",
				body,
				shape->size,
				resolved->id()
			);
		}
		return "no baked modes";
	}

	const strike_drive drive = drive_of({
		.material = *resolved,
		.box = *box,
		.body = *transform,
		.contact = contact,
		.delivered = delivered,
		.gain = sound.gain,
	});
	const std::uint64_t rung_out_step = scene.physics_step + static_cast<std::uint64_t>(std::ceil(ring_time(*box) / system_clock::fixed_dt()));

	const auto ringing = std::ranges::find_if(d.slots, [body](const voice_slot& slot) {
		return slot.active && slot.material != nullptr && slot.emitter == body && !slot.rubbing.exists();
	});
	if (ringing != d.slots.end()) {
		const auto index = static_cast<std::uint32_t>(std::distance(d.slots.begin(), ringing));
		if (!d.output->submit(strike_voice{ .slot = index, .drive = drive })) {
			return "dropped";
		}
		ringing->rung_out_step = rung_out_step;
		return "rang";
	}

	if (!scene.listener) {
		return "no listener";
	}
	if (started == contact_starts_per_frame) {
		return "over the frame limit";
	}
	if (modal_voices(d) >= modal_voice_limit) {
		return "over the voice limit";
	}

	const auto source = locate(d, body, scene.listener, scene.transforms);
	const start_ringing command{
		.rings = box->rings.storage,
		.drive = drive,
		.at = source->at,
	};
	const voice_handle handle = start_voice(d, command, nullptr, source);
	if (handle.generation == 0) {
		return "dropped";
	}
	if (d.trace_contacts) {
		trace_modes(body, *resolved, *box);
	}
	auto& slot = d.slots[handle.index];
	slot.material = std::move(resolved);
	slot.rung_out_step = rung_out_step;
	++started;
	return "started";
}

auto gse::audio::start_contact_voices(data& d, const std::span<const physics::contact_event> contacts, const contact_scene& scene) -> void {
	if (contacts.empty()) {
		return;
	}
	const auto& [listener, physics_step, assets_s, contact_sounds, materials, transforms, colliders, motions] = scene;

	auto& loudest = d.loudest_contacts;
	loudest.clear();
	for (const auto& contact : contacts) {
		loudest.push_back({
			.contact = contact,
			.delivered = delivered_impulse(contact, motions),
		});
	}
	std::ranges::stable_sort(loudest, std::ranges::greater{}, &heard_contact::delivered);

	std::uint32_t started = 0;
	std::uint32_t sounding = contact_voices(d);
	for (const auto& heard : loudest) {
		const auto& [contact, delivered] = heard;
		const auto* on_a = contact_sounds.find(contact.body_a);
		const auto* sound = on_a != nullptr ? on_a : contact_sounds.find(contact.body_b);
		const bool rings_a = materials.find(contact.body_a) != nullptr;
		const bool rings_b = materials.find(contact.body_b) != nullptr;

		std::string_view outcome = "started";
		if (!audible(contact.relative_speed, delivered)) {
			outcome = "too soft";
		}
		else if (rings_a || rings_b) {
			if (rings_a) {
				outcome = ring_body(d, heard, contact.body_a, scene, started);
			}
			if (rings_b) {
				outcome = ring_body(d, heard, contact.body_b, scene, started);
			}
		}
		else if (sound == nullptr) {
			outcome = "no contact sound";
		}
		else if (!listener) {
			outcome = "no listener";
		}
		else if (started == contact_starts_per_frame) {
			outcome = "over the frame limit";
		}
		else if (sounding >= contact_voice_limit) {
			outcome = "over the voice limit";
		}
		else if (const auto clip = asset::try_get<audio_clip>(assets_s, sound->clip); !clip.valid()) {
			outcome = "clip not loaded";
		}
		else {
			const auto source = locate(d, contact, *listener);
			auto samples = clip.resolve();
			const start_clip command{
				.samples = samples->samples(),
				.gain = percentage<float>::fraction(sound->gain.value() * contact_loudness(contact.relative_speed)),
				.at = source.at,
			};
			if (start_voice(d, command, std::move(samples), source).generation == 0) {
				outcome = "dropped";
			}
			else {
				++started;
				++sounding;
			}
		}

		if (d.trace_contacts) {
			log::println(
				log::category::general,
				"audio: contact step {} bodies {} {} impulse {:.5f} speed {:.5f} point {:.4f} outcome {}, sink frame {}, physics step {}",
				contact.step,
				contact.body_a,
				contact.body_b,
				contact.impulse,
				contact.relative_speed,
				contact.point,
				outcome,
				d.sink.samples.size() / output_channels,
				physics_step
			);
		}
	}
}

auto gse::audio::start_voice(data& d, voice_command command, std::shared_ptr<const audio_clip> clip, const std::optional<located_emitter>& source) -> voice_handle {
	if (silent(d)) {
		return {};
	}
	if (d.free_slots.empty()) {
		log::println(log::level::warning, log::category::general, "audio: all {} voices are in use, so a sound was dropped", voice_capacity);
		return {};
	}

	const std::uint32_t index = d.free_slots.back();
	std::visit(
		[index](auto& addressed) {
			addressed.slot = index;
		},
		command
	);
	if (!d.output->submit(command)) {
		log::println(log::level::warning, log::category::general, "audio: the mixer's command queue is full, so a sound was dropped");
		return {};
	}

	d.free_slots.pop_back();
	auto& slot = d.slots[index];
	++slot.generation;
	slot.active = true;
	slot.clip = std::move(clip);
	slot.placed = source.has_value();
	slot.emitter = source ? source->owner : id{};
	slot.rubbing = {};
	slot.unheard = source ? source->unheard : std::array<id, 2>{};
	slot.emitter_position = source ? source->position : vec3<current_position>{};
	slot.occluded = source ? source->occluded : 0.f;

	++d.voices_started;
	++d.sounding_voices;
	d.peak_voices = std::max(d.peak_voices, d.sounding_voices);

	return {
		.index = index,
		.generation = slot.generation,
	};
}

auto gse::audio::valid_voice(const data& d, const voice_handle handle) -> bool {
	const auto& slot = d.slots[handle.index];
	return slot.active && slot.generation == handle.generation;
}

auto gse::audio::stop_slot(data& d, const std::uint32_t index) -> bool {
	if (!d.output->submit(stop_voice{ .slot = index })) {
		log::println(log::level::warning, log::category::general, "audio: the mixer's command queue is full, so a stop was dropped");
		return false;
	}
	d.slots[index].active = false;
	--d.sounding_voices;
	return true;
}

auto gse::audio::stop_orphaned_voices(data& d, const read<physics::transform_component>& transforms) -> void {
	for (std::uint32_t index = 0; index < voice_capacity; ++index) {
		const auto& slot = d.slots[index];
		if (slot.active && slot.emitter.exists() && transforms.find(slot.emitter) == nullptr) {
			stop_slot(d, index);
		}
	}
}

auto gse::audio::stop_rung_out_voices(data& d, const std::uint64_t physics_step) -> void {
	for (std::uint32_t index = 0; index < voice_capacity; ++index) {
		const auto& slot = d.slots[index];
		if (slot.active && slot.material != nullptr && physics_step >= slot.rung_out_step) {
			stop_slot(d, index);
		}
	}
}

auto gse::audio::move_placed_voices(data& d, const scene_listener& listener, const read<physics::transform_component>& transforms) -> void {
	const time occlusion_swing = milliseconds(500.f);

	const auto span = system_clock::fixed_dt() * static_cast<float>(system_clock::fixed_steps_this_frame());
	const auto ramp_frames = static_cast<std::uint32_t>(std::lround(sample_rate() * span));
	const auto occlusion_step = static_cast<float>(span / occlusion_swing);

	for (std::uint32_t index = 0; index < voice_capacity; ++index) {
		auto& slot = d.slots[index];
		if (!slot.active || !slot.placed) {
			continue;
		}
		vec3<current_position> position = slot.emitter_position;
		if (slot.emitter.exists()) {
			const auto* source = transforms.find(slot.emitter);
			if (source == nullptr) {
				continue;
			}
			position = source->position;
		}
		const vec3<velocity> emitter_velocity = (position - slot.emitter_position) / span;
		slot.emitter_position = position;
		const float blocked = blocked_share(d, listener, slot.unheard, position);
		slot.occluded += std::clamp(blocked - slot.occluded, -occlusion_step, occlusion_step);
		const emission heard{
			.position = position,
			.velocity = emitter_velocity,
			.occluded = slot.occluded,
		};
		d.output->submit(move_voice{
			.slot = index,
			.at = place(listener.transform, heard, d.listening),
			.ramp_frames = ramp_frames,
		});
	}
}

auto gse::audio::render_fixed_steps(data& d) -> void {
	const int steps = system_clock::fixed_steps_this_frame();
	if (steps == 0) {
		return;
	}
	auto& samples = d.sink.samples;
	const std::size_t rendered = samples.size();
	samples.resize(rendered + static_cast<std::size_t>(steps) * d.frames_per_step * output_channels);
	d.output->render(std::span(samples).subspan(rendered));
}

auto gse::audio::run(data& d, const shared_view<physics::data> phys_s, const shared_view<asset::data> assets_s, const channel_read<play_request, play_tone_request, stop_request, offline_sink_request, physics::contact_event> requests_in, read<listener_component> listeners, write<contact_sound_component> contact_sounds, write<acoustic_material_component> materials, read<physics::transform_component> transforms, read<physics::collision_component> colliders, read<physics::motion_component> motions) -> async::task<> {
	for (const auto& req : requests_in.of<offline_sink_request>()) {
		d.sink.path = req.path;
	}

	for (const id owner : contact_sounds.drain(component_event::added)) {
		if (const auto* sound = contact_sounds.find(owner)) {
			asset::try_get<audio_clip>(assets_s, sound->clip);
		}
	}

	for (const id owner : materials.drain(component_event::added)) {
		if (const auto* sound = materials.find(owner)) {
			asset::try_get<acoustic_material>(assets_s, sound->material);
		}
	}

	std::uint32_t finished = 0;
	while (d.output->take_finished(finished)) {
		auto& slot = d.slots[finished];
		if (slot.active) {
			slot.active = false;
			--d.sounding_voices;
		}
		slot.clip.reset();
		slot.material.reset();
		d.free_slots.push_back(finished);
	}

	for (const auto& req : requests_in.of<stop_request>()) {
		if (valid_voice(d, req.handle)) {
			stop_slot(d, req.handle.index);
		}
	}

	stop_orphaned_voices(d, transforms);
	stop_rung_out_voices(d, phys_s.step_index);

	std::optional<scene_listener> listener;
	if (!listeners.empty()) {
		const id owner = listeners.owner_ids().front();
		if (const auto* transform = transforms.find(owner)) {
			listener = {
				.owner = owner,
				.body = listeners[0].body,
				.transform = *transform,
			};
		}
	}

	const auto placed = [](const auto& sound) {
		return sound.emitter.exists();
	};
	const bool sounding_placed = std::ranges::any_of(d.slots, [](const voice_slot& slot) {
		return slot.active && slot.placed;
	});
	const auto contacts = requests_in.of<physics::contact_event>();
	const bool moving_placed = sounding_placed && system_clock::fixed_steps_this_frame() > 0;
	const bool contact_sounding = std::ranges::any_of(contacts, [&](const physics::contact_event& contact) {
		return sounds(contact, contact_sounds, materials, motions);
	});
	const std::span<const physics::moving_contact> all_moving = phys_s.moving_contacts;
	const std::span<const physics::moving_contact> moving = d.rub_sounds ? all_moving : all_moving.first(0);
	const bool stepped = phys_s.step_index != d.rubbed_step;
	const bool rubbing = stepped && std::ranges::any_of(moving, [&](const physics::moving_contact& contact) {
		return materials.find(contact.body_a) != nullptr || materials.find(contact.body_b) != nullptr;
	});
	const bool starting_placed = std::ranges::any_of(requests_in.of<play_request>(), placed) || std::ranges::any_of(requests_in.of<play_tone_request>(), placed) || contact_sounding || rubbing;
	if (listener && (moving_placed || starting_placed)) {
		trace::scope_guard _{ trace_id<"audio::occlusion">() };
		physics::gather_ray_targets(transforms, colliders, phys_s.hulls, d.ray_targets);
		if (moving_placed) {
			move_placed_voices(d, *listener, transforms);
		}
	}

	for (const auto& req : requests_in.of<play_request>()) {
		if (!req.clip.valid()) {
			log::println(log::level::warning, log::category::general, "audio: clip {} is {}, so its play request was dropped", req.clip.id(), req.clip.state());
			req.promise.fulfill({});
			continue;
		}
		const auto source = locate(d, req.emitter, listener, transforms);
		if (req.emitter.exists() && !source) {
			req.promise.fulfill({});
			continue;
		}
		auto clip = req.clip.resolve();
		const start_clip command{
			.samples = clip->samples(),
			.loop = req.loop,
			.gain = req.gain,
			.at = source.transform(&located_emitter::at),
		};
		req.promise.fulfill(start_voice(d, command, std::move(clip), source));
	}

	for (const auto& req : requests_in.of<play_tone_request>()) {
		const auto source = locate(d, req.emitter, listener, transforms);
		if (req.emitter.exists() && !source) {
			req.promise.fulfill({});
			continue;
		}
		const start_tone command{
			.pitch = req.pitch,
			.gain = req.gain,
			.at = source.transform(&located_emitter::at),
		};
		req.promise.fulfill(start_voice(d, command, nullptr, source));
	}

	const contact_scene scene{
		.listener = listener,
		.physics_step = phys_s.step_index,
		.assets = assets_s,
		.contact_sounds = contact_sounds,
		.materials = materials,
		.transforms = transforms,
		.colliders = colliders,
		.motions = motions,
	};
	start_contact_voices(d, contacts, scene);
	if (stepped) {
		d.rubbed_step = phys_s.step_index;
		rub_contacts(d, moving, scene);
	}
	else if (system_clock::fixed_steps_this_frame() > 0) {
		fade_rubs(d, std::numeric_limits<std::uint64_t>::max());
	}

	const std::uint32_t ringing = modal_voices(d);
	d.peak_modal_voices = std::max(d.peak_modal_voices, ringing);
	d.frames_at_modal_limit += ringing == modal_voice_limit ? 1 : 0;
	++d.frames_run;

	if (d.mode == launch::audio_output::offline) {
		render_fixed_steps(d);
	}

	return {};
}

auto gse::audio::write_sink(const data& d) -> void {
	if (d.voices_started == 0) {
		log::println(log::category::general, "audio: offline render had no voice, so no WAV was written");
		return;
	}

	const auto& [path, samples] = d.sink;
	if (path.empty()) {
		log::println(log::level::error, log::category::general, "audio: offline render mixed {} frames but no sink was named, so no WAV was written", samples.size() / output_channels);
		return;
	}

	const auto written = write_wav(path, samples, { .channels = output_channels, .frame_rate = output_frame_rate });
	if (!written) {
		log::println(log::level::error, log::category::general, "audio: offline render could not write {}: {}", path.generic_display_string(), written.error());
		return;
	}

	const std::string_view bytes(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(float));
	log::println(
		log::category::general,
		"audio: offline render wrote {}, {} frames, sample hash {:#018x}, {} voices started, peak {} voices",
		path.generic_display_string(),
		samples.size() / output_channels,
		stable_id(bytes),
		d.voices_started,
		d.peak_voices
	);
}

auto gse::audio::shutdown(data& d) -> void {
	if (d.peak_modal_voices > 0) {
		log::println(
			log::category::general,
			"audio: modal voices peaked at {} of {}, at the limit on {} of {} frames",
			d.peak_modal_voices,
			modal_voice_limit,
			d.frames_at_modal_limit,
			d.frames_run
		);
	}
	if (d.peak_rub_voices > 0) {
		log::println(log::category::general, "audio: rub voices peaked at {} of {}", d.peak_rub_voices, rub_voice_limit);
	}
	if (d.device) {
		stop_and_report(*d.device, d.peak_voices);
		d.device.reset();
	}
	if (d.mode == launch::audio_output::offline) {
		write_sink(d);
	}
}

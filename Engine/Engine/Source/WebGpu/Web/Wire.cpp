module;

#include <webgpu/webgpu.h>

module gse.webgpu:wire_impl;

import std;

import gse.assert;

extern "C" auto gse_host_gpu(const std::uint32_t* words, std::int32_t count, const void* tail, std::int32_t tail_bytes) -> void;
extern "C" auto gse_host_gpu_poll(std::uint32_t* out, std::int32_t max) -> std::int32_t;

namespace gse::webgpu::wire {
	enum class op : std::uint32_t {
		request_adapter = 1,
		request_device,
		get_queue,
		create_surface,
		configure_surface,
		unconfigure_surface,
		get_current_texture,
		present,
		create_buffer,
		create_texture,
		create_view,
		create_sampler,
		create_bind_group_layout,
		create_bind_group,
		create_pipeline_layout,
		create_shader_module,
		create_compute_pipeline,
		create_render_pipeline,
		create_command_encoder,
		begin_render_pass,
		begin_compute_pass,
		finish,
		submit,
		on_submitted_work_done,
		write_buffer,
		write_texture,
		map_read,
		unmap,
		push_error_scope,
		pop_error_scope,
		clear_buffer,
		copy_buffer_to_buffer,
		copy_texture_to_buffer,
		copy_texture_to_texture,
		push_debug_group,
		pop_debug_group,
		pass_end,
		set_pipeline,
		set_bind_group,
		set_index_buffer,
		set_viewport,
		set_scissor,
		draw,
		draw_indexed,
		draw_indexed_indirect,
		dispatch,
		dispatch_indirect,
		release,
	};

	enum class event : std::uint32_t {
		adapter = 1,
		device,
		mapped,
		work_done,
		scope_popped,
		uncaptured_error,
		device_lost,
	};

	constexpr std::size_t limit_count = 32;

	struct adapter_state {
		std::string vendor;
		std::string architecture;
		std::string device;
		std::string description;
		std::uint32_t vendor_id = 0;
		std::uint32_t device_id = 0;
		WGPULimits limits{};
		std::vector<WGPUFeatureName> features;
		WGPUTextureFormat preferred = WGPUTextureFormat_BGRA8Unorm;
	};

	struct device_state {
		WGPULimits limits{};
		WGPUDeviceLostCallbackInfo lost{};
		WGPUUncapturedErrorCallbackInfo uncaptured{};
	};

	struct mapping {
		std::vector<std::byte> bytes;
		std::size_t offset = 0;
	};

	using pending = std::variant<
		WGPURequestAdapterCallbackInfo,
		std::pair<WGPURequestDeviceCallbackInfo, std::uint32_t>,
		std::pair<WGPUBufferMapCallbackInfo, std::uint32_t>,
		WGPUQueueWorkDoneCallbackInfo,
		WGPUPopErrorScopeCallbackInfo
	>;

	struct state {
		std::atomic<std::uint32_t> next_id{ 1 };
		std::mutex mutex;
		std::unordered_map<std::uint32_t, adapter_state> adapters;
		std::unordered_map<std::uint32_t, device_state> devices;
		std::unordered_map<std::uint32_t, mapping> mappings;
		std::unordered_map<std::uint64_t, pending> futures;
		std::uint64_t next_future = 1;
		std::vector<std::uint32_t> inbox = std::vector<std::uint32_t>(1u << 18);
	};

	auto shared() -> state&;

	class writer {
	public:
		explicit writer(
			op code
		);

		auto u32(
			std::uint32_t value
		) -> writer&;

		auto i32(
			std::int32_t value
		) -> writer&;

		auto u64(
			std::uint64_t value
		) -> writer&;

		auto f32(
			float value
		) -> writer&;

		auto f64(
			double value
		) -> writer&;

		auto str(
			WGPUStringView view
		) -> writer&;

		auto bytes(
			const void* data,
			std::size_t size
		) -> writer&;

		template <typename H>
		auto id(
			H handle
		) -> writer&;

		auto tail(
			const void* data,
			std::size_t size
		) -> writer&;

		auto send() -> void;

	private:
		std::vector<std::uint32_t>& m_words;
		const void* m_tail = nullptr;
		std::size_t m_tail_bytes = 0;
	};

	template <typename H>
	[[nodiscard]] auto id_of(
		H handle
	) -> std::uint32_t;

	template <typename H>
	[[nodiscard]] auto fresh() -> H;

	[[nodiscard]] auto view_of(
		WGPUStringView view
	) -> std::string_view;

	[[nodiscard]] auto future_for(
		pending callback
	) -> std::uint64_t;

	auto texture_copy(
		writer& out,
		const WGPUTexelCopyTextureInfo& info
	) -> void;

	auto extent(
		writer& out,
		const WGPUExtent3D& size
	) -> void;

	auto limits(
		writer& out,
		const WGPULimits& value
	) -> void;

	auto programmable(
		writer& out,
		WGPUShaderModule module,
		WGPUStringView entry,
		std::size_t constant_count,
		const WGPUConstantEntry* constants
	) -> void;

	class reader {
	public:
		reader(
			const std::uint32_t* words,
			std::size_t count
		);

		[[nodiscard]] auto u32() -> std::uint32_t;

		[[nodiscard]] auto u64() -> std::uint64_t;

		[[nodiscard]] auto str() -> std::string;

		[[nodiscard]] auto limits() -> WGPULimits;

	private:
		const std::uint32_t* m_words;
		std::size_t m_count;
		std::size_t m_at = 0;
	};

	auto dispatch(
		event kind,
		reader& in
	) -> void;
}

auto gse::webgpu::wire::shared() -> state& {
	static state instance;
	return instance;
}

gse::webgpu::wire::writer::writer(const op code) : m_words([] -> std::vector<std::uint32_t>& {
	thread_local std::vector<std::uint32_t> words;
	return words;
}()) {
	m_words.clear();
	m_words.push_back(std::to_underlying(code));
	m_words.push_back(0);
}

auto gse::webgpu::wire::writer::u32(const std::uint32_t value) -> writer& {
	m_words.push_back(value);
	return *this;
}

auto gse::webgpu::wire::writer::i32(const std::int32_t value) -> writer& {
	return u32(std::bit_cast<std::uint32_t>(value));
}

auto gse::webgpu::wire::writer::u64(const std::uint64_t value) -> writer& {
	m_words.push_back(static_cast<std::uint32_t>(value));
	m_words.push_back(static_cast<std::uint32_t>(value >> 32));
	return *this;
}

auto gse::webgpu::wire::writer::f32(const float value) -> writer& {
	return u32(std::bit_cast<std::uint32_t>(value));
}

auto gse::webgpu::wire::writer::f64(const double value) -> writer& {
	return u64(std::bit_cast<std::uint64_t>(value));
}

auto gse::webgpu::wire::writer::str(const WGPUStringView view) -> writer& {
	const auto text = view_of(view);
	return bytes(text.data(), text.size());
}

auto gse::webgpu::wire::writer::bytes(const void* data, const std::size_t size) -> writer& {
	m_words.push_back(static_cast<std::uint32_t>(size));
	const auto at = m_words.size();
	m_words.resize(at + (size + 3) / 4, 0u);
	if (size != 0) {
		std::memcpy(m_words.data() + at, data, size);
	}
	return *this;
}

template <typename H>
auto gse::webgpu::wire::writer::id(const H handle) -> writer& {
	return u32(id_of(handle));
}

auto gse::webgpu::wire::writer::tail(const void* data, const std::size_t size) -> writer& {
	m_words.push_back(static_cast<std::uint32_t>(size));
	m_tail = data;
	m_tail_bytes = size;
	return *this;
}

auto gse::webgpu::wire::writer::send() -> void {
	m_words[1] = static_cast<std::uint32_t>(m_words.size() + (m_tail_bytes + 3) / 4);
	gse_host_gpu(m_words.data(), static_cast<std::int32_t>(m_words.size()), m_tail, static_cast<std::int32_t>(m_tail_bytes));
}

template <typename H>
auto gse::webgpu::wire::id_of(const H handle) -> std::uint32_t {
	return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(handle));
}

template <typename H>
auto gse::webgpu::wire::fresh() -> H {
	return reinterpret_cast<H>(static_cast<std::uintptr_t>(shared().next_id.fetch_add(1, std::memory_order_relaxed)));
}

auto gse::webgpu::wire::view_of(const WGPUStringView view) -> std::string_view {
	if (!view.data) {
		return {};
	}
	if (view.length == WGPU_STRLEN) {
		return std::string_view(view.data);
	}
	return std::string_view(view.data, view.length);
}

auto gse::webgpu::wire::future_for(pending callback) -> std::uint64_t {
	auto& s = shared();
	const std::lock_guard _(s.mutex);
	const auto future = s.next_future++;
	s.futures.emplace(future, std::move(callback));
	return future;
}

auto gse::webgpu::wire::texture_copy(writer& out, const WGPUTexelCopyTextureInfo& info) -> void {
	out.id(info.texture).u32(info.mipLevel).u32(info.origin.x).u32(info.origin.y).u32(info.origin.z).u32(info.aspect);
}

auto gse::webgpu::wire::extent(writer& out, const WGPUExtent3D& size) -> void {
	out.u32(size.width).u32(size.height).u32(size.depthOrArrayLayers);
}

auto gse::webgpu::wire::limits(writer& out, const WGPULimits& value) -> void {
	out.u64(value.maxTextureDimension1D).u64(value.maxTextureDimension2D).u64(value.maxTextureDimension3D).u64(value.maxTextureArrayLayers)
		.u64(value.maxBindGroups).u64(value.maxBindGroupsPlusVertexBuffers).u64(value.maxBindingsPerBindGroup)
		.u64(value.maxDynamicUniformBuffersPerPipelineLayout).u64(value.maxDynamicStorageBuffersPerPipelineLayout)
		.u64(value.maxSampledTexturesPerShaderStage).u64(value.maxSamplersPerShaderStage).u64(value.maxStorageBuffersPerShaderStage)
		.u64(value.maxStorageTexturesPerShaderStage).u64(value.maxUniformBuffersPerShaderStage).u64(value.maxUniformBufferBindingSize)
		.u64(value.maxStorageBufferBindingSize).u64(value.minUniformBufferOffsetAlignment).u64(value.minStorageBufferOffsetAlignment)
		.u64(value.maxVertexBuffers).u64(value.maxBufferSize).u64(value.maxVertexAttributes).u64(value.maxVertexBufferArrayStride)
		.u64(value.maxInterStageShaderVariables).u64(value.maxColorAttachments).u64(value.maxColorAttachmentBytesPerSample)
		.u64(value.maxComputeWorkgroupStorageSize).u64(value.maxComputeInvocationsPerWorkgroup).u64(value.maxComputeWorkgroupSizeX)
		.u64(value.maxComputeWorkgroupSizeY).u64(value.maxComputeWorkgroupSizeZ).u64(value.maxComputeWorkgroupsPerDimension)
		.u64(value.maxImmediateSize);
}

auto gse::webgpu::wire::programmable(writer& out, const WGPUShaderModule module, const WGPUStringView entry, const std::size_t constant_count, const WGPUConstantEntry* constants) -> void {
	out.id(module).str(entry).u32(static_cast<std::uint32_t>(constant_count));
	for (std::size_t i = 0; i < constant_count; ++i) {
		out.str(constants[i].key).f64(constants[i].value);
	}
}

gse::webgpu::wire::reader::reader(const std::uint32_t* words, const std::size_t count) : m_words(words), m_count(count) {}

auto gse::webgpu::wire::reader::u32() -> std::uint32_t {
	assert(m_at < m_count, "webgpu wire: event read past its end");
	return m_words[m_at++];
}

auto gse::webgpu::wire::reader::u64() -> std::uint64_t {
	const auto low = static_cast<std::uint64_t>(u32());
	return low | static_cast<std::uint64_t>(u32()) << 32;
}

auto gse::webgpu::wire::reader::str() -> std::string {
	const auto size = u32();
	assert(m_at + (size + 3) / 4 <= m_count, "webgpu wire: event string runs past its end");
	std::string text(reinterpret_cast<const char*>(m_words + m_at), size);
	m_at += (size + 3) / 4;
	return text;
}

auto gse::webgpu::wire::reader::limits() -> WGPULimits {
	WGPULimits out{};
	const auto narrow = [this] {
		return static_cast<std::uint32_t>(u64());
	};
	out.maxTextureDimension1D = narrow();
	out.maxTextureDimension2D = narrow();
	out.maxTextureDimension3D = narrow();
	out.maxTextureArrayLayers = narrow();
	out.maxBindGroups = narrow();
	out.maxBindGroupsPlusVertexBuffers = narrow();
	out.maxBindingsPerBindGroup = narrow();
	out.maxDynamicUniformBuffersPerPipelineLayout = narrow();
	out.maxDynamicStorageBuffersPerPipelineLayout = narrow();
	out.maxSampledTexturesPerShaderStage = narrow();
	out.maxSamplersPerShaderStage = narrow();
	out.maxStorageBuffersPerShaderStage = narrow();
	out.maxStorageTexturesPerShaderStage = narrow();
	out.maxUniformBuffersPerShaderStage = narrow();
	out.maxUniformBufferBindingSize = u64();
	out.maxStorageBufferBindingSize = u64();
	out.minUniformBufferOffsetAlignment = narrow();
	out.minStorageBufferOffsetAlignment = narrow();
	out.maxVertexBuffers = narrow();
	out.maxBufferSize = u64();
	out.maxVertexAttributes = narrow();
	out.maxVertexBufferArrayStride = narrow();
	out.maxInterStageShaderVariables = narrow();
	out.maxColorAttachments = narrow();
	out.maxColorAttachmentBytesPerSample = narrow();
	out.maxComputeWorkgroupStorageSize = narrow();
	out.maxComputeInvocationsPerWorkgroup = narrow();
	out.maxComputeWorkgroupSizeX = narrow();
	out.maxComputeWorkgroupSizeY = narrow();
	out.maxComputeWorkgroupSizeZ = narrow();
	out.maxComputeWorkgroupsPerDimension = narrow();
	out.maxImmediateSize = narrow();
	return out;
}

auto gse::webgpu::wire::dispatch(const event kind, reader& in) -> void {
	auto& s = shared();
	const auto take = [&s](const std::uint64_t future) {
		const std::lock_guard _(s.mutex);
		const auto it = s.futures.find(future);
		assert(it != s.futures.end(), "webgpu wire: event for unknown future {}", future);
		auto callback = std::move(it->second);
		s.futures.erase(it);
		return callback;
	};
	const auto message_of = [](const std::string& text) {
		return WGPUStringView{ .data = text.data(), .length = text.size() };
	};

	switch (kind) {
		case event::adapter: {
			const auto callback = std::get<WGPURequestAdapterCallbackInfo>(take(in.u64()));
			const auto status = static_cast<WGPURequestAdapterStatus>(in.u32());
			const auto adapter = in.u32();
			const auto message = in.str();
			if (status == WGPURequestAdapterStatus_Success) {
				adapter_state info;
				info.vendor = in.str();
				info.architecture = in.str();
				info.device = in.str();
				info.description = in.str();
				info.vendor_id = in.u32();
				info.device_id = in.u32();
				info.limits = in.limits();
				const auto feature_count = in.u32();
				for (std::uint32_t i = 0; i < feature_count; ++i) {
					info.features.push_back(static_cast<WGPUFeatureName>(in.u32()));
				}
				info.preferred = static_cast<WGPUTextureFormat>(in.u32());
				const std::lock_guard _(s.mutex);
				s.adapters.insert_or_assign(adapter, std::move(info));
			}
			callback.callback(status, status == WGPURequestAdapterStatus_Success ? reinterpret_cast<WGPUAdapter>(static_cast<std::uintptr_t>(adapter)) : nullptr, message_of(message), callback.userdata1, callback.userdata2);
			break;
		}
		case event::device: {
			const auto [callback, device] = std::get<std::pair<WGPURequestDeviceCallbackInfo, std::uint32_t>>(take(in.u64()));
			const auto status = static_cast<WGPURequestDeviceStatus>(in.u32());
			const auto message = in.str();
			if (status == WGPURequestDeviceStatus_Success) {
				const auto limits = in.limits();
				const std::lock_guard _(s.mutex);
				s.devices[device].limits = limits;
			}
			callback.callback(status, status == WGPURequestDeviceStatus_Success ? reinterpret_cast<WGPUDevice>(static_cast<std::uintptr_t>(device)) : nullptr, message_of(message), callback.userdata1, callback.userdata2);
			break;
		}
		case event::mapped: {
			const auto callback = std::get<std::pair<WGPUBufferMapCallbackInfo, std::uint32_t>>(take(in.u64())).first;
			const auto status = static_cast<WGPUMapAsyncStatus>(in.u32());
			const auto message = in.str();
			callback.callback(status, message_of(message), callback.userdata1, callback.userdata2);
			break;
		}
		case event::work_done: {
			const auto callback = std::get<WGPUQueueWorkDoneCallbackInfo>(take(in.u64()));
			const auto status = static_cast<WGPUQueueWorkDoneStatus>(in.u32());
			const auto message = in.str();
			callback.callback(status, message_of(message), callback.userdata1, callback.userdata2);
			break;
		}
		case event::scope_popped: {
			const auto callback = std::get<WGPUPopErrorScopeCallbackInfo>(take(in.u64()));
			const auto status = static_cast<WGPUPopErrorScopeStatus>(in.u32());
			const auto type = static_cast<WGPUErrorType>(in.u32());
			const auto message = in.str();
			callback.callback(status, type, message_of(message), callback.userdata1, callback.userdata2);
			break;
		}
		case event::uncaptured_error:
		case event::device_lost: {
			const auto device_id = in.u32();
			const auto code = in.u32();
			const auto message = in.str();
			device_state device;
			{
				const std::lock_guard _(s.mutex);
				device = s.devices[device_id];
			}
			auto device_handle = reinterpret_cast<WGPUDevice>(static_cast<std::uintptr_t>(device_id));
			if (kind == event::uncaptured_error && device.uncaptured.callback) {
				device.uncaptured.callback(&device_handle, static_cast<WGPUErrorType>(code), message_of(message), device.uncaptured.userdata1, device.uncaptured.userdata2);
			}
			if (kind == event::device_lost && device.lost.callback) {
				device.lost.callback(&device_handle, static_cast<WGPUDeviceLostReason>(code), message_of(message), device.lost.userdata1, device.lost.userdata2);
			}
			break;
		}
	}
}

using namespace gse::webgpu::wire;

extern "C" auto wgpuCreateInstance(const WGPUInstanceDescriptor*) -> WGPUInstance {
	return fresh<WGPUInstance>();
}

extern "C" auto wgpuInstanceRelease(WGPUInstance) -> void {}

extern "C" auto wgpuInstanceProcessEvents(WGPUInstance) -> void {
	auto& s = shared();
	std::vector<std::uint32_t> batch;
	{
		const std::lock_guard _(s.mutex);
		const auto count = gse_host_gpu_poll(s.inbox.data(), static_cast<std::int32_t>(s.inbox.size()));
		batch.assign(s.inbox.begin(), s.inbox.begin() + count);
	}
	for (std::size_t at = 0; at + 2 <= batch.size();) {
		const auto kind = static_cast<event>(batch[at]);
		const auto length = batch[at + 1];
		gse::assert(length >= 2 && at + length <= batch.size(), "webgpu wire: malformed event of {} words", length);
		reader in(batch.data() + at + 2, length - 2);
		dispatch(kind, in);
		at += length;
	}
}

extern "C" auto wgpuInstanceRequestAdapter(WGPUInstance, const WGPURequestAdapterOptions* options, const WGPURequestAdapterCallbackInfo info) -> WGPUFuture {
	const auto future = future_for(info);
	const auto adapter = fresh<WGPUAdapter>();
	writer(op::request_adapter).u64(future).id(adapter).u32(options ? options->powerPreference : 0).u32(options ? options->forceFallbackAdapter : 0).send();
	return { future };
}

extern "C" auto wgpuInstanceCreateSurface(WGPUInstance, const WGPUSurfaceDescriptor*) -> WGPUSurface {
	const auto surface = fresh<WGPUSurface>();
	writer(op::create_surface).id(surface).send();
	return surface;
}

extern "C" auto wgpuAdapterGetInfo(const WGPUAdapter adapter, WGPUAdapterInfo* info) -> WGPUStatus {
	auto& s = shared();
	const std::lock_guard _(s.mutex);
	const auto it = s.adapters.find(id_of(adapter));
	if (it == s.adapters.end()) {
		return WGPUStatus_Error;
	}
	const auto& a = it->second;
	*info = WGPUAdapterInfo{
		.nextInChain = nullptr,
		.vendor = { a.vendor.data(), a.vendor.size() },
		.architecture = { a.architecture.data(), a.architecture.size() },
		.device = { a.device.data(), a.device.size() },
		.description = { a.description.data(), a.description.size() },
		.backendType = WGPUBackendType_WebGPU,
		.adapterType = WGPUAdapterType_Unknown,
		.vendorID = a.vendor_id,
		.deviceID = a.device_id,
		.subgroupMinSize = 0,
		.subgroupMaxSize = 0,
	};
	return WGPUStatus_Success;
}

extern "C" auto wgpuAdapterInfoFreeMembers(WGPUAdapterInfo) -> void {}

extern "C" auto wgpuAdapterGetLimits(const WGPUAdapter adapter, WGPULimits* limits) -> WGPUStatus {
	auto& s = shared();
	const std::lock_guard _(s.mutex);
	const auto it = s.adapters.find(id_of(adapter));
	if (it == s.adapters.end()) {
		return WGPUStatus_Error;
	}
	*limits = it->second.limits;
	return WGPUStatus_Success;
}

extern "C" auto wgpuAdapterHasFeature(const WGPUAdapter adapter, const WGPUFeatureName feature) -> WGPUBool {
	auto& s = shared();
	const std::lock_guard _(s.mutex);
	const auto it = s.adapters.find(id_of(adapter));
	return it != s.adapters.end() && std::ranges::contains(it->second.features, feature);
}

extern "C" auto wgpuAdapterRelease(WGPUAdapter) -> void {}

extern "C" auto wgpuAdapterRequestDevice(const WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor, const WGPURequestDeviceCallbackInfo info) -> WGPUFuture {
	const auto device = fresh<WGPUDevice>();
	{
		auto& s = shared();
		const std::lock_guard _(s.mutex);
		s.devices[id_of(device)] = device_state{
			.limits = {},
			.lost = descriptor->deviceLostCallbackInfo,
			.uncaptured = descriptor->uncapturedErrorCallbackInfo,
		};
	}
	const auto future = future_for(std::pair{ info, id_of(device) });
	writer out(op::request_device);
	out.u64(future).id(adapter).id(device).str(descriptor->label).u32(static_cast<std::uint32_t>(descriptor->requiredFeatureCount));
	for (std::size_t i = 0; i < descriptor->requiredFeatureCount; ++i) {
		out.u32(descriptor->requiredFeatures[i]);
	}
	out.u32(descriptor->requiredLimits ? 1u : 0u);
	if (descriptor->requiredLimits) {
		limits(out, *descriptor->requiredLimits);
	}
	out.send();
	return { future };
}

extern "C" auto wgpuDeviceGetLimits(const WGPUDevice device, WGPULimits* limits) -> WGPUStatus {
	auto& s = shared();
	const std::lock_guard _(s.mutex);
	const auto it = s.devices.find(id_of(device));
	if (it == s.devices.end()) {
		return WGPUStatus_Error;
	}
	*limits = it->second.limits;
	return WGPUStatus_Success;
}

extern "C" auto wgpuDeviceGetQueue(const WGPUDevice device) -> WGPUQueue {
	const auto queue = fresh<WGPUQueue>();
	writer(op::get_queue).id(device).id(queue).send();
	return queue;
}

extern "C" auto wgpuDeviceRelease(WGPUDevice) -> void {}

extern "C" auto wgpuQueueRelease(WGPUQueue) -> void {}

extern "C" auto wgpuDevicePushErrorScope(const WGPUDevice device, const WGPUErrorFilter filter) -> void {
	writer(op::push_error_scope).id(device).u32(filter).send();
}

extern "C" auto wgpuDevicePopErrorScope(const WGPUDevice device, const WGPUPopErrorScopeCallbackInfo info) -> WGPUFuture {
	const auto future = future_for(info);
	writer(op::pop_error_scope).id(device).u64(future).send();
	return { future };
}

extern "C" auto wgpuSurfaceGetCapabilities(WGPUSurface, const WGPUAdapter adapter, WGPUSurfaceCapabilities* capabilities) -> WGPUStatus {
	static constexpr std::array present_modes{ WGPUPresentMode_Fifo };
	static constexpr std::array alpha_modes{ WGPUCompositeAlphaMode_Opaque, WGPUCompositeAlphaMode_Premultiplied };
	thread_local std::array<WGPUTextureFormat, 2> formats{};
	{
		auto& s = shared();
		const std::lock_guard _(s.mutex);
		const auto it = s.adapters.find(id_of(adapter));
		formats[0] = it != s.adapters.end() ? it->second.preferred : WGPUTextureFormat_BGRA8Unorm;
	}
	formats[1] = formats[0] == WGPUTextureFormat_BGRA8Unorm ? WGPUTextureFormat_RGBA8Unorm : WGPUTextureFormat_BGRA8Unorm;
	*capabilities = WGPUSurfaceCapabilities{
		.nextInChain = nullptr,
		.usages = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
		.formatCount = formats.size(),
		.formats = formats.data(),
		.presentModeCount = present_modes.size(),
		.presentModes = present_modes.data(),
		.alphaModeCount = alpha_modes.size(),
		.alphaModes = alpha_modes.data(),
	};
	return WGPUStatus_Success;
}

extern "C" auto wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities) -> void {}

extern "C" auto wgpuSurfaceConfigure(const WGPUSurface surface, const WGPUSurfaceConfiguration* config) -> void {
	writer out(op::configure_surface);
	out.id(surface).id(config->device).u32(config->format).u64(config->usage).u32(config->width).u32(config->height).u32(config->alphaMode).u32(static_cast<std::uint32_t>(config->viewFormatCount));
	for (std::size_t i = 0; i < config->viewFormatCount; ++i) {
		out.u32(config->viewFormats[i]);
	}
	out.send();
}

extern "C" auto wgpuSurfaceUnconfigure(const WGPUSurface surface) -> void {
	writer(op::unconfigure_surface).id(surface).send();
}

extern "C" auto wgpuSurfaceGetCurrentTexture(const WGPUSurface surface, WGPUSurfaceTexture* out) -> void {
	const auto texture = fresh<WGPUTexture>();
	writer(op::get_current_texture).id(surface).id(texture).send();
	*out = WGPUSurfaceTexture{
		.nextInChain = nullptr,
		.texture = texture,
		.status = WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal,
	};
}

extern "C" auto wgpuSurfacePresent(const WGPUSurface surface) -> WGPUStatus {
	writer(op::present).id(surface).send();
	return WGPUStatus_Success;
}

extern "C" auto wgpuSurfaceRelease(const WGPUSurface surface) -> void {
	writer(op::release).id(surface).send();
}

extern "C" auto wgpuDeviceCreateBuffer(const WGPUDevice device, const WGPUBufferDescriptor* descriptor) -> WGPUBuffer {
	gse::assert(!descriptor->mappedAtCreation, "webgpu wire: mappedAtCreation is not carried over the wire");
	const auto buffer = fresh<WGPUBuffer>();
	writer(op::create_buffer).id(device).id(buffer).str(descriptor->label).u64(descriptor->usage).u64(descriptor->size).send();
	return buffer;
}

extern "C" auto wgpuDeviceCreateTexture(const WGPUDevice device, const WGPUTextureDescriptor* descriptor) -> WGPUTexture {
	const auto texture = fresh<WGPUTexture>();
	writer out(op::create_texture);
	out.id(device).id(texture).str(descriptor->label).u64(descriptor->usage).u32(descriptor->dimension);
	extent(out, descriptor->size);
	out.u32(descriptor->format).u32(descriptor->mipLevelCount).u32(descriptor->sampleCount).u32(static_cast<std::uint32_t>(descriptor->viewFormatCount));
	for (std::size_t i = 0; i < descriptor->viewFormatCount; ++i) {
		out.u32(descriptor->viewFormats[i]);
	}
	out.send();
	return texture;
}

extern "C" auto wgpuTextureCreateView(const WGPUTexture texture, const WGPUTextureViewDescriptor* descriptor) -> WGPUTextureView {
	const auto view = fresh<WGPUTextureView>();
	writer out(op::create_view);
	out.id(texture).id(view).u32(descriptor ? 1u : 0u);
	if (descriptor) {
		out.str(descriptor->label).u32(descriptor->format).u32(descriptor->dimension).u32(descriptor->baseMipLevel).u32(descriptor->mipLevelCount)
			.u32(descriptor->baseArrayLayer).u32(descriptor->arrayLayerCount).u32(descriptor->aspect).u64(descriptor->usage);
	}
	out.send();
	return view;
}

extern "C" auto wgpuDeviceCreateSampler(const WGPUDevice device, const WGPUSamplerDescriptor* d) -> WGPUSampler {
	const auto sampler = fresh<WGPUSampler>();
	writer(op::create_sampler).id(device).id(sampler).str(d->label).u32(d->addressModeU).u32(d->addressModeV).u32(d->addressModeW)
		.u32(d->magFilter).u32(d->minFilter).u32(d->mipmapFilter).f32(d->lodMinClamp).f32(d->lodMaxClamp).u32(d->compare).u32(d->maxAnisotropy).send();
	return sampler;
}

extern "C" auto wgpuDeviceCreateBindGroupLayout(const WGPUDevice device, const WGPUBindGroupLayoutDescriptor* d) -> WGPUBindGroupLayout {
	const auto layout = fresh<WGPUBindGroupLayout>();
	writer out(op::create_bind_group_layout);
	out.id(device).id(layout).str(d->label).u32(static_cast<std::uint32_t>(d->entryCount));
	for (std::size_t i = 0; i < d->entryCount; ++i) {
		const auto& e = d->entries[i];
		out.u32(e.binding).u64(e.visibility).u32(e.bindingArraySize)
			.u32(e.buffer.type).u32(e.buffer.hasDynamicOffset).u64(e.buffer.minBindingSize)
			.u32(e.sampler.type)
			.u32(e.texture.sampleType).u32(e.texture.viewDimension).u32(e.texture.multisampled)
			.u32(e.storageTexture.access).u32(e.storageTexture.format).u32(e.storageTexture.viewDimension);
	}
	out.send();
	return layout;
}

extern "C" auto wgpuDeviceCreateBindGroup(const WGPUDevice device, const WGPUBindGroupDescriptor* d) -> WGPUBindGroup {
	const auto group = fresh<WGPUBindGroup>();
	writer out(op::create_bind_group);
	out.id(device).id(group).str(d->label).id(d->layout).u32(static_cast<std::uint32_t>(d->entryCount));
	for (std::size_t i = 0; i < d->entryCount; ++i) {
		const auto& e = d->entries[i];
		out.u32(e.binding).id(e.buffer).u64(e.offset).u64(e.size).id(e.sampler).id(e.textureView);
	}
	out.send();
	return group;
}

extern "C" auto wgpuDeviceCreatePipelineLayout(const WGPUDevice device, const WGPUPipelineLayoutDescriptor* d) -> WGPUPipelineLayout {
	const auto layout = fresh<WGPUPipelineLayout>();
	writer out(op::create_pipeline_layout);
	out.id(device).id(layout).str(d->label).u32(static_cast<std::uint32_t>(d->bindGroupLayoutCount));
	for (std::size_t i = 0; i < d->bindGroupLayoutCount; ++i) {
		out.id(d->bindGroupLayouts[i]);
	}
	out.send();
	return layout;
}

extern "C" auto wgpuDeviceCreateShaderModule(const WGPUDevice device, const WGPUShaderModuleDescriptor* d) -> WGPUShaderModule {
	const auto* chain = d->nextInChain;
	gse::assert(chain && chain->sType == WGPUSType_ShaderSourceWGSL, "webgpu wire: only WGSL shader modules cross the wire");
	const auto* source = reinterpret_cast<const WGPUShaderSourceWGSL*>(chain);
	const auto module = fresh<WGPUShaderModule>();
	writer(op::create_shader_module).id(device).id(module).str(d->label).str(source->code).send();
	return module;
}

extern "C" auto wgpuDeviceCreateComputePipeline(const WGPUDevice device, const WGPUComputePipelineDescriptor* d) -> WGPUComputePipeline {
	const auto pipeline = fresh<WGPUComputePipeline>();
	writer out(op::create_compute_pipeline);
	out.id(device).id(pipeline).str(d->label).id(d->layout);
	programmable(out, d->compute.module, d->compute.entryPoint, d->compute.constantCount, d->compute.constants);
	out.send();
	return pipeline;
}

extern "C" auto wgpuDeviceCreateRenderPipeline(const WGPUDevice device, const WGPURenderPipelineDescriptor* d) -> WGPURenderPipeline {
	const auto pipeline = fresh<WGPURenderPipeline>();
	writer out(op::create_render_pipeline);
	out.id(device).id(pipeline).str(d->label).id(d->layout);
	programmable(out, d->vertex.module, d->vertex.entryPoint, d->vertex.constantCount, d->vertex.constants);
	out.u32(static_cast<std::uint32_t>(d->vertex.bufferCount));
	for (std::size_t i = 0; i < d->vertex.bufferCount; ++i) {
		const auto& b = d->vertex.buffers[i];
		out.u32(b.stepMode).u64(b.arrayStride).u32(static_cast<std::uint32_t>(b.attributeCount));
		for (std::size_t a = 0; a < b.attributeCount; ++a) {
			out.u32(b.attributes[a].format).u64(b.attributes[a].offset).u32(b.attributes[a].shaderLocation);
		}
	}
	out.u32(d->primitive.topology).u32(d->primitive.stripIndexFormat).u32(d->primitive.frontFace).u32(d->primitive.cullMode).u32(d->primitive.unclippedDepth);
	out.u32(d->depthStencil ? 1u : 0u);
	if (const auto* ds = d->depthStencil) {
		out.u32(ds->format).u32(ds->depthWriteEnabled).u32(ds->depthCompare);
		for (const auto& face : { ds->stencilFront, ds->stencilBack }) {
			out.u32(face.compare).u32(face.failOp).u32(face.depthFailOp).u32(face.passOp);
		}
		out.u32(ds->stencilReadMask).u32(ds->stencilWriteMask).i32(ds->depthBias).f32(ds->depthBiasSlopeScale).f32(ds->depthBiasClamp);
	}
	out.u32(d->multisample.count).u32(d->multisample.mask).u32(d->multisample.alphaToCoverageEnabled);
	out.u32(d->fragment ? 1u : 0u);
	if (const auto* f = d->fragment) {
		programmable(out, f->module, f->entryPoint, f->constantCount, f->constants);
		out.u32(static_cast<std::uint32_t>(f->targetCount));
		for (std::size_t i = 0; i < f->targetCount; ++i) {
			const auto& t = f->targets[i];
			out.u32(t.format).u64(t.writeMask).u32(t.blend ? 1u : 0u);
			if (t.blend) {
				for (const auto& c : { t.blend->color, t.blend->alpha }) {
					out.u32(c.operation).u32(c.srcFactor).u32(c.dstFactor);
				}
			}
		}
	}
	out.send();
	return pipeline;
}

extern "C" auto wgpuDeviceCreateCommandEncoder(const WGPUDevice device, const WGPUCommandEncoderDescriptor* d) -> WGPUCommandEncoder {
	const auto encoder = fresh<WGPUCommandEncoder>();
	writer(op::create_command_encoder).id(device).id(encoder).str(d ? d->label : WGPUStringView{}).send();
	return encoder;
}

extern "C" auto wgpuCommandEncoderBeginRenderPass(const WGPUCommandEncoder encoder, const WGPURenderPassDescriptor* d) -> WGPURenderPassEncoder {
	gse::assert(!d->timestampWrites && !d->occlusionQuerySet, "webgpu wire: render pass queries are not carried over the wire");
	const auto pass = fresh<WGPURenderPassEncoder>();
	writer out(op::begin_render_pass);
	out.id(encoder).id(pass).str(d->label).u32(static_cast<std::uint32_t>(d->colorAttachmentCount));
	for (std::size_t i = 0; i < d->colorAttachmentCount; ++i) {
		const auto& c = d->colorAttachments[i];
		out.id(c.view).u32(c.depthSlice).id(c.resolveTarget).u32(c.loadOp).u32(c.storeOp).f64(c.clearValue.r).f64(c.clearValue.g).f64(c.clearValue.b).f64(c.clearValue.a);
	}
	out.u32(d->depthStencilAttachment ? 1u : 0u);
	if (const auto* ds = d->depthStencilAttachment) {
		out.id(ds->view).u32(ds->depthLoadOp).u32(ds->depthStoreOp).f32(ds->depthClearValue).u32(ds->depthReadOnly)
			.u32(ds->stencilLoadOp).u32(ds->stencilStoreOp).u32(ds->stencilClearValue).u32(ds->stencilReadOnly);
	}
	out.send();
	return pass;
}

extern "C" auto wgpuCommandEncoderBeginComputePass(const WGPUCommandEncoder encoder, const WGPUComputePassDescriptor* d) -> WGPUComputePassEncoder {
	gse::assert(!d || !d->timestampWrites, "webgpu wire: compute pass timestamps are not carried over the wire");
	const auto pass = fresh<WGPUComputePassEncoder>();
	writer(op::begin_compute_pass).id(encoder).id(pass).str(d ? d->label : WGPUStringView{}).send();
	return pass;
}

extern "C" auto wgpuCommandEncoderFinish(const WGPUCommandEncoder encoder, const WGPUCommandBufferDescriptor* d) -> WGPUCommandBuffer {
	const auto buffer = fresh<WGPUCommandBuffer>();
	writer(op::finish).id(encoder).id(buffer).str(d ? d->label : WGPUStringView{}).send();
	return buffer;
}

extern "C" auto wgpuCommandEncoderClearBuffer(const WGPUCommandEncoder encoder, const WGPUBuffer buffer, const std::uint64_t offset, const std::uint64_t size) -> void {
	writer(op::clear_buffer).id(encoder).id(buffer).u64(offset).u64(size).send();
}

extern "C" auto wgpuCommandEncoderCopyBufferToBuffer(const WGPUCommandEncoder encoder, const WGPUBuffer source, const std::uint64_t source_offset, const WGPUBuffer destination, const std::uint64_t destination_offset, const std::uint64_t size) -> void {
	writer(op::copy_buffer_to_buffer).id(encoder).id(source).u64(source_offset).id(destination).u64(destination_offset).u64(size).send();
}

extern "C" auto wgpuCommandEncoderCopyTextureToBuffer(const WGPUCommandEncoder encoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyBufferInfo* destination, const WGPUExtent3D* size) -> void {
	writer out(op::copy_texture_to_buffer);
	out.id(encoder);
	texture_copy(out, *source);
	out.id(destination->buffer).u64(destination->layout.offset).u32(destination->layout.bytesPerRow).u32(destination->layout.rowsPerImage);
	extent(out, *size);
	out.send();
}

extern "C" auto wgpuCommandEncoderCopyTextureToTexture(const WGPUCommandEncoder encoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyTextureInfo* destination, const WGPUExtent3D* size) -> void {
	writer out(op::copy_texture_to_texture);
	out.id(encoder);
	texture_copy(out, *source);
	texture_copy(out, *destination);
	extent(out, *size);
	out.send();
}

extern "C" auto wgpuCommandEncoderPushDebugGroup(const WGPUCommandEncoder encoder, const WGPUStringView label) -> void {
	writer(op::push_debug_group).id(encoder).str(label).send();
}

extern "C" auto wgpuCommandEncoderPopDebugGroup(const WGPUCommandEncoder encoder) -> void {
	writer(op::pop_debug_group).id(encoder).send();
}

extern "C" auto wgpuCommandEncoderRelease(const WGPUCommandEncoder encoder) -> void {
	writer(op::release).id(encoder).send();
}

extern "C" auto wgpuCommandBufferRelease(const WGPUCommandBuffer buffer) -> void {
	writer(op::release).id(buffer).send();
}

extern "C" auto wgpuRenderPassEncoderPushDebugGroup(const WGPURenderPassEncoder pass, const WGPUStringView label) -> void {
	writer(op::push_debug_group).id(pass).str(label).send();
}

extern "C" auto wgpuRenderPassEncoderPopDebugGroup(const WGPURenderPassEncoder pass) -> void {
	writer(op::pop_debug_group).id(pass).send();
}

extern "C" auto wgpuRenderPassEncoderEnd(const WGPURenderPassEncoder pass) -> void {
	writer(op::pass_end).id(pass).send();
}

extern "C" auto wgpuRenderPassEncoderRelease(const WGPURenderPassEncoder pass) -> void {
	writer(op::release).id(pass).send();
}

extern "C" auto wgpuRenderPassEncoderSetPipeline(const WGPURenderPassEncoder pass, const WGPURenderPipeline pipeline) -> void {
	writer(op::set_pipeline).id(pass).id(pipeline).send();
}

extern "C" auto wgpuRenderPassEncoderSetBindGroup(const WGPURenderPassEncoder pass, const std::uint32_t index, const WGPUBindGroup group, const std::size_t count, const std::uint32_t* offsets) -> void {
	writer out(op::set_bind_group);
	out.id(pass).u32(index).id(group).u32(static_cast<std::uint32_t>(count));
	for (std::size_t i = 0; i < count; ++i) {
		out.u32(offsets[i]);
	}
	out.send();
}

extern "C" auto wgpuRenderPassEncoderSetIndexBuffer(const WGPURenderPassEncoder pass, const WGPUBuffer buffer, const WGPUIndexFormat format, const std::uint64_t offset, const std::uint64_t size) -> void {
	writer(op::set_index_buffer).id(pass).id(buffer).u32(format).u64(offset).u64(size).send();
}

extern "C" auto wgpuRenderPassEncoderSetViewport(const WGPURenderPassEncoder pass, const float x, const float y, const float width, const float height, const float min_depth, const float max_depth) -> void {
	writer(op::set_viewport).id(pass).f32(x).f32(y).f32(width).f32(height).f32(min_depth).f32(max_depth).send();
}

extern "C" auto wgpuRenderPassEncoderSetScissorRect(const WGPURenderPassEncoder pass, const std::uint32_t x, const std::uint32_t y, const std::uint32_t width, const std::uint32_t height) -> void {
	writer(op::set_scissor).id(pass).u32(x).u32(y).u32(width).u32(height).send();
}

extern "C" auto wgpuRenderPassEncoderDraw(const WGPURenderPassEncoder pass, const std::uint32_t vertices, const std::uint32_t instances, const std::uint32_t first_vertex, const std::uint32_t first_instance) -> void {
	writer(op::draw).id(pass).u32(vertices).u32(instances).u32(first_vertex).u32(first_instance).send();
}

extern "C" auto wgpuRenderPassEncoderDrawIndexed(const WGPURenderPassEncoder pass, const std::uint32_t indices, const std::uint32_t instances, const std::uint32_t first_index, const std::int32_t base_vertex, const std::uint32_t first_instance) -> void {
	writer(op::draw_indexed).id(pass).u32(indices).u32(instances).u32(first_index).i32(base_vertex).u32(first_instance).send();
}

extern "C" auto wgpuRenderPassEncoderDrawIndexedIndirect(const WGPURenderPassEncoder pass, const WGPUBuffer buffer, const std::uint64_t offset) -> void {
	writer(op::draw_indexed_indirect).id(pass).id(buffer).u64(offset).send();
}

extern "C" auto wgpuComputePassEncoderSetPipeline(const WGPUComputePassEncoder pass, const WGPUComputePipeline pipeline) -> void {
	writer(op::set_pipeline).id(pass).id(pipeline).send();
}

extern "C" auto wgpuComputePassEncoderSetBindGroup(const WGPUComputePassEncoder pass, const std::uint32_t index, const WGPUBindGroup group, const std::size_t count, const std::uint32_t* offsets) -> void {
	writer out(op::set_bind_group);
	out.id(pass).u32(index).id(group).u32(static_cast<std::uint32_t>(count));
	for (std::size_t i = 0; i < count; ++i) {
		out.u32(offsets[i]);
	}
	out.send();
}

extern "C" auto wgpuComputePassEncoderDispatchWorkgroups(const WGPUComputePassEncoder pass, const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) -> void {
	writer(op::dispatch).id(pass).u32(x).u32(y).u32(z).send();
}

extern "C" auto wgpuComputePassEncoderDispatchWorkgroupsIndirect(const WGPUComputePassEncoder pass, const WGPUBuffer buffer, const std::uint64_t offset) -> void {
	writer(op::dispatch_indirect).id(pass).id(buffer).u64(offset).send();
}

extern "C" auto wgpuComputePassEncoderEnd(const WGPUComputePassEncoder pass) -> void {
	writer(op::pass_end).id(pass).send();
}

extern "C" auto wgpuComputePassEncoderRelease(const WGPUComputePassEncoder pass) -> void {
	writer(op::release).id(pass).send();
}

extern "C" auto wgpuQueueSubmit(const WGPUQueue queue, const std::size_t count, const WGPUCommandBuffer* commands) -> void {
	writer out(op::submit);
	out.id(queue).u32(static_cast<std::uint32_t>(count));
	for (std::size_t i = 0; i < count; ++i) {
		out.id(commands[i]);
	}
	out.send();
}

extern "C" auto wgpuQueueOnSubmittedWorkDone(const WGPUQueue queue, const WGPUQueueWorkDoneCallbackInfo info) -> WGPUFuture {
	const auto future = future_for(info);
	writer(op::on_submitted_work_done).id(queue).u64(future).send();
	return { future };
}

extern "C" auto wgpuQueueWriteBuffer(const WGPUQueue queue, const WGPUBuffer buffer, const std::uint64_t offset, const void* data, const std::size_t size) -> void {
	writer(op::write_buffer).id(queue).id(buffer).u64(offset).tail(data, size).send();
}

extern "C" auto wgpuQueueWriteTexture(const WGPUQueue queue, const WGPUTexelCopyTextureInfo* destination, const void* data, const std::size_t size, const WGPUTexelCopyBufferLayout* layout, const WGPUExtent3D* write_size) -> void {
	writer out(op::write_texture);
	out.id(queue);
	texture_copy(out, *destination);
	out.u64(layout->offset).u32(layout->bytesPerRow).u32(layout->rowsPerImage);
	extent(out, *write_size);
	out.tail(data, size);
	out.send();
}

extern "C" auto wgpuBufferMapAsync(const WGPUBuffer buffer, const WGPUMapMode mode, const std::size_t offset, const std::size_t size, const WGPUBufferMapCallbackInfo info) -> WGPUFuture {
	gse::assert(mode == WGPUMapMode_Read, "webgpu wire: only read mappings cross the wire");
	auto& s = shared();
	std::byte* staging = nullptr;
	{
		const std::lock_guard _(s.mutex);
		auto& m = s.mappings[id_of(buffer)];
		m.bytes.assign(size, std::byte{ 0 });
		m.offset = offset;
		staging = m.bytes.data();
	}
	const auto future = future_for(std::pair{ info, id_of(buffer) });
	writer(op::map_read).id(buffer).u64(future).u64(offset).u64(size).u32(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(staging))).send();
	return { future };
}

extern "C" auto wgpuBufferGetConstMappedRange(const WGPUBuffer buffer, const std::size_t offset, std::size_t) -> const void* {
	auto& s = shared();
	const std::lock_guard _(s.mutex);
	const auto it = s.mappings.find(id_of(buffer));
	if (it == s.mappings.end() || offset < it->second.offset) {
		return nullptr;
	}
	return it->second.bytes.data() + (offset - it->second.offset);
}

extern "C" auto wgpuBufferUnmap(const WGPUBuffer buffer) -> void {
	{
		auto& s = shared();
		const std::lock_guard _(s.mutex);
		s.mappings.erase(id_of(buffer));
	}
	writer(op::unmap).id(buffer).send();
}

extern "C" auto wgpuBufferRelease(const WGPUBuffer buffer) -> void {
	writer(op::release).id(buffer).send();
}

extern "C" auto wgpuTextureRelease(const WGPUTexture texture) -> void {
	writer(op::release).id(texture).send();
}

extern "C" auto wgpuTextureViewRelease(const WGPUTextureView view) -> void {
	writer(op::release).id(view).send();
}

extern "C" auto wgpuBindGroupRelease(const WGPUBindGroup group) -> void {
	writer(op::release).id(group).send();
}

module gse.gpu;

import std;
import vulkan;

import :vulkan_allocator;

import gse.core;
import gse.assert;
import gse.log;
import gse.save;

auto gse::vulkan::allocator::pool_key::operator==(const pool_key& other) const -> bool {
    return memory_type_index == other.memory_type_index
        && properties == other.properties
        && device_address == other.device_address;
}

auto gse::vulkan::allocator::pool_key_hash::operator()(const pool_key& key) const noexcept -> std::size_t {
    const auto memory_hash = std::hash<std::uint32_t>()(key.memory_type_index);
    const auto props_hash = std::hash<vk::MemoryPropertyFlags>()(key.properties);
    const auto address_hash = std::hash<bool>()(key.device_address);
    return memory_hash ^ (props_hash << 1) ^ (address_hash << 2);
}

gse::vulkan::allocation::allocation(const vk::DeviceMemory memory, const vk::DeviceSize size, const vk::DeviceSize offset, void* mapped, sub_allocation* owner, allocator* alloc, const vk::Device device, allocation_debug_info debug_info)
    : m_memory(memory),
      m_size(size),
      m_offset(offset),
      m_mapped(mapped),
      m_owner(owner),
      m_allocator(alloc),
      m_device(device),
      m_debug_info(std::move(debug_info)) {}

gse::vulkan::allocation::~allocation() {
    if (m_owner && m_allocator) {
        m_allocator->free(*this);
    }
}

gse::vulkan::allocation::allocation(allocation&& other) noexcept
    : m_memory(other.m_memory),
      m_size(other.m_size),
      m_offset(other.m_offset),
      m_mapped(other.m_mapped),
      m_owner(other.m_owner),
      m_allocator(other.m_allocator),
      m_device(other.m_device),
      m_debug_info(std::move(other.m_debug_info)) {
    other.m_owner = nullptr;
    other.m_allocator = nullptr;
    other.m_device = nullptr;
}

auto gse::vulkan::allocation::operator=(allocation&& other) noexcept -> allocation& {
    if (this != &other) {
        if (m_owner && m_allocator) {
            m_allocator->free(*this);
        }

        m_memory = other.m_memory;
        m_size = other.m_size;
        m_offset = other.m_offset;
        m_mapped = other.m_mapped;
        m_owner = other.m_owner;
        m_allocator = other.m_allocator;
        m_device = other.m_device;
        m_debug_info = std::move(other.m_debug_info);

        other.m_owner = nullptr;
        other.m_allocator = nullptr;
        other.m_device = nullptr;
    }
    return *this;
}

auto gse::vulkan::allocation::memory() const -> vk::DeviceMemory {
    return m_memory;
}

auto gse::vulkan::allocation::size() const -> vk::DeviceSize {
    return m_size;
}

auto gse::vulkan::allocation::offset() const -> vk::DeviceSize {
    return m_offset;
}

auto gse::vulkan::allocation::mapped() const -> std::byte* {
    return static_cast<std::byte*>(m_mapped);
}

auto gse::vulkan::allocation::owner() const -> sub_allocation* {
    return m_owner;
}

auto gse::vulkan::allocation::device() const -> vk::Device {
    return m_device;
}

auto gse::vulkan::allocation::debug_info() const -> const allocation_debug_info& {
    return m_debug_info;
}

gse::vulkan::image_resource::image_resource(const vk::Image image, const vk::ImageView view, const vk::Format format, const vk::ImageLayout current_layout, vulkan::allocation allocation) {
    this->image = image;
    this->view = view;
    this->format = format;
    this->current_layout = current_layout;
    this->allocation = std::move(allocation);
}

gse::vulkan::image_resource::~image_resource() {
    if (allocation.device()) {
        if (view) {
            allocation.device().destroyImageView(view, nullptr);
        }
        if (image) {
            allocation.device().destroyImage(image, nullptr);
        }
    }
}

gse::vulkan::image_resource::image_resource(image_resource&& other) noexcept {
    image = other.image;
    view = other.view;
    format = other.format;
    current_layout = other.current_layout;
    allocation = std::move(other.allocation);

    other.image = nullptr;
    other.view = nullptr;
}

auto gse::vulkan::image_resource::operator=(image_resource&& other) noexcept -> image_resource& {
    if (this != &other) {
        if (allocation.device()) {
            if (view) {
                allocation.device().destroyImageView(view, nullptr);
            }
            if (image) {
                allocation.device().destroyImage(image, nullptr);
            }
        }

        image = other.image;
        view = other.view;
        format = other.format;
        current_layout = other.current_layout;
        allocation = std::move(other.allocation);
        other.image = nullptr;
        other.view = nullptr;
    }
    return *this;
}

gse::vulkan::buffer_resource::buffer_resource(const vk::Buffer buffer, vulkan::allocation allocation, const vk::DeviceSize size) {
    this->buffer = buffer;
    this->allocation = std::move(allocation);
    this->size = size;
}

gse::vulkan::buffer_resource::~buffer_resource() {
    if (allocation.device() && buffer) {
        allocation.device().destroyBuffer(buffer, nullptr);
    }
}

gse::vulkan::buffer_resource::buffer_resource(buffer_resource&& other) noexcept {
    buffer = other.buffer;
    size = other.size;
    allocation = std::move(other.allocation);
    other.buffer = nullptr;
    other.size = 0;
}

auto gse::vulkan::buffer_resource::operator=(buffer_resource&& other) noexcept -> buffer_resource& {
    if (this != &other) {
        if (allocation.device() && buffer) {
            allocation.device().destroyBuffer(buffer, nullptr);
        }

        buffer = other.buffer;
        size = other.size;
        allocation = std::move(other.allocation);
        other.buffer = nullptr;
        other.size = 0;
    }
    return *this;
}

gse::vulkan::allocator::allocator(const vk::raii::Device& device, const vk::raii::PhysicalDevice& physical_device, save::state& save_state) : m_device(*device), m_physical_device(*physical_device) {
    save_state.bind("Vulkan", "Track Allocations", m_tracking_enabled)
        .description("Track Vulkan memory allocations for debugging destruction order issues")
        .default_value(false)
        .commit();

    save_state.bind("Vulkan", "Name Resources", m_name_resources)
        .description("Assign debug names to Vulkan buffers to improve validation output")
        .default_value(false)
        .commit();
}

gse::vulkan::allocator::allocator(allocator&& other) noexcept : m_device(other.m_device), m_physical_device(other.m_physical_device), m_pools(std::move(other.m_pools)) {
    other.m_device = nullptr;
    other.m_physical_device = nullptr;
}

auto gse::vulkan::allocator::operator=(allocator&& other) noexcept -> allocator& {
    if (this != &other) {
        clean_up();
        m_device = other.m_device;
        m_physical_device = other.m_physical_device;
        m_pools = std::move(other.m_pools);
        other.m_device = nullptr;
        other.m_physical_device = nullptr;
    }
    return *this;
}

gse::vulkan::allocator::~allocator() {
    clean_up();
}

auto gse::vulkan::allocator::allocate(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags properties, const std::string_view tag, const std::source_location loc, const bool device_address) -> std::expected<allocation, std::string> {
    std::lock_guard lock(m_mutex);

    if (m_tracking_enabled) {
        assert(
            !m_cleaned_up,
            std::source_location::current(),
            "Attempted to allocate after allocator cleanup! This indicates a resource lifetime issue."
        );
    }

    const auto mem_props = m_physical_device.getMemoryProperties();

    std::uint32_t new_memory_type_index = std::numeric_limits<std::uint32_t>::max();
    for (std::uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((requirements.memoryTypeBits & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            new_memory_type_index = i;
            break;
        }
    }

    if (new_memory_type_index == std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected{ "Failed to find suitable memory type for allocation!" };
    }

    auto& [memory_type_index, blocks] = m_pools[{ new_memory_type_index, properties, device_address }];
    if (blocks.empty()) {
        memory_type_index = new_memory_type_index;
    }

    memory_block* best_block = nullptr;
    std::list<sub_allocation>::iterator best_sub_it;
    vk::DeviceSize best_leftover = std::numeric_limits<vk::DeviceSize>::max();
    vk::DeviceSize best_aligned_offset = 0;

    if (!blocks.empty()) {
        for (auto& block : blocks) {
            for (auto it = block.allocations.begin(); it != block.allocations.end(); ++it) {
                if (it->in_use) {
                    continue;
                }

                const vk::DeviceSize aligned_offset = (it->offset + requirements.alignment - 1) & ~(requirements.alignment - 1);
                if (aligned_offset + requirements.size > it->offset + it->size) {
                    continue;
                }

                if (const vk::DeviceSize leftover = it->size - (aligned_offset - it->offset) - requirements.size; leftover < best_leftover) {
                    best_block = &block;
                    best_sub_it = it;
                    best_leftover = leftover;
                    best_aligned_offset = aligned_offset;
                    if (leftover == 0) {
                        break;
                    }
                }
            }
            if (best_leftover == 0) {
                break;
            }
        }
    }

    if (best_block) {
        auto& list = best_block->allocations;
        const auto sub = *best_sub_it;

        const vk::DeviceSize prefix_size = best_aligned_offset - sub.offset;
        const vk::DeviceSize suffix_size = sub.size - prefix_size - requirements.size;

        const auto next_it = list.erase(best_sub_it);
        if (prefix_size) {
            list.insert(next_it, { sub.offset, prefix_size, false });
        }
        const auto owner_it = list.insert(next_it, { best_aligned_offset, requirements.size, true });
        if (suffix_size) {
            list.insert(next_it, { best_aligned_offset + requirements.size, suffix_size, false });
        }

        allocation_debug_info debug_info;
        debug_info.creation_location = loc;
        debug_info.tag = tag;
        if (m_tracking_enabled) {
            debug_info.allocation_id = m_next_allocation_id++;
            m_live_allocations[debug_info.allocation_id] = debug_info;
            ++m_live_allocation_count;
        }

        return allocation{
            best_block->memory,
            requirements.size,
            best_aligned_offset,
            best_block->mapped ? static_cast<char*>(best_block->mapped) + best_aligned_offset : nullptr,
            &*owner_it,
            this,
            m_device,
            std::move(debug_info)
        };
    }

    const vk::DeviceSize aligned_offset = (0 + requirements.alignment - 1) & ~(requirements.alignment - 1);
    const vk::DeviceSize required_space = aligned_offset + requirements.size;
    const auto new_block_size = std::max(required_space, k_default_block_size);

    const vk::MemoryAllocateFlagsInfo flags_info{
        .flags = vk::MemoryAllocateFlagBits::eDeviceAddress,
    };
    const vk::MemoryAllocateInfo alloc_info{
        .pNext = device_address ? static_cast<const void*>(&flags_info) : nullptr,
        .allocationSize = new_block_size,
        .memoryTypeIndex = new_memory_type_index,
    };
    auto memory = m_device.allocateMemory(alloc_info, nullptr);

    blocks.emplace_back(
        memory,
        new_block_size,
        properties
    );

    auto& new_block = blocks.back();

    if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
        new_block.mapped = m_device.mapMemory(new_block.memory, 0, new_block.size, {});
    }

    const vk::DeviceSize prefix_size = aligned_offset;
    const vk::DeviceSize suffix_size = new_block.size - required_space;

    if (prefix_size > 0) {
        new_block.allocations.push_back({ 0, prefix_size, false });
    }
    new_block.allocations.push_back({ aligned_offset, requirements.size, true });
    sub_allocation* owner_ptr = &new_block.allocations.back();
    if (suffix_size > 0) {
        new_block.allocations.push_back({ aligned_offset + requirements.size, suffix_size, false });
    }

    allocation_debug_info debug_info;
    debug_info.creation_location = loc;
    debug_info.tag = tag;
    if (m_tracking_enabled) {
        debug_info.allocation_id = m_next_allocation_id++;
        m_live_allocations[debug_info.allocation_id] = debug_info;
        ++m_live_allocation_count;
    }

    return allocation{
        new_block.memory,
        requirements.size,
        aligned_offset,
        new_block.mapped ? static_cast<char*>(new_block.mapped) + aligned_offset : nullptr,
        owner_ptr,
        this,
        m_device,
        std::move(debug_info)
    };
}

auto gse::vulkan::allocator::create_buffer(const vk::BufferCreateInfo& buffer_info, const void* data, const std::string_view tag, const std::source_location& loc) -> buffer_resource {
    auto actual_buffer_info = buffer_info;
    constexpr auto device_addressable_usage =
        vk::BufferUsageFlagBits::eUniformBuffer
        | vk::BufferUsageFlagBits::eStorageBuffer
        | vk::BufferUsageFlagBits::eIndirectBuffer
        | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
        | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
        | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    const bool needs_device_address = static_cast<bool>(actual_buffer_info.usage & device_addressable_usage);

    if (needs_device_address) {
        actual_buffer_info.usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
    }

    auto buffer = m_device.createBuffer(actual_buffer_info, nullptr);
    const auto requirements = m_device.getBufferMemoryRequirements(buffer);

    allocation alloc{};
    bool success = false;

    if (data) {
        constexpr vk::MemoryPropertyFlags required_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        if (auto expected_alloc = allocate(requirements, required_flags, tag, loc, needs_device_address)) {
            alloc = std::move(*expected_alloc);
            success = true;
        }
    }
    else {
        for (const auto property_preferences = memory_flag_preferences(actual_buffer_info.usage); const auto& props : property_preferences) {
            if (auto expected_alloc = allocate(requirements, props, tag, loc, needs_device_address)) {
                alloc = std::move(*expected_alloc);
                success = true;
                break;
            }
        }
    }

    assert(success, std::source_location::current(), "Failed to allocate memory for buffer after trying all preferences.");

    m_device.bindBufferMemory(buffer, alloc.memory(), alloc.offset());

    if (m_name_resources) {
        const auto& debug_info = alloc.debug_info();
        const auto file = std::filesystem::path(debug_info.creation_location.file_name()).filename().string();
        const auto name = debug_info.tag.empty()
            ? std::format("Buffer ({}:{})", file, debug_info.creation_location.line())
            : std::format("Buffer '{}' ({}:{})", debug_info.tag, file, debug_info.creation_location.line());

        m_device.setDebugUtilsObjectNameEXT({
            .objectType = vk::ObjectType::eBuffer,
            .objectHandle = static_cast<std::uint64_t>(std::bit_cast<std::uintptr_t>(buffer)),
            .pObjectName = name.c_str(),
        });
    }

    if (data && alloc.mapped()) {
        gse::memcpy(alloc.mapped(), data, actual_buffer_info.size);
    }
    else if (data && !alloc.mapped()) {
        assert(false, std::source_location::current(), "Buffer created with data, but the allocated memory is not mappable.");
    }

    return { buffer, std::move(alloc), actual_buffer_info.size };
}

auto gse::vulkan::allocator::create_image(const vk::ImageCreateInfo& info, const vk::MemoryPropertyFlags properties, const vk::ImageViewCreateInfo& view_info, const void* data, const std::string_view tag, const std::source_location loc) -> image_resource {
    auto image = m_device.createImage(info, nullptr);
    const auto requirements = m_device.getImageMemoryRequirements(image);

    auto expected_alloc = allocate(requirements, properties, tag, loc);

    if (!expected_alloc) {
        assert(
            false,
            std::source_location::current(),
            "Failed to allocate memory for image with size {} and usage {} after trying all preferences.",
            requirements.size,
            to_string(info.usage)
        );
    }

    allocation alloc = std::move(*expected_alloc);

    m_device.bindImageMemory(image, alloc.memory(), alloc.offset());

    if (data && alloc.mapped()) {
        gse::memcpy(alloc.mapped(), data, requirements.size);
    }

    vk::ImageView view;

    if (view_info != vk::ImageViewCreateInfo{}) {
        assert(!view_info.image, std::source_location::current(), "Image view info must not have an image set yet!");
        vk::ImageViewCreateInfo actual_view_info = view_info;
        actual_view_info.image = image;
        view = m_device.createImageView(actual_view_info, nullptr);
    }
    else {
        view = m_device.createImageView({
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = info.format,
            .subresourceRange = {
                .aspectMask = info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = info.mipLevels,
                .baseArrayLayer = 0,
                .layerCount = info.arrayLayers,
            },
        }, nullptr);
    }

    return { image, view, info.format, info.initialLayout, std::move(alloc) };
}

auto gse::vulkan::allocator::free(const allocation& alloc) -> void {
    std::lock_guard lock(m_mutex);

    if (!alloc.owner()) {
        return;
    }

    if (m_tracking_enabled) {
        const auto& [creation_location, tag, allocation_id] = alloc.debug_info();
        assert(!m_cleaned_up, std::source_location::current(),
            "Allocation freed after allocator cleanup! Tag: '{}', Created at: {}:{}:{}",
            tag,
            creation_location.file_name(),
            creation_location.line(),
            creation_location.function_name());

        if (allocation_id != 0) {
            m_live_allocations.erase(allocation_id);
            --m_live_allocation_count;
        }
    }

    const auto* sub_to_free = alloc.owner();

    for (auto& [memory_type_index, blocks] : m_pools | std::views::values) {
        for (auto& block : blocks) {
            for (auto it = block.allocations.begin(); it != block.allocations.end(); ++it) {
                if (&*it == sub_to_free) {
                    it->in_use = false;

                    if (const auto next_it = std::next(it); next_it != block.allocations.end() && !next_it->in_use) {
                        it->size += next_it->size;
                        block.allocations.erase(next_it);
                    }

                    if (it != block.allocations.begin()) {
                        if (const auto prev_it = std::prev(it); !prev_it->in_use) {
                            prev_it->size += it->size;
                            block.allocations.erase(it);
                            return;
                        }
                    }
                    return;
                }
            }
        }
    }
}

auto gse::vulkan::allocator::clean_up() -> void {
    std::lock_guard lock(m_mutex);

    if (!m_device) {
        return;
    }

    std::uint32_t leaked_sub_allocations = 0;
    for (const auto& [memory_type_index, blocks] : m_pools | std::views::values) {
        for (const auto& block : blocks) {
            for (const auto& sub : block.allocations) {
                if (sub.in_use) {
                    ++leaked_sub_allocations;
                }
            }
        }
    }

    if (leaked_sub_allocations > 0) {
        log::println(log::level::error, log::category::vulkan_memory, "{} sub-allocations still in use at allocator cleanup!", leaked_sub_allocations);
        log::println(log::level::error, log::category::vulkan_memory, "Resources are being destroyed after their memory is freed.");
        log::println(log::level::error, log::category::vulkan_memory, "Destroy all vulkan::image_resource and vulkan::buffer_resource instances before the allocator.");

        if (m_tracking_enabled && !m_live_allocations.empty()) {
            log::println(log::level::error, log::category::vulkan_memory, "Tracked allocations still alive:");
            for (const auto& [id, debug] : m_live_allocations) {
                log::println(
                    log::level::error,
                    log::category::vulkan_memory,
                    "- [{}] '{}' created at {}:{}:{}",
                    id,
                    debug.tag.empty() ? "(no tag)" : debug.tag,
                    debug.creation_location.file_name(),
                    debug.creation_location.line(),
                    debug.creation_location.function_name()
                );
            }
        } else if (!m_tracking_enabled) {
            log::println(log::level::warning, log::category::vulkan_memory, "Enable 'Vulkan.Track Allocations' for detailed allocation info.");
        }

        assert(
            false,
            std::source_location::current(),
            "Allocator cleanup called with {} leaked sub-allocations - destroy resources before the allocator!",
            leaked_sub_allocations
        );
    }

    m_cleaned_up = true;

    for (auto& [memory_type_index, blocks] : m_pools | std::views::values) {
        for (auto& block : blocks) {
            if (block.mapped) {
                m_device.unmapMemory(block.memory);
                block.mapped = nullptr;
            }
            if (block.memory) {
                m_device.freeMemory(block.memory, nullptr);
            }
        }
    }
    m_pools.clear();
}

auto gse::vulkan::allocator::live_allocation_count() const -> std::uint32_t {
    return m_live_allocation_count.load();
}

auto gse::vulkan::allocator::tracking_enabled() const -> bool {
    return m_tracking_enabled;
}

auto gse::vulkan::allocator::memory_flag_preferences(const vk::BufferUsageFlags usage) -> std::vector<vk::MemoryPropertyFlags> {
    using mpf = vk::MemoryPropertyFlagBits;

    if (usage & vk::BufferUsageFlagBits::eVertexBuffer || usage & vk::BufferUsageFlagBits::eIndexBuffer) {
        return {
            mpf::eHostVisible | mpf::eHostCoherent | mpf::eDeviceLocal,
            mpf::eHostVisible | mpf::eHostCoherent,
            mpf::eDeviceLocal,
        };
    }

    if (usage & vk::BufferUsageFlagBits::eUniformBuffer) {
        return {
            mpf::eHostVisible | mpf::eHostCoherent,
            mpf::eHostVisible,
        };
    }

    if (usage & vk::BufferUsageFlagBits::eStorageBuffer) {
        return {
            mpf::eHostVisible | mpf::eHostCoherent,
            mpf::eDeviceLocal,
        };
    }

    if (usage & vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR) {
        return { mpf::eDeviceLocal };
    }

    return { mpf::eHostVisible | mpf::eHostCoherent };
}

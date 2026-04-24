export module gse.gpu:device_bootstrap;

import std;
import vulkan;

import :vulkan_runtime;
import :vulkan_allocator;
import :descriptor_heap;
import :descriptor_buffer_types;
import :gpu_command_pools;

import gse.os;
import gse.save;

export namespace gse::vulkan {
    struct bootstrap_result {
        instance_config instance;
        device_config device;
        std::unique_ptr<allocator> alloc;
        queue_config queue;
        command_config command;
        worker_command_pools worker_pools;
        std::unique_ptr<descriptor_heap> desc_heap;
        descriptor_buffer_properties desc_buf_props;
        vk::Format surface_format;
        bool video_encode_enabled = false;
    };

    auto bootstrap_device(
        const window& win,
        save::state& save
    ) -> bootstrap_result;

    auto find_queue_families(
        const vk::raii::PhysicalDevice& device,
        const vk::raii::SurfaceKHR& surface
    ) -> queue_family;
}

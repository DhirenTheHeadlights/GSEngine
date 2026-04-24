module;

export module gse.gpu:descriptor_buffer_types;

export import vulkan;

export namespace gse::vulkan {
    struct descriptor_buffer_properties {
        vk::DeviceSize offset_alignment = 0;
        vk::DeviceSize uniform_buffer_descriptor_size = 0;
        vk::DeviceSize storage_buffer_descriptor_size = 0;
        vk::DeviceSize sampled_image_descriptor_size = 0;
        vk::DeviceSize sampler_descriptor_size = 0;
        vk::DeviceSize combined_image_sampler_descriptor_size = 0;
        vk::DeviceSize storage_image_descriptor_size = 0;
        vk::DeviceSize input_attachment_descriptor_size = 0;
        vk::DeviceSize acceleration_structure_descriptor_size = 0;
        bool push_descriptors_supported = false;
        bool bufferless_push_descriptors = false;
    };
}

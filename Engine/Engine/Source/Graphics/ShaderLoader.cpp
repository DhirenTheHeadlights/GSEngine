module;

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

module gse.graphics.shader_loader;

import std;
import vulkan_hpp;

import gse.core.config;
import gse.graphics.shader;
import gse.platform;

struct shader_info_hash {
    using is_transparent = void;
    auto operator()(const gse::shader::info& s) const -> size_t {
        return std::hash<std::string>{}(s.name);
    }
    auto operator()(const std::string_view s) const -> size_t {
        return std::hash<std::string_view>{}(s);
    }
	auto operator()(const std::pair<std::filesystem::path, std::filesystem::path>& paths) const -> size_t {
        return std::hash<std::string>{}(paths.first.string() + paths.second.string());
    }
};

struct shader_info_equal {
    using is_transparent = void;
    auto operator()(const gse::shader::info& lhs, const gse::shader::info& rhs) const -> bool {
        return lhs.name == rhs.name;
    }
    auto operator()(const gse::shader::info& lhs, const std::string_view rhs) const -> bool {
        return lhs.name == rhs;
    }
    auto operator()(const std::string_view lhs, const gse::shader::info& rhs) const -> bool {
        return lhs == rhs.name;
    }
    auto operator()(const gse::shader::info& lhs, const std::pair<std::filesystem::path, std::filesystem::path>& paths) const -> bool {
        return lhs.vert_path == paths.first && lhs.frag_path == paths.second;
    }
    auto operator()(const std::pair<std::filesystem::path, std::filesystem::path>& paths, const gse::shader::info& rhs) const -> bool {
        return rhs.vert_path == paths.first && rhs.frag_path == paths.second;
    }
};

std::unordered_map<gse::shader::info, gse::shader, shader_info_hash, shader_info_equal> g_shaders;
std::unordered_map<descriptor_layout, struct gse::shader::layout> g_layouts;

constexpr int max_lights = 10;

constexpr auto vs = vk::ShaderStageFlagBits::eVertex;
constexpr auto fs = vk::ShaderStageFlagBits::eFragment;

auto create_layout(const vk::Device device, const std::vector<vk::DescriptorSetLayoutBinding>& bindings) -> vk::DescriptorSetLayout {
    return device.createDescriptorSetLayout({
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    });
}

auto init_descriptor_layouts(const vk::raii::Device& device) -> void {
    constexpr int max_lights = 10;

    auto create_layout = [&](
        struct gse::shader::layout& layout,
        const gse::shader::set::binding_type type,
        const std::vector<vk::DescriptorSetLayoutBinding>& bindings
        ) -> void {
    	vk::raii::DescriptorSetLayout descriptor_set_layout = device.createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo{
				.flags = type == gse::shader::set::binding_type::push ? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR : vk::DescriptorSetLayoutCreateFlags(),
        		.bindingCount = static_cast<uint32_t>(bindings.size()),
        		.pBindings = bindings.data()
            }
        );

		std::vector<struct gse::shader::binding> shader_bindings;
		shader_bindings.reserve(bindings.size());

        for (const auto& binding : bindings) {
            shader_bindings.emplace_back(std::string(), binding, std::nullopt);
		}

        layout.sets[type] = {
            .type = type,
            .layout = std::move(descriptor_set_layout),
            .bindings = std::move(shader_bindings)
        };
    };

    g_layouts.clear();

    struct gse::shader::layout std_3d;

    create_layout(std_3d, gse::shader::set::binding_type::persistent, {
        { 0, vk::DescriptorType::eUniformBuffer,        1, vs },
        { 1, vk::DescriptorType::eUniformBuffer,        1, vs },
        { 2, vk::DescriptorType::eCombinedImageSampler, 1, fs },
        { 3, vk::DescriptorType::eStorageBuffer,        1, fs },
        { 4, vk::DescriptorType::eCombinedImageSampler, 1, fs },
        { 5, vk::DescriptorType::eCombinedImageSampler, 1, fs },
        { 6, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		});

	g_layouts.emplace(descriptor_layout::standard_3d, std::move(std_3d));

    struct gse::shader::layout def_3d;

    create_layout(def_3d, gse::shader::set::binding_type::persistent, {
        { 0, vk::DescriptorType::eInputAttachment,      1, fs }, // g_position
        { 1, vk::DescriptorType::eInputAttachment,      1, fs }, // g_normal
        { 2, vk::DescriptorType::eInputAttachment,      1, fs }, // g_albedo_spec
        { 3, vk::DescriptorType::eCombinedImageSampler, max_lights, fs },
        { 4, vk::DescriptorType::eCombinedImageSampler, max_lights, fs },
        { 5, vk::DescriptorType::eUniformBuffer,        1, fs }, // light_space_matrix
        { 6, vk::DescriptorType::eCombinedImageSampler, 1, fs }, // diffuse_texture
        { 7, vk::DescriptorType::eCombinedImageSampler, 1, fs }, // environment_map
		{ 8, vk::DescriptorType::eStorageBuffer,        1, fs }, // light buffer
		});

	g_layouts.emplace(descriptor_layout::deferred_3d, std::move(def_3d));

    struct gse::shader::layout for_3d;

    create_layout(for_3d, gse::shader::set::binding_type::persistent, {
		{ 4, vk::DescriptorType::eStorageBuffer, 1, fs }
    });

    create_layout(for_3d, gse::shader::set::binding_type::push, {
        { 0, vk::DescriptorType::eUniformBuffer,        1, vs },
        { 1, vk::DescriptorType::eUniformBuffer,        1, vs },
        { 2, vk::DescriptorType::eCombinedImageSampler, 1, fs },
        { 3, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		});

	g_layouts.emplace(descriptor_layout::forward_3d, std::move(for_3d));

    struct gse::shader::layout for_2d;

    create_layout(for_2d, gse::shader::set::binding_type::persistent, {
        { 0, vk::DescriptorType::eUniformBuffer,        1, vs },
        { 1, vk::DescriptorType::eCombinedImageSampler, 1, fs },
        { 2, vk::DescriptorType::eUniformBuffer,        1, fs },
        });

	g_layouts.emplace(descriptor_layout::forward_2d, std::move(for_2d));

	struct gse::shader::layout post_process;
    
    create_layout(post_process, gse::shader::set::binding_type::persistent, {
        { 0, vk::DescriptorType::eCombinedImageSampler, 1, fs },
        { 1, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		});

	g_layouts.emplace(descriptor_layout::post_process, std::move(post_process));
}

auto gse::shader_loader::load_shaders(const vk::raii::Device& device) -> void {
    init_descriptor_layouts(device);

    const std::unordered_map<std::string, descriptor_layout> layouts = compile_shaders();

	const auto shader_path = config::shader_spirv_path;
	std::unordered_map<std::string, gse::shader::info> shader_files;

	assert(exists(shader_path) && is_directory(shader_path), "Shader directory does not exist");

    for (const auto& entry : std::filesystem::recursive_directory_iterator(shader_path)) {
        if (!entry.is_regular_file()) continue;

        const std::filesystem::path file_path = entry.path();
        const std::string filename = file_path.filename().string();
        const std::string extension = file_path.extension().string();

        if (extension == ".spv") {
            std::string base_name = file_path.stem().stem().string();
            if (filename.find(".vert.spv") != std::string::npos) {
                shader_files[base_name].vert_path = file_path;
            }
            else if (filename.find(".frag.spv") != std::string::npos) {
                shader_files[base_name].frag_path = file_path;
            }
            shader_files[base_name].name = base_name;
        }
    }

	for (const auto& info : shader_files | std::views::values) {
		assert(!info.vert_path.empty() && !info.frag_path.empty(), "Missing shader file");
		std::cout << "Loading shader: " << info.name << '\n';

        auto layout_type = layouts.at(info.name);
		auto* layout = &g_layouts.at(layout_type);

        g_shaders.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(info),
            std::forward_as_tuple(
                device,
                info.vert_path,
                info.frag_path,
                layout
            )
        );

		std::cout << "Loaded shader: " << info.name << '\n';
	}
}

auto gse::shader_loader::get_shader(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path) -> const shader& {
	const auto it = g_shaders.find(std::make_pair(vert_path, frag_path));
	assert(it != g_shaders.end(), "Shader not found");
	return it->second;
}

auto gse::shader_loader::get_shader(const std::string_view name) -> const shader& {
	const auto it = g_shaders.find(name);
	assert(it != g_shaders.end(), "Shader not found");
	return it->second;
}

auto gse::shader_loader::compile_shaders() -> std::unordered_map<std::string, descriptor_layout> {
	const auto root_path = config::shader_raw_path;
	const auto destination_path = config::shader_spirv_path;

	std::unordered_map<std::string, descriptor_layout> layouts;

	glslang::InitializeProcess();

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path)) {
		if (!entry.is_regular_file()) continue;

		const auto ext = entry.path().extension().string();
		if (ext != ".vert" && ext != ".frag" && ext != ".comp" && ext != ".geom" && ext != ".tesc" && ext != ".tese") continue;

        if (ext == ".vert" || ext == ".frag") {
            /// Grab descriptor layout type. Shader sets in this format: 'layout (constant_id = 99) const int descriptor_layout_type = 1;'
            std::ifstream file(entry.path());
            std::string line;

            const std::string token = "const int descriptor_layout_type =";

            while (std::getline(file, line)) {
                if (auto pos = line.find(token); pos != std::string::npos) {
                    pos += token.size();
                    auto end = line.find(';', pos);
                    std::string value_str = line.substr(pos, end - pos);
                    value_str.erase(std::ranges::remove_if(value_str, isspace).begin(), value_str.end());
                    int layout_value = std::stoi(value_str);
                    layouts[entry.path().stem().stem().string()] = static_cast<descriptor_layout>(layout_value);
                    break;
                }
            }
        }

        const auto source_path = entry.path();
        const auto destination_relative = relative(source_path, root_path);
        const auto destination_file = destination_path / (destination_relative.string() + ".spv");

        if (exists(destination_file)) {
            auto src_time = last_write_time(source_path);
            auto dst_time = last_write_time(destination_file);
			if (src_time <= dst_time) {
				std::cout << "Shader already compiled: " << destination_file.filename().string() << '\n';
			}
		} else {
            if (auto dst_dir = destination_file.parent_path(); !exists(dst_dir)) {
                create_directories(dst_dir);
            }
		}

        EShLanguage stage;

		if (ext == ".vert") stage = EShLangVertex;
		else if (ext == ".frag") stage = EShLangFragment;
		else if (ext == ".comp") stage = EShLangCompute;
		else if (ext == ".geom") stage = EShLangGeometry;
		else if (ext == ".tesc") stage = EShLangTessControl;
		else if (ext == ".tese") stage = EShLangTessEvaluation;
        else assert(false, std::format("Unknown shader extension: {}", ext));

		std::ifstream in(source_path, std::ios::ate | std::ios::binary);
		assert(in.is_open(), std::format("Failed to open shader file: {}", source_path.string().c_str()));

		size_t n = in.tellg();
		std::string source(n, '\0');
		in.seekg(0);
		in.read(source.data(), n);

		if (source.size() > 3 && source[0] == 0xEF && source[1] == 0xBB && source[2] == 0xBF) {
			source.erase(0, 3);
		}

        const char* code = source.c_str();

        glslang::TShader shader(stage);

        shader.setStrings(&code, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        bool ok = shader.parse(
            GetDefaultResources(),
            450,
            false,
            EShMsgDefault
        );

        if (!ok) {
            auto info = shader.getInfoLog();
            auto debug = shader.getInfoDebugLog();

            auto msg = std::format(
                "File ({}) GLSL parse error:\n{}\n{}",
                source_path.filename().string(), info, debug
            );
            assert(false, msg);
        }

        glslang::TProgram program;
        program.addShader(&shader);
        assert(program.link(EShMsgDefault), std::format("GLSL link error:\n{}", program.getInfoLog()));

        std::vector<uint32_t> spirv;
        GlslangToSpv(*program.getIntermediate(stage), spirv);

        std::ofstream out(destination_file, std::ios::binary);
        assert(out.is_open(), std::format("Failed to write compiled SPIR-V: {}", destination_file.string().c_str()));
        out.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));

		std::cout << "Compiled shader: " << destination_file.filename().string() << "\n";
    }

    glslang::FinalizeProcess();

    return layouts;
}

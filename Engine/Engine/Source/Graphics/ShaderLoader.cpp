module;

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

module gse.graphics.shader_loader;

import std;
import vulkan_hpp;

import gse.core.config;
import gse.graphics.shader;
import gse.platform.assert;
import gse.platform.context;

struct shader_info {
    std::string name;
    std::filesystem::path vert_path;
    std::filesystem::path frag_path;
};

struct descriptor_layout_info {
    vk::DescriptorSetLayout layout;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
};

struct shader_info_hash {
    using is_transparent = void;
    auto operator()(const shader_info& s) const -> size_t {
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
    auto operator()(const shader_info& lhs, const shader_info& rhs) const -> bool {
        return lhs.name == rhs.name;
    }
    auto operator()(const shader_info& lhs, const std::string_view rhs) const -> bool {
        return lhs.name == rhs;
    }
    auto operator()(const std::string_view lhs, const shader_info& rhs) const -> bool {
        return lhs == rhs.name;
    }
    auto operator()(const shader_info& lhs, const std::pair<std::filesystem::path, std::filesystem::path>& paths) const -> bool {
        return lhs.vert_path == paths.first && lhs.frag_path == paths.second;
    }
    auto operator()(const std::pair<std::filesystem::path, std::filesystem::path>& paths, const shader_info& rhs) const -> bool {
        return rhs.vert_path == paths.first && rhs.frag_path == paths.second;
    }
};

std::unordered_map<shader_info, gse::shader, shader_info_hash, shader_info_equal> g_shaders;
std::unordered_map<descriptor_layout, descriptor_layout_info> g_layouts;

constexpr int max_lights = 10;

constexpr auto vs = vk::ShaderStageFlagBits::eVertex;
constexpr auto fs = vk::ShaderStageFlagBits::eFragment;

auto create_layout(const vk::Device device, const std::vector<vk::DescriptorSetLayoutBinding>& bindings) -> vk::DescriptorSetLayout {
    return device.createDescriptorSetLayout({
		{},
		static_cast<std::uint32_t>(bindings.size()),
		bindings.data()
		});
}

auto init_descriptor_layouts() -> void {
    const auto& device = gse::vulkan::get_device_config().device;

    constexpr auto vs = vk::ShaderStageFlagBits::eVertex;
    constexpr auto fs = vk::ShaderStageFlagBits::eFragment;
    constexpr int max_lights = 10;

    auto create_layout = [&](std::vector<vk::DescriptorSetLayoutBinding> bindings) {
        const vk::DescriptorSetLayout layout = device.createDescriptorSetLayout({
            {},
        	static_cast<uint32_t>(bindings.size()),
        	bindings.data()
            });

        return descriptor_layout_info{ layout, std::move(bindings) };
        };

    g_layouts = {
        { descriptor_layout::standard_3d, create_layout({
            { 0, vk::DescriptorType::eUniformBuffer,        1, vs },
            { 1, vk::DescriptorType::eUniformBuffer,        1, vs },
            { 2, vk::DescriptorType::eCombinedImageSampler, 1, fs },
            { 3, vk::DescriptorType::eStorageBuffer,        1, fs },
			{ 4, vk::DescriptorType::eCombinedImageSampler, 1, fs },
			{ 5, vk::DescriptorType::eCombinedImageSampler, 1, fs },
            { 6, vk::DescriptorType::eCombinedImageSampler, 1, fs },
        })},

        { descriptor_layout::deferred_3d, create_layout({
	      { 0, vk::DescriptorType::eCombinedImageSampler, 1, fs },              // g_position
	      { 1, vk::DescriptorType::eCombinedImageSampler, 1, fs },              // g_normal
	      { 2, vk::DescriptorType::eCombinedImageSampler, 1, fs },              // g_albedo_spec
	      { 3, vk::DescriptorType::eCombinedImageSampler, max_lights, fs },
	      { 4, vk::DescriptorType::eCombinedImageSampler, max_lights, fs },
		  { 5, vk::DescriptorType::eUniformBuffer,        1, fs },              // light_space_matrix
	      { 6, vk::DescriptorType::eCombinedImageSampler, 1, fs },              // diffuse_texture
	      { 7, vk::DescriptorType::eCombinedImageSampler, 1, fs },              // environment_map
		  { 8, vk::DescriptorType::eStorageBuffer,        1, fs },              // light buffer
        })},

        { descriptor_layout::forward_3d, create_layout({
            { 0, vk::DescriptorType::eUniformBuffer,        1, vs },
            { 1, vk::DescriptorType::eUniformBuffer,        1, vs },
            { 2, vk::DescriptorType::eCombinedImageSampler, 3, fs },
            { 3, vk::DescriptorType::eCombinedImageSampler, 1, fs },
			{ 4, vk::DescriptorType::eStorageBuffer,        1, fs },
        })},

        { descriptor_layout::forward_2d, create_layout({
            { 0, vk::DescriptorType::eUniformBuffer,        1, vs },
            { 1, vk::DescriptorType::eCombinedImageSampler, 1, fs },
            { 2, vk::DescriptorType::eUniformBuffer,        1, fs },
        })},

        { descriptor_layout::post_process, create_layout({
            { 0, vk::DescriptorType::eCombinedImageSampler, 1, fs },
            { 1, vk::DescriptorType::eCombinedImageSampler, 1, fs },
        })},
    };
}

auto gse::shader_loader::load_shaders() -> void {
    init_descriptor_layouts();

    const std::unordered_map<std::string, descriptor_layout> layouts = compile_shaders();

	const auto shader_path = config::shader_spirv_path;
	std::unordered_map<std::string, shader_info> shader_files;

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
		auto [layout, bindings] = g_layouts.at(layout_type);
        const vk::DescriptorSetLayout* layout_ptr =
            layout_type == descriptor_layout::custom
            ? nullptr
			: &layout;

        g_shaders.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(info),
            std::forward_as_tuple(
                info.vert_path,
                info.frag_path,
                layout_ptr,
				bindings
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
			source.erase(0, 3); // Remove BOM
		}

        const char* code = source.c_str();

        glslang::TShader shader(stage);

        shader.setStrings(&code, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        bool ok = shader.parse(GetDefaultResources(),
            450,
            /*forwardCompatible=*/false,
            EShMsgDefault);

        if (!ok) {
            auto info = shader.getInfoLog();       // now populated
            auto debug = shader.getInfoDebugLog();  // now populated

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

		std::cout << "Compiled shader: " << destination_file.filename().string() << '\n';
    }

    glslang::FinalizeProcess();

    return layouts;
}

auto gse::shader_loader::get_descriptor_layout(const descriptor_layout layout_type) -> const vk::DescriptorSetLayout* {
    return &g_layouts[layout_type].layout;
}

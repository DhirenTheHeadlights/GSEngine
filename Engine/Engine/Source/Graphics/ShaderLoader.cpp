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

enum class descriptor_layout : std::uint8_t {
    standard_3d     = 1,
    deferred_3d     = 2,
    forward_3d      = 3,
    forward_2d      = 4,
    post_process    = 5,
    custom          = 99
};

auto create_descriptor_layouts() -> std::unordered_map<descriptor_layout, vk::DescriptorSetLayout> {
	return {
		{ descriptor_layout::standard_3d,
		   gse::vulkan::get_device_config().device.createDescriptorSetLayout({
			   {},
			   3,
			   std::array{
				   vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },  // Global: ViewProj
				   vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },                                       // Per-object: Model matrix
				   vk::DescriptorSetLayoutBinding{ 2, vk::DescriptorType::eCombinedImageSampler, 3, vk::ShaderStageFlagBits::eFragment }                               // Textures (Diffuse, Normal, Roughness)
			   }.data()
		   })
	   },
	   { descriptor_layout::deferred_3d,
		   gse::vulkan::get_device_config().device.createDescriptorSetLayout({
			   {},
			   3,
			   std::array{
				   vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },    // Global: ViewProj
				   vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },                                         // Per-object: Model matrix
				   vk::DescriptorSetLayoutBinding{ 2, vk::DescriptorType::eCombinedImageSampler, 3, vk::ShaderStageFlagBits::eFragment }                                 // Albedo, Normal, Depth (G-buffer)
			   }.data()
		   })
	   },
	   { descriptor_layout::forward_3d,
		   gse::vulkan::get_device_config().device.createDescriptorSetLayout({
			   {},
			   4,
			   std::array{
				   vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },    // Global: ViewProj, Lights
				   vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },                                         // Per-object: Model matrix
				   vk::DescriptorSetLayoutBinding{ 2, vk::DescriptorType::eCombinedImageSampler, 3, vk::ShaderStageFlagBits::eFragment },                                // Textures (Diffuse, Normal, Roughness)
				   vk::DescriptorSetLayoutBinding{ 3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment }                                 // Shadow map (if needed)
			   }.data()
		   })
	   },
	   { descriptor_layout::forward_2d,
		   gse::vulkan::get_device_config().device.createDescriptorSetLayout({
			   {},
			   2,
			   std::array{
				   vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },                                         // Global 2D Transform (Ortho Projection)
				   vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment }                                 // Texture (UI, Sprite)
			   }.data()
		   })
	   },
	   { descriptor_layout::post_process,
		   gse::vulkan::get_device_config().device.createDescriptorSetLayout({
			   {},
			   1,
			   std::array{
				   vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment }                                 // Screen Texture (HDR, Bloom, etc.)
			   }.data()
		   })
	   },
	   { descriptor_layout::custom,
		   gse::vulkan::get_device_config().device.createDescriptorSetLayout({
			   {},
			   0, nullptr // Empty layout; users define custom sets dynamically.
		   })
	   }
	};
}

namespace gse::shader_loader {
    auto compile_shaders() -> std::vector<descriptor_layout>;
}

auto gse::shader_loader::load_shaders() -> void {
	create_descriptor_layouts();

	const auto shader_path = config::shader_spirv_path;
	std::unordered_map<std::string, shader_info> shader_files;

	perma_assert(exists(shader_path) && is_directory(shader_path), "Shader directory does not exist");

    for (const auto& entry : std::filesystem::directory_iterator(shader_path)) {
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
		perma_assert(!info.vert_path.empty() && !info.frag_path.empty(), "Missing shader file");
		g_shaders.emplace(std::piecewise_construct, std::forward_as_tuple(info), std::forward_as_tuple(info.vert_path, info.frag_path));
		std::cout << "Loaded shader: " << info.name << '\n';
	}
}

auto gse::shader_loader::get_shader(const std::filesystem::path& vert_path, const std::filesystem::path& frag_path) -> const shader& {
	const auto it = g_shaders.find(std::make_pair(vert_path, frag_path));
	perma_assert(it != g_shaders.end(), "Shader not found");
	return it->second;
}

auto gse::shader_loader::get_shader(const std::string_view name) -> const shader& {
	const auto it = g_shaders.find(name);
	perma_assert(it != g_shaders.end(), "Shader not found");
	return it->second;
}

auto gse::shader_loader::compile_shaders() -> std::vector<descriptor_layout> {
	const auto root_path = config::shader_raw_path;
	const auto destination_path = config::shader_spirv_path;

	std::vector<descriptor_layout> layouts;

	glslang::InitializeProcess();

    auto infer_shader_stage = [](const std::filesystem::path& path) -> EShLanguage {
        
        return {};
        };

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path)) {
        /// Grab descriptor layout type. Shader sets in this format: 'layout (constant_id = 99) const int descriptor_layout_type = 1;'
		std::ifstream file(entry.path());
		std::string line;

        constexpr std::string token = "const int descriptor_layout_type =";

        while (std::getline(file, line)) {
	        if (auto pos = line.find(token); pos != std::string::npos) {
                pos += token.size();
                auto end = line.find(';', pos);
                std::string value_str = line.substr(pos, end - pos);
                value_str.erase(std::ranges::remove_if(value_str, isspace).begin(), value_str.end());
                int layout_value = std::stoi(value_str);
                layouts.push_back(static_cast<descriptor_layout>(layout_value));
                break;
            }
        }

        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        if (ext != ".vert" && ext != ".frag" && ext != ".comp" && ext != ".geom" && ext != ".tesc" && ext != ".tese") continue;

        const auto source_path = entry.path();
        const auto destination_relative = relative(source_path, root_path);
        const auto destination_file = destination_path / (destination_relative.string() + ".spv");

        create_directories(destination_file.parent_path());

        EShLanguage stage;

		if (ext == ".vert") stage = EShLangVertex;
		else if (ext == ".frag") stage = EShLangFragment;
		else if (ext == ".comp") stage = EShLangCompute;
		else if (ext == ".geom") stage = EShLangGeometry;
		else if (ext == ".tesc") stage = EShLangTessControl;
		else if (ext == ".tese") stage = EShLangTessEvaluation;
        else perma_assert(false, "Unknown shader extension: {}", ext.c_str());

        const char* code = source_path.string().c_str();

        glslang::TShader shader(stage);

        shader.setStrings(&code, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        perma_assert(shader.parse(GetDefaultResources(), 100, false, EShMsgDefault), "GLSL parse error:\n{}", shader.getInfoLog());

        glslang::TProgram program;
        program.addShader(&shader);
        perma_assert(program.link(EShMsgDefault), "GLSL link error:\n{}", program.getInfoLog());

        std::vector<uint32_t> spirv;
        GlslangToSpv(*program.getIntermediate(stage), spirv);

        std::ofstream out(destination_file, std::ios::binary);
        perma_assert(out.is_open(), "Failed to write compiled SPIR-V: {}", destination_file.string().c_str());
        out.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));

		std::cout << "Compiled shader: " << destination_file.string() << '\n';
    }

    glslang::FinalizeProcess();
}
module;

#include <glad/glad.h>
#include "json.hpp"
#include "imgui.h"
#include "Core/ResourcePaths.h"
#include "iostream"

export module gse.graphics.renderer3d;

import gse.graphics.camera;

export namespace gse::renderer3d {
	auto initialize() -> void;
	auto initialize_objects() -> void;
	auto render() -> void;

	auto get_camera() -> camera&;
}

import gse.core.json_parser;
import gse.core.id;
import gse.core.object_registry;
import gse.graphics.debug;
import gse.graphics.render_component;
import gse.graphics.mesh;
import gse.graphics.shader;
import gse.graphics.cube_map;
import gse.graphics.material;
import gse.graphics.model;
import gse.graphics.model_loader;
import gse.graphics.light;
import gse.graphics.light_source_component;
import gse.graphics.point_light;
import gse.physics.motion_component;
import gse.physics.math;
import gse.platform.glfw.window;
import gse.platform.glfw.input;
import gse.platform.glfw.error_reporting;
import gse.graphics.texture_loader;

gse::camera g_camera;

std::unordered_map<std::string, gse::material> g_materials;
std::unordered_map<std::string, gse::shader> g_deferred_rendering_shaders;
std::unordered_map<std::string, gse::shader> g_forward_rendering_shaders;
std::unordered_map<std::string, gse::shader> g_lighting_shaders;
std::unordered_map<std::string, gse::shader> g_texture_shaders;

GLuint g_g_buffer = 0;
GLuint g_g_position = 0;
GLuint g_g_normal = 0;
GLuint g_g_albedo_spec = 0;
GLuint g_ssbo_lights = 0;
GLuint g_light_space_block_ubo = 0;

GLuint g_hdr_fbo = 0;
GLuint g_hdr_color_buffer[2] = { 0, 0 };
GLuint g_blur_fbo[2] = { 0, 0 };
GLuint g_blur_color_buffer[2] = { 0, 0 };

float g_hdr_exposure = 0.5f;
float g_bloom_intensity = 1.f;
float g_bloom_threshold = 0.25f;
float g_blur_radius = 1.0f;

gse::cube_map g_reflection_cube_map;

float g_shadow_width = 4096;
float g_shadow_height = 4096;

bool g_depth_map_debug = false;
bool g_brightness_extraction_debug = false;
bool g_hdr = true;
bool g_bloom = true;
int g_amount_of_blur_passes_in_each_direction = 5;

auto load_shaders(const std::string& shader_path, const std::string& shader_file_name,
	std::unordered_map<std::string, gse::shader>& shaders) -> void {
	gse::json_parse::parse(
		gse::json_parse::load_json(shader_path + shader_file_name),
		[&](const std::string& key, const nlohmann::json& value) {
			shaders.emplace(key, gse::shader(shader_path + value["vertex"].get<std::string>(),
				shader_path + value["fragment"].get<std::string>()));
		}
	);
}

auto gse::renderer3d::initialize() -> void {
	enable_report_gl_errors();

	const std::string shader_path = std::string(ENGINE_RESOURCES_PATH) + "Shaders/";

	const std::string object_shaders_path = shader_path + "Object/";
	json_parse::parse(
		json_parse::load_json(object_shaders_path + "object_shaders.json"),
		[&](const std::string& key, const nlohmann::json& value) {
			g_materials.emplace(key, material(object_shaders_path + value["vertex"].get<std::string>(),
				object_shaders_path + value["fragment"].get<std::string>(), key, ENGINE_RESOURCES_PATH + value["texture"].get<std::string>()));
		}
	);

	load_shaders(shader_path + "DeferredRendering/", "deferred_rendering.json", g_deferred_rendering_shaders);
	load_shaders(shader_path + "ForwardRendering/", "forward_rendering.json", g_forward_rendering_shaders);
	load_shaders(shader_path + "Lighting/", "light_shaders.json", g_lighting_shaders);
	load_shaders(shader_path + "TextureShaders/", "texture_shaders.json", g_texture_shaders);

	const GLsizei screen_width = window::get_frame_buffer_size().x;
	const GLsizei screen_height = window::get_frame_buffer_size().y;

	glGenFramebuffers(1, &g_g_buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, g_g_buffer);

	// Set up G-buffer textures
	glGenTextures(1, &g_g_position);
	glBindTexture(GL_TEXTURE_2D, g_g_position);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screen_width, screen_height, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_g_position, 0);

	glGenTextures(1, &g_g_normal);
	glBindTexture(GL_TEXTURE_2D, g_g_normal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screen_width, screen_height, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g_g_normal, 0);

	glGenTextures(1, &g_g_albedo_spec);
	glBindTexture(GL_TEXTURE_2D, g_g_albedo_spec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g_g_albedo_spec, 0);

	constexpr GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);

	GLuint rbo_depth;
	glGenRenderbuffers(1, &rbo_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screen_width, screen_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Set up the UBO for light space matrices
	glGenBuffers(1, &g_light_space_block_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, g_light_space_block_ubo);
	constexpr size_t buffer_size = sizeof(mat4) * 10; // MAX_LIGHTS is 10 in the shader
	glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(buffer_size), nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 4, g_light_space_block_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// Initialize SSBO for lights
	glGenBuffers(1, &g_ssbo_lights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_ssbo_lights);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, g_ssbo_lights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set up HDR frame buffer
	glGenFramebuffers(1, &g_hdr_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, g_hdr_fbo);

	glGenTextures(2, g_hdr_color_buffer);
	for (int i = 0; i < 2; i++) {
		glBindTexture(GL_TEXTURE_2D, g_hdr_color_buffer[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screen_width, screen_height, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Optional but recommended
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Optional but recommended
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, g_hdr_color_buffer[i], 0);
	}

	constexpr GLenum hdr_draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, hdr_draw_buffers);

	GLuint hdr_rbo_depth;
	glGenRenderbuffers(1, &hdr_rbo_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, hdr_rbo_depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screen_width, screen_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, hdr_rbo_depth);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Set up bloom frame buffers
	glGenFramebuffers(2, g_blur_fbo);
	glGenTextures(2, g_blur_color_buffer);
	for (int i = 0; i < 2; i++) {
		glBindFramebuffer(GL_FRAMEBUFFER, g_blur_fbo[i]);

		glBindTexture(GL_TEXTURE_2D, g_blur_color_buffer[i]);
		glTexImage2D(
			GL_TEXTURE_2D, 0, GL_RGBA16F,
			screen_width, screen_height, 0,
			GL_RGBA, GL_FLOAT, nullptr
		);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Optional but recommended
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Optional but recommended
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_blur_color_buffer[i], 0);

	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	const auto& blur_shader = g_deferred_rendering_shaders["GaussianBlur"];

	blur_shader.use();
	blur_shader.set_int("image", 0);
	blur_shader.set_bool("horizontal", true);
	blur_shader.set_float("bloom_intensity", g_bloom_intensity);
	blur_shader.set_float("blur_radius", g_blur_radius);
	const auto& lighting_pass_shader = g_deferred_rendering_shaders["LightingPass"];

	lighting_pass_shader.use();
	lighting_pass_shader.set_int("gPosition", 0);
	lighting_pass_shader.set_int("gNormal", 1);
	lighting_pass_shader.set_int("gAlbedoSpec", 2);
	lighting_pass_shader.set_bool("depthMapDebug", g_depth_map_debug);
	lighting_pass_shader.set_bool("brightness_extraction_debug", g_brightness_extraction_debug);
	lighting_pass_shader.set_int("diffuseTexture", 3);

	g_reflection_cube_map.create(1024);

	const auto& post_processing_shader = g_deferred_rendering_shaders["PostProcessing"];

	post_processing_shader.use();
	post_processing_shader.set_int("scene", 0);
	post_processing_shader.set_bool("hdr", g_hdr);
	post_processing_shader.set_float("exposure", g_hdr_exposure);
	post_processing_shader.set_int("bloomBlur", 1);
	post_processing_shader.set_bool("bloom", g_bloom);
}

auto gse::renderer3d::initialize_objects() -> void {
	for (const auto& light_source_components = registry::get_components<light_source_component>(); auto& light_source_component : light_source_components) {
		for (const auto& light : light_source_component.get_lights()) {
			if (const auto point_light_ptr = dynamic_cast<point_light*>(light); point_light_ptr) {
				point_light_ptr->get_shadow_map().create(static_cast<int>((g_shadow_width + g_shadow_height) / 2.f), true);
			}
			else {
				GLuint depth_map, depth_map_fbo;
				glGenTextures(1, &depth_map);
				glBindTexture(GL_TEXTURE_2D, depth_map);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, static_cast<GLsizei>(g_shadow_width), static_cast<GLsizei>(g_shadow_height), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
				constexpr float border_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

				glGenFramebuffers(1, &depth_map_fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, depth_map_fbo);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_map, 0);
				glDrawBuffer(GL_NONE);
				glReadBuffer(GL_NONE);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				light->set_depth_map(depth_map, depth_map_fbo);
			}
		}
	}
}

auto render_object(const std::uint32_t object_id, const gse::render_queue_entry& entry, const gse::mat4& view_matrix, const gse::mat4& projection_matrix) -> void {
	if (const auto it = g_materials.find(entry.material_key); it != g_materials.end()) {
		gse::mat4 model_matrix = entry.model_matrix;
		if (const auto* motion_component = gse::registry::get_component_ptr<gse::physics::motion_component>(object_id); motion_component) {
			model_matrix = motion_component->get_transformation_matrix();
		}

		//it->second.use(view_matrix, projection_matrix, model_matrix);
//it->second.shader.set_vec3("color", entry.color);

		const auto& texture_shader = g_texture_shaders["DefaultTexture"];
		texture_shader.use();
		texture_shader.set_mat4("view", view_matrix);
		texture_shader.set_mat4("projection", projection_matrix);
		texture_shader.set_mat4("model", model_matrix);
		texture_shader.set_vec3("color", entry.color);
		texture_shader.set_int("texture_diffuse1", 0);
		texture_shader.set_vec3("viewPos", g_camera.get_position().as<gse::units::meters>());

		constexpr GLuint binding_unit = 5;
		g_reflection_cube_map.bind(binding_unit);
		texture_shader.set_int("environmentMap", binding_unit);

		// Entry has no mtl data
		if (entry.material == nullptr) {
			texture_shader.set_bool("usemtl", false);
			texture_shader.set_bool("useDiffuseTexture", true);
			texture_shader.set_bool("useSpecularTexture", false);
			texture_shader.set_bool("useNormalTexture", false);

			for (const auto& texture : entry.texture_ids) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, texture);
			}
		}
		//entry is using mtl textures
		else {
			texture_shader.set_bool("usemtl", true);
			texture_shader.set_int("texture_diffuse1", 0);
			texture_shader.set_bool("useDiffuseTexture", entry.material->diffuse_texture != 0);

			texture_shader.set_int("texture_specular1", 1);
			texture_shader.set_bool("useSpecularTexture", entry.material->specular_texture != 0);

			texture_shader.set_int("ntexture_normal1", 2);
			texture_shader.set_bool("useNormalTexture", entry.material->normal_texture != 0);

			texture_shader.set_vec3("ambient", entry.material->ambient);
			texture_shader.set_vec3("diffuse", entry.material->diffuse);
			texture_shader.set_vec3("specular", entry.material->specular);
			texture_shader.set_vec3("emission", entry.material->emission);
			texture_shader.set_float("shininess", entry.material->shininess);
			texture_shader.set_float("optical_density", entry.material->optical_density);
			texture_shader.set_float("transparency", entry.material->transparency);
			texture_shader.set_int("illumination_model", entry.material->illumination_model);


			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, entry.material->diffuse_texture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, entry.material->specular_texture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, entry.material->normal_texture);
		}

		glBindVertexArray(entry.vao);
		glDrawElements(entry.draw_mode, entry.vertex_count, GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);

		if (it->second.material_texture != 0) {
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
}

auto render_object_forward(const std::uint32_t object_id, const gse::shader& forward_rendering_shader, const gse::render_queue_entry& entry, const gse::mat4& view_matrix, const gse::mat4& projection_matrix) -> void {
	gse::mat4 model_matrix = entry.model_matrix;
	if (const auto* motion_component = gse::registry::get_component_ptr<gse::physics::motion_component>(object_id); motion_component) {
		model_matrix = motion_component->get_transformation_matrix();
	}

	forward_rendering_shader.set_vec3("color", entry.color);
	forward_rendering_shader.set_mat4("model", model_matrix);
	forward_rendering_shader.set_mat4("view", view_matrix);
	forward_rendering_shader.set_mat4("projection", projection_matrix);
	forward_rendering_shader.set_vec3("viewPos", g_camera.get_position().as<gse::units::meters>());

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, g_g_albedo_spec);

	glBindVertexArray(entry.vao);
	glDrawElements(entry.draw_mode, entry.vertex_count, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}

auto render_object(const gse::light_render_queue_entry& entry) -> void {
	if (const auto it = g_lighting_shaders.find(entry.shader_key); it != g_lighting_shaders.end()) {
		it->second.use();
		it->second.set_mat4("model", gse::mat4(1.0f));
		it->second.set_mat4("view", g_camera.get_view_matrix());
		it->second.set_mat4("projection", g_camera.get_projection_matrix());

		it->second.set_vec3("color", entry.shader_entry.color);
		it->second.set_float("intensity", entry.shader_entry.intensity);
	}
	else {
		std::cerr << "Shader program key not found: " << entry.shader_key << '\n';
	}
}

auto render_fullscreen_quad() -> void {
	static unsigned int quad_vao = 0;
	static unsigned int quad_vbo = 0;

	if (quad_vao == 0) {
		constexpr float quad_vertices[] = {
			// Positions   // TexCoords
			-1.0f,  1.0f,  0.0f, 1.0f,
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,

			-1.0f,  1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f
		};

		glGenVertexArrays(1, &quad_vao);
		glGenBuffers(1, &quad_vbo);
		glBindVertexArray(quad_vao);
		glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), static_cast<void*>(nullptr));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
	}

	glBindVertexArray(quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

auto render_lighting_pass(const gse::shader& lighting_shader, const std::vector<gse::light_shader_entry>& light_data, const std::vector<gse::mat4>& light_space_matrices, const std::vector<GLuint>& depth_maps) -> void {
	if (light_data.empty()) {
		return;
	}

	lighting_shader.use();

	// Update SSBO with light data
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_ssbo_lights);
	glBufferData(GL_SHADER_STORAGE_BUFFER, light_data.size() * sizeof(gse::light_shader_entry), light_data.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, g_ssbo_lights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Update UBO with light space matrices
	glBindBuffer(GL_UNIFORM_BUFFER, g_light_space_block_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, light_space_matrices.size() * sizeof(gse::mat4), light_space_matrices.data());
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, g_hdr_fbo);
	constexpr GLenum hdr_draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, hdr_draw_buffers);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	std::vector<GLint> shadow_map_units(depth_maps.size());
	for (size_t i = 0; i < depth_maps.size(); ++i) {
		const GLenum texture_unit = GL_TEXTURE4 + i;
		glActiveTexture(texture_unit);
		glBindTexture(light_data[i].light_type == static_cast<int>(gse::light_type::point) ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, depth_maps[i]);
		shadow_map_units[i] = texture_unit - GL_TEXTURE0;
	}

	lighting_shader.set_int_array("shadowMaps", shadow_map_units.data(), static_cast<unsigned>(shadow_map_units.size()));
	lighting_shader.set_mat4_array("lightSpaceMatrices", light_space_matrices.data(), static_cast<unsigned>(light_space_matrices.size()));
	lighting_shader.set_bool("brightness_extraction_debug", g_brightness_extraction_debug);
	lighting_shader.set_bool("depthMapDebug", g_depth_map_debug);
	lighting_shader.set_float("bloomThreshold", g_bloom_threshold);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_g_position);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, g_g_normal);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, g_g_albedo_spec);

	constexpr GLuint binding_unit = 5;
	g_reflection_cube_map.bind(binding_unit);
	lighting_shader.set_int("environmentMap", binding_unit);

	render_fullscreen_quad();

	glEnable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

auto render_blur_pass(const gse::shader& blur_shader) -> int {
	blur_shader.use();
	blur_shader.set_int("image", 0);
	blur_shader.set_int("blur_amount", g_amount_of_blur_passes_in_each_direction);
	blur_shader.set_float("bloom_intensity", g_bloom_intensity);
	blur_shader.set_float("blur_radius", g_blur_radius);

	bool horizontal = true;
	bool first_iteration = true;
	const GLuint input_texture = g_hdr_color_buffer[1];

	for (int i = 0; i < (2 * g_amount_of_blur_passes_in_each_direction); ++i) {
		glBindFramebuffer(GL_FRAMEBUFFER, g_blur_fbo[horizontal]);
		blur_shader.set_bool("horizontal", horizontal);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, first_iteration ? input_texture : g_blur_color_buffer[!horizontal]);

		render_fullscreen_quad();

		horizontal = !horizontal;
		first_iteration = false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// After completion, 'horizontal' has been toggled amount times.
	// The last written texture is in g_blur_color_buffer[!horizontal].
	return !horizontal;
}

auto render_additional_post_processing(const gse::shader& post_processing_shader, const int current_texture) -> void {
	post_processing_shader.use();
	post_processing_shader.set_int("scene", 0);
	post_processing_shader.set_bool("hdr", g_hdr);
	post_processing_shader.set_float("exposure", g_hdr_exposure);
	post_processing_shader.set_int("bloomBlur", 1);
	post_processing_shader.set_bool("bloom", g_bloom);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_hdr_color_buffer[0]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, g_blur_color_buffer[current_texture]);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	render_fullscreen_quad();

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

auto render_shadow_pass(const gse::shader& shadow_shader, const std::vector<gse::render_component>& render_components, const gse::mat4& light_projection, const gse::mat4& light_view, const GLuint depth_map_fbo, gse::id* light_ignore_list_id) -> void {
	shadow_shader.set_mat4("light_view", light_view);
	shadow_shader.set_mat4("light_projection", light_projection);

	glViewport(0, 0, static_cast<GLsizei>(g_shadow_width), static_cast<GLsizei>(g_shadow_height));
	glBindFramebuffer(GL_FRAMEBUFFER, depth_map_fbo);
	glClear(GL_DEPTH_BUFFER_BIT);

	for (auto& render_component : render_components) {
		if (gse::registry::is_entity_id_in_list(light_ignore_list_id, render_component.parent_id)) {
			continue;
		}

		for (auto& model_handle : render_component.models) {
			for (auto& [material_key, vao, draw_mode, vertex_count, model_matrix, color, textures, material] : model_handle.get_render_queue_entries()) {
				gse::mat4 model_matrix_v = model_matrix;

				if (const auto* motion_component = gse::registry::get_component_ptr<gse::physics::motion_component>(render_component.parent_id); motion_component) {
					if (const auto& transformed_model_matrix = motion_component->get_transformation_matrix(); transformed_model_matrix != model_matrix) {
						model_matrix_v = transformed_model_matrix;
					}
				}

				shadow_shader.set_mat4("model", model_matrix_v);
				glBindVertexArray(vao);
				glDrawElements(draw_mode, vertex_count, GL_UNSIGNED_INT, nullptr);
				glBindVertexArray(0);
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, gse::window::get_frame_buffer_size().x, gse::window::get_frame_buffer_size().y); // Restore viewport
}

auto ensure_non_collinear_up(const gse::unitless::vec3& direction, const gse::unitless::vec3& up) -> gse::unitless::vec3 {
	const gse::unitless::vec3 normalized_direction = gse::normalize(direction);
	gse::unitless::vec3 normalized_up = gse::normalize(up);

	if (const float dot_product = gse::dot(normalized_direction, normalized_up); std::abs(dot_product) > 1.0f - std::numeric_limits<float>::epsilon()) {
		if (std::abs(normalized_direction.y) > 0.9f) {
			normalized_up = gse::unitless::vec3(0.0f, 0.0f, 1.0f); // Z-axis
		}
		else {
			normalized_up = gse::unitless::vec3(0.0f, 1.0f, 0.0f); // Y-axis
		}
	}

	return normalized_up;
}

auto calculate_light_projection(const gse::light* light) -> gse::mat4 {
	const auto& entry = light->get_render_queue_entry();

	gse::mat4 light_projection(1.0f);

	if (light->get_type() == gse::light_type::directional) {
		light_projection = orthographic(
			gse::meters(-10000.0f),
			gse::meters(10000.0f),
			gse::meters(-10000.0f),
			gse::meters(1000.0f),
			entry.near_plane,
			entry.far_plane);
	}
	else if (light->get_type() == gse::light_type::spot) {
		const auto cutoff = gse::radians(entry.shader_entry.cut_off);
		light_projection = perspective(cutoff, 1.0f, entry.near_plane, entry.far_plane);
	}

	return light_projection;
}

auto calculate_light_view(const gse::light* light) -> gse::mat4 {
	const auto& entry = light->get_render_queue_entry();
	const gse::unitless::vec3 light_direction = entry.shader_entry.direction;

	gse::vec3<gse::length> light_pos(0.0f);

	if (light->get_type() == gse::light_type::directional) {
		light_pos = -light_direction * 10.0f;
	}
	else if (light->get_type() == gse::light_type::spot) {
		light_pos = entry.shader_entry.position;
	}

	return look_at(
		light_pos,
		light_pos + light_direction,
		ensure_non_collinear_up(light_direction, { 0.0f, 1.0f, 0.0f })
	);
}

auto gse::renderer3d::render() -> void {
	debug::add_imgui_callback([] {
		ImGui::Begin("Renderer3D");

		ImGui::Checkbox("HDR", &g_hdr);
		ImGui::Checkbox("Bloom", &g_bloom);
		ImGui::Checkbox("Depth Map Debug", &g_depth_map_debug);
		ImGui::Checkbox("Brightness Extraction Debug", &g_brightness_extraction_debug);
		ImGui::SliderInt("Blur Amount", &g_amount_of_blur_passes_in_each_direction, 0, 10);


		debug::unit_slider("Exposure", g_hdr_exposure,(0.1f),(10.f));
		debug::unit_slider("Bloom Intensity", g_bloom_intensity,(0.1f),(10.f));
		debug::unit_slider("Bloom Threshold", g_bloom_threshold,(0.1f),(10.f));
		debug::unit_slider("Bloom Radius", g_blur_radius,(0.1f),(10.f));
		ImGui::End();
		});

	auto& render_components = registry::get_components<render_component>();
	const auto& light_source_components = registry::get_components<light_source_component>();

	g_camera.update_camera_vectors();

	if (!window::is_mouse_visible()) g_camera.process_mouse_movement(window::get_mouse_delta());

	std::vector<light_shader_entry> light_data;
	light_data.reserve(light_source_components.size());

	std::vector<GLuint> depth_maps;
	depth_maps.reserve(light_source_components.size());

	for (const auto& light_source_component : light_source_components) {
		for (const auto& entry : light_source_component.get_render_queue_entries()) {
			light_data.push_back(entry.shader_entry);
			depth_maps.push_back(entry.depth_map);
		}
	}

	const auto& forward_rendering_shader = g_forward_rendering_shaders["ForwardRendering"];
	forward_rendering_shader.use();

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_ssbo_lights);
	glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(light_data.size()) * sizeof(light_shader_entry), light_data.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_ssbo_lights);

	g_reflection_cube_map.update(unitless::vec3(0.f), g_camera.get_projection_matrix(),
		[&render_components, &forward_rendering_shader](const mat4& view_matrix, const mat4& projection_matrix) {
			for (const auto& render_component : render_components) {
				for (const auto& model_handle : render_component.models) {
					for (const auto& render_queue_entry : model_handle.get_render_queue_entries()) {
						render_object_forward(render_component.parent_id, forward_rendering_shader, render_queue_entry, view_matrix, projection_matrix);
					}
				}
			}
		});

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	std::vector<mat4> light_space_matrices;

	const auto& shadow_shader = g_deferred_rendering_shaders["ShadowPass"];
	shadow_shader.use();

	for (auto& light_source_component : light_source_components) {
		for (const auto& light : light_source_component.get_lights()) {
			if (const auto point_light_ptr = dynamic_cast<point_light*>(light); point_light_ptr) {
				const auto light_pos = point_light_ptr->get_render_queue_entry().shader_entry.position;

				point_light_ptr->get_shadow_map().update(light_pos, mat4(1.f), [&](const mat4& view_matrix, const mat4& projection_matrix) {
					shadow_shader.use();
					shadow_shader.set_mat4("view", view_matrix);
					shadow_shader.set_mat4("projection", projection_matrix);
					shadow_shader.set_vec3("lightPos", light_pos);
					shadow_shader.set_float("farPlane", point_light_ptr->get_render_queue_entry().far_plane.as<units::meters>());

					for (const auto& render_component : render_components) {
						if (registry::is_entity_id_in_list(point_light_ptr->get_ignore_list_id(), render_component.parent_id)) {
							continue;
						}

						for (const auto& model_handle : render_component.models) {
							for (const auto& [material_key, vao, draw_mode, vertex_count, model_matrix, color, textures, material] : model_handle.get_render_queue_entries()) {
								shadow_shader.set_mat4("model", model_matrix);  // Object's model matrix
								glBindVertexArray(vao);
								glDrawElements(draw_mode, vertex_count, GL_UNSIGNED_INT, nullptr);
							}
						}

					}
					});
			}
			else {
				auto light_projection = calculate_light_projection(light);
				auto light_view = calculate_light_view(light);
				light_space_matrices.push_back(light_projection * light_view);
				render_shadow_pass(shadow_shader, render_components, light_projection, light_view, light->get_render_queue_entry().depth_map_fbo, light->get_ignore_list_id());
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Geometry pass
	glBindFramebuffer(GL_FRAMEBUFFER, g_g_buffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (const auto& render_component : render_components) {
		for (const auto& model_handle : render_component.models) {
			for (const auto& render_queue_entry : model_handle.get_render_queue_entries()) {
				render_object(render_component.parent_id, render_queue_entry, g_camera.get_view_matrix(), g_camera.get_projection_matrix());
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Light glow pass
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_DEPTH_TEST);

	for (const auto& light_source_component : light_source_components) {
		for (const auto& entry : light_source_component.get_render_queue_entries()) {
			render_object(entry);
		}
	}

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	render_lighting_pass(g_deferred_rendering_shaders["LightingPass"], light_data, light_space_matrices, depth_maps);

	const int current_texture = render_blur_pass(g_deferred_rendering_shaders["GaussianBlur"]);

	if (window::get_fbo().has_value()) {
		glBindFramebuffer(GL_FRAMEBUFFER, window::get_fbo().value());
	}
	else {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	render_additional_post_processing(g_deferred_rendering_shaders["PostProcessing"], current_texture);

	// Finally, render bounding boxes
	for (const auto& render_component : render_components) {

		for (const auto& model_handle : render_component.models) {
			for (const auto& [material_key, vao, draw_mode, vertex_count, model_matrix, color, textures, material] : model_handle.get_render_queue_entries()) {
				if (const auto it = g_materials.find(material_key); it != g_materials.end()) {
					it->second.use(g_camera.get_view_matrix(), g_camera.get_projection_matrix(), model_matrix);
					it->second.shader.set_vec3("color", color);

					glBindVertexArray(vao);
					glDrawArrays(draw_mode, 0, vertex_count);
					glBindVertexArray(0);
				}
				else {
					std::cerr << "Shader program key not found: " << material_key << '\n';
				}
			}
		}


	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

auto gse::renderer3d::get_camera() -> camera& {
	return g_camera;
}
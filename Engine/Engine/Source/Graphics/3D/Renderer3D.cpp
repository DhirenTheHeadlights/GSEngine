#include "Graphics/3D/Renderer3D.h"

#include <glm/gtx/string_cast.hpp>

#include "Core/ObjectRegistry.h"
#include "Core/JsonParser.h"
#include "Core/ResourcePaths.h"
#include "Graphics/Shader.h"
#include "Graphics/3D/CubeMap.h"
#include "Graphics/3D/Material.h"
#include "Graphics/3D/Lights/PointLight.h"
#include "Platform/GLFW/ErrorReporting.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

namespace {
	gse::camera g_camera;

	std::unordered_map<std::string, gse::material> g_materials;
	std::unordered_map<std::string, gse::shader> g_deferred_rendering_shaders;
	std::unordered_map<std::string, gse::shader> g_forward_rendering_shaders;
	std::unordered_map<std::string, gse::shader> g_lighting_shaders;

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

	gse::unitless g_hdr_exposure = 1.f;
	gse::unitless g_bloom_intensity = 1.f;
	gse::unitless g_bloom_threshold = 0.5f;

	gse::cube_map g_reflection_cube_map;

	float g_shadow_width = 4096;
	float g_shadow_height = 4096;

	bool g_depth_map_debug = false;
	bool g_brightness_extraction_debug = false;
	bool g_hdr = true;
	bool g_bloom = true;

	void load_shaders(const std::string& shader_path, const std::string& shader_file_name, std::unordered_map<std::string, gse::shader>& shaders) {
		gse::json_parse::parse(
			gse::json_parse::load_json(shader_path + shader_file_name),
			[&](const std::string& key, const nlohmann::json& value) {
				shaders.emplace(key, gse::shader(shader_path + value["vertex"].get<std::string>(),
					shader_path + value["fragment"].get<std::string>()));
			}
		);
	}
}

void gse::renderer::group::add_render_component(const std::shared_ptr<render_component>& new_render_component) {
	m_render_components.push_back(new_render_component);
}

void gse::renderer::group::add_light_source_component(const std::shared_ptr<light_source_component>& new_light_source_component) {
	m_light_source_components.push_back(new_light_source_component);

	for (const auto& light : new_light_source_component->get_lights()) {
		if (const auto point_light_ptr = std::dynamic_pointer_cast<point_light>(light); point_light_ptr) {
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

void gse::renderer::group::remove_render_component(const std::shared_ptr<render_component>& render_component_to_remove) {
	std::erase_if(m_render_components, [&](const std::weak_ptr<render_component>& component) {
		return !component.owner_before(render_component_to_remove) && !render_component_to_remove.owner_before(component);
		});
}

void gse::renderer::group::remove_light_source_component(const std::shared_ptr<light_source_component>& light_source_component_to_remove) {
	const auto it = std::ranges::find_if(m_light_source_components,
		[&](const std::weak_ptr<light_source_component>& component) {
			return !component.owner_before(light_source_component_to_remove) && !light_source_component_to_remove.owner_before(component);
		});

	for (const auto& light : light_source_component_to_remove->get_lights()) {
		if (const auto point_light_ptr = std::dynamic_pointer_cast<point_light>(light); point_light_ptr) {
			
		}
		else {
			const auto depth_map = light->get_render_queue_entry().depth_map;
			const auto depth_map_fbo = light->get_render_queue_entry().depth_map_fbo;

			glDeleteTextures(1, &depth_map);
			glDeleteFramebuffers(1, &depth_map_fbo);
		}
	}
}

void gse::renderer::initialize3d() {
	enable_report_gl_errors();

	const std::string shader_path = std::string(ENGINE_RESOURCES_PATH) + "Shaders/";

	const std::string object_shaders_path = shader_path + "Object/";
	json_parse::parse(
		json_parse::load_json(object_shaders_path + "object_shaders.json"),
		[&](const std::string& key, const nlohmann::json& value) {
			g_materials.emplace(key, material(object_shaders_path + value["vertex"].get<std::string>(),
			object_shaders_path + value["fragment"].get<std::string>(), key));
		}
	);

	load_shaders(shader_path + "DeferredRendering/", "deferred_rendering.json", g_deferred_rendering_shaders);
	load_shaders(shader_path + "ForwardRendering/", "forward_rendering.json", g_forward_rendering_shaders);
	load_shaders(shader_path + "Lighting/", "light_shaders.json", g_lighting_shaders);

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
	constexpr size_t buffer_size = sizeof(glm::mat4) * 10; // MAX_LIGHTS is 10 in the shader
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

		GLenum bloom_draw_buffer = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &bloom_draw_buffer);

		GLuint bloom_rbo_depth;
		glGenRenderbuffers(1, &bloom_rbo_depth);
		glBindRenderbuffer(GL_RENDERBUFFER, bloom_rbo_depth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screen_width, screen_height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, bloom_rbo_depth);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	const auto& bloom_shader = g_deferred_rendering_shaders["Bloom"];
	const auto& blur_shader = g_deferred_rendering_shaders["GaussianBlur"];

	bloom_shader.use();
	bloom_shader.set_int("scene", 0);
	bloom_shader.set_int("bloomBlur", 1);
	bloom_shader.set_bool("hdr", g_hdr);
	bloom_shader.set_float("exposure", g_hdr_exposure);
	bloom_shader.set_float("bloomIntensity", g_bloom_intensity);

	blur_shader.use();
	blur_shader.set_int("image", 0);
	blur_shader.set_bool("horizontal", true);

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
}

namespace {
	void render_object(const gse::render_queue_entry& entry, const glm::mat4& view_matrix, const glm::mat4& projection_matrix) {
		if (const auto it = g_materials.find(entry.material_key); it != g_materials.end()) {
			it->second.use(view_matrix, projection_matrix, entry.model_matrix);
			it->second.shader.set_vec3("color", entry.color);
			it->second.shader.set_bool("useTexture", entry.texture_id != 0);

			if (entry.texture_id != 0) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, entry.texture_id);
				it->second.shader.set_int("diffuseTexture", 0);
			}

			glBindVertexArray(entry.vao);
			glDrawElements(entry.draw_mode, entry.vertex_count, GL_UNSIGNED_INT, nullptr);
			glBindVertexArray(0);

			if (entry.texture_id != 0) {
				glBindTexture(GL_TEXTURE_2D, 0);
			}
		}
		else {
			std::cerr << "Shader program key not found: " << entry.material_key << '\n';
		}
	}

	void render_object_forward(const gse::shader& forward_rendering_shader, const gse::render_queue_entry& entry, const glm::mat4& view_matrix, const glm::mat4& projection_matrix) {
		forward_rendering_shader.set_vec3("color", entry.color);
		forward_rendering_shader.set_bool("useTexture", entry.texture_id != 0);
		forward_rendering_shader.set_mat4("model", entry.model_matrix);
		forward_rendering_shader.set_mat4("view", view_matrix);
		forward_rendering_shader.set_mat4("projection", projection_matrix);
		forward_rendering_shader.set_vec3("viewPos", g_camera.get_position().as<gse::units::meters>());

		if (entry.texture_id != 0) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, entry.texture_id);
			forward_rendering_shader.set_int("diffuseTexture", 0);
		}

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, g_g_albedo_spec);

		glBindVertexArray(entry.vao);
		glDrawElements(entry.draw_mode, entry.vertex_count, GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);

		if (entry.texture_id != 0) {
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	void render_object(const gse::light_render_queue_entry& entry) {
		if (const auto it = g_lighting_shaders.find(entry.shader_key); it != g_lighting_shaders.end()) {
			it->second.use();
			it->second.set_mat4("model", glm::mat4(1.0f));
			it->second.set_mat4("view", g_camera.get_view_matrix());
			it->second.set_mat4("projection", g_camera.get_projection_matrix());

			it->second.set_vec3("color", entry.shader_entry.color);
			it->second.set_float("intensity", entry.shader_entry.intensity);
		}
		else {
			std::cerr << "Shader program key not found: " << entry.shader_key << '\n';
		}
	}

	void render_fullscreen_quad() {
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

	void render_lighting_pass(const gse::shader& lighting_shader, const std::vector<gse::light_shader_entry>& light_data, const std::vector<glm::mat4>& light_space_matrices, const std::vector<GLuint>& depth_maps) {
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
		glBufferSubData(GL_UNIFORM_BUFFER, 0, light_space_matrices.size() * sizeof(glm::mat4), light_space_matrices.data());
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// Bind HDR frame buffer
		glBindFramebuffer(GL_FRAMEBUFFER, g_hdr_fbo);
		constexpr GLenum hdr_draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, hdr_draw_buffers);

		// Lighting pass
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

		// Pass other G-buffer textures
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

		glEnable(GL_DEPTH_TEST);  // Re-enable depth testing after the lighting pass

		// Unbind HDR frame buffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	int render_blur_pass(const gse::shader& blur_shader) {
		blur_shader.use();
		blur_shader.set_int("image", 0);

		bool horizontal = true;
		bool first_iteration = true;
		unsigned int current_fbo = 0;
		unsigned int current_texture = 0;

		for (int i = 0; i < 10; ++i) {
			glBindFramebuffer(GL_FRAMEBUFFER, g_blur_fbo[current_fbo]);
			blur_shader.set_bool("horizontal", horizontal);

			glActiveTexture(GL_TEXTURE0);
			if (first_iteration) {
				glBindTexture(GL_TEXTURE_2D, g_hdr_color_buffer[1]);
				first_iteration = false;
			}
			else {
				glBindTexture(GL_TEXTURE_2D, g_blur_color_buffer[current_texture]);
			}

			render_fullscreen_quad();

			horizontal = !horizontal;
			current_fbo = 1 - current_fbo;
			current_texture = 1 - current_texture;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		return static_cast<int>(current_texture);
	}

	void render_bloom_compositing(const gse::shader& bloom_shader, const int current_texture) {
		bloom_shader.use();
		bloom_shader.set_int("scene", 0);
		bloom_shader.set_int("bloomBlur", 1);
		bloom_shader.set_bool("hdr", g_hdr);
		bloom_shader.set_bool("bloom", g_bloom);
		bloom_shader.set_float("exposure", g_hdr_exposure);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_hdr_color_buffer[0]); // Full HDR scene

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, g_blur_color_buffer[current_texture]); // Blurred scene

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		render_fullscreen_quad();

		// Unbind textures after rendering
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void render_additional_post_processing(const gse::shader& post_processing_shader) {
		post_processing_shader.use();
		post_processing_shader.set_int("scene", 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_hdr_color_buffer[0]);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		render_fullscreen_quad();

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void render_shadow_pass(const gse::shader& shadow_shader, const std::vector<std::weak_ptr<gse::render_component>>& render_components, const glm::mat4& light_projection, const glm::mat4& light_view, const GLuint depth_map_fbo, const std::shared_ptr<gse::id>& light_ignore_list_id) {
		shadow_shader.set_mat4("light_view", light_view);
		shadow_shader.set_mat4("light_projection", light_projection);

		glViewport(0, 0, static_cast<GLsizei>(g_shadow_width), static_cast<GLsizei>(g_shadow_height)); 
		glBindFramebuffer(GL_FRAMEBUFFER, depth_map_fbo);
		glClear(GL_DEPTH_BUFFER_BIT);

		for (const auto& render_component : render_components) {
			if (const auto render_component_ptr = render_component.lock(); render_component_ptr) {
				if (gse::registry::is_id_in_list(light_ignore_list_id, render_component_ptr->get_id())) {
					continue;
				}

				for (const auto& entry : render_component_ptr->get_queue_entries()) {
					shadow_shader.set_mat4("model", entry.model_matrix);
					glBindVertexArray(entry.vao);
					glDrawElements(entry.draw_mode, entry.vertex_count, GL_UNSIGNED_INT, nullptr);
					glBindVertexArray(0);
				}
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, gse::window::get_frame_buffer_size().x, gse::window::get_frame_buffer_size().y); // Restore viewport
	}

	glm::vec3 ensure_non_collinear_up(const glm::vec3& direction, const glm::vec3& up) {
		constexpr float epsilon = 0.001f;

		const glm::vec3 normalized_direction = normalize(direction);
		glm::vec3 normalized_up = normalize(up);

		if (const float dot_product = dot(normalized_direction, normalized_up); glm::abs(dot_product) > 1.0f - epsilon) {
			if (glm::abs(normalized_direction.y) > 0.9f) {
				normalized_up = glm::vec3(0.0f, 0.0f, 1.0f); // Z-axis
			}
			else {
				normalized_up = glm::vec3(0.0f, 1.0f, 0.0f); // Y-axis
			}
		}

		return normalized_up;
	}

	glm::mat4 calculate_light_projection(const std::shared_ptr<gse::light>& light) {
		const auto& entry = light->get_render_queue_entry();

		glm::mat4 light_projection(1.0f);

		if (light->get_type() == gse::light_type::directional) {
			light_projection = glm::ortho(-10000.0f, 10000.0f, -10000.0f, 1000.0f,
				entry.near_plane.as<gse::units::meters>(), entry.far_plane.as<gse::units::meters>());
		}
		else if (light->get_type() == gse::light_type::spot) {
			const float cutoff = entry.shader_entry.cut_off;
			light_projection = glm::perspective(cutoff, 1.0f, entry.near_plane.as<gse::units::meters>(), entry.far_plane.as<gse::units::meters>());
		}

		return light_projection;
	}

	glm::mat4 calculate_light_view(const std::shared_ptr<gse::light>& light) {
		const auto& entry = light->get_render_queue_entry();
		const glm::vec3 light_direction = entry.shader_entry.direction;

		glm::vec3 light_pos(0.0f);

		if (light->get_type() == gse::light_type::directional) {
			light_pos = -light_direction * 10.0f;
		}
		else if (light->get_type() == gse::light_type::spot) {
			light_pos = entry.shader_entry.position;
		}

		return lookAt(
			light_pos,
			light_pos + light_direction,
			ensure_non_collinear_up(light_direction, glm::vec3(0.0f, 1.0f, 0.0f))
		);
	}
}

void gse::renderer::render_objects(group& group) {
	debug::add_imgui_callback([] {
		ImGui::Begin("Renderer3D");

		ImGui::Checkbox("HDR", &g_hdr);
		ImGui::Checkbox("Bloom", &g_bloom);
		ImGui::Checkbox("Depth Map Debug", &g_depth_map_debug);
		ImGui::Checkbox("Brightness Extraction Debug", &g_brightness_extraction_debug);

		debug::unit_slider("Exposure", g_hdr_exposure, unitless(0.1f), unitless(10.f));
		debug::unit_slider("Bloom Intensity", g_bloom_intensity, unitless(0.1f), unitless(10.f));
		debug::unit_slider("Bloom Threshold", g_bloom_threshold, unitless(0.1f), unitless(10.f));

		ImGui::End();
		});

	const auto& render_components = group.get_render_components();
	const auto& light_source_components = group.get_light_source_components();

	g_camera.update_camera_vectors();
	if (!window::is_mouse_visible()) g_camera.process_mouse_movement(input::get_mouse().delta);

	std::vector<light_shader_entry> light_data;
	light_data.reserve(light_source_components.size());

	std::vector<GLuint> depth_maps;
	depth_maps.reserve(light_source_components.size());

	for (const auto& light_source_component : light_source_components) {
		if (const auto light_source_component_ptr = light_source_component.lock(); light_source_component_ptr) {
			for (const auto& entry : light_source_component_ptr->get_render_queue_entries()) {
				light_data.push_back(entry.shader_entry);
				depth_maps.push_back(entry.depth_map);
			}
		}
		else {
			group.remove_light_source_component(light_source_component.lock());
		}
	}

	const auto& forward_rendering_shader = g_forward_rendering_shaders["ForwardRendering"];
	forward_rendering_shader.use();

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_ssbo_lights);
	glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(light_data.size()) * sizeof(light_shader_entry), light_data.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_ssbo_lights);

	g_reflection_cube_map.update(glm::vec3(0.f), g_camera.get_projection_matrix(),
		[&group, render_components, forward_rendering_shader](const glm::mat4& view_matrix, const glm::mat4& projection_matrix) {
			for (const auto& render_component : render_components) {
				if (const auto render_component_ptr = render_component.lock(); render_component_ptr) {
					for (auto entries = render_component_ptr->get_queue_entries(); const auto& entry : entries) {
						render_object_forward(forward_rendering_shader, entry, view_matrix, projection_matrix);
					}
				}
				else {
					group.remove_render_component(render_component.lock());
				}
			}
		});

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	std::vector<glm::mat4> light_space_matrices;

	const auto& shadow_shader = g_deferred_rendering_shaders["ShadowPass"];
	shadow_shader.use();

	for (const auto& light_source_component : light_source_components) {
		for (const auto& light : light_source_component.lock()->get_lights()) {
			if (const auto point_light_ptr = std::dynamic_pointer_cast<point_light>(light); point_light_ptr) {
				const auto light_pos = point_light_ptr->get_render_queue_entry().shader_entry.position;

				point_light_ptr->get_shadow_map().update(light_pos, glm::mat4(1.f), [&](const glm::mat4& view_matrix, const glm::mat4& projection_matrix) {
					shadow_shader.use();
					shadow_shader.set_mat4("view", view_matrix);     
					shadow_shader.set_mat4("projection", projection_matrix);
					shadow_shader.set_vec3("lightPos", light_pos);
					shadow_shader.set_float("farPlane", point_light_ptr->get_render_queue_entry().far_plane.as<units::meters>());

					for (const auto& render_component : render_components) {
						if (const auto render_component_ptr = render_component.lock(); render_component_ptr) {
							if (registry::is_id_in_list(point_light_ptr->get_ignore_list_id(), render_component_ptr->get_id())) {
								continue;
							}

							for (const auto& entry : render_component_ptr->get_queue_entries()) {
								shadow_shader.set_mat4("model", entry.model_matrix);  // Object's model matrix
								glBindVertexArray(entry.vao);
								glDrawElements(entry.draw_mode, entry.vertex_count, GL_UNSIGNED_INT, nullptr);
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
		if (const auto render_component_ptr = render_component.lock(); render_component_ptr) {
			for (auto entries = render_component_ptr->get_queue_entries(); const auto & entry : entries) {
				render_object(entry, g_camera.get_view_matrix(), g_camera.get_projection_matrix());
			}
		}
		else {
			group.remove_render_component(render_component.lock());
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Light glow pass
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_DEPTH_TEST);

	for (const auto& light_source_component : light_source_components) {
		if (const auto light_source_component_ptr = light_source_component.lock(); light_source_component_ptr) {
			for (const auto& entry : light_source_component_ptr->get_render_queue_entries()) {
				render_object(entry); 
			}
		}
		else {
			group.remove_light_source_component(light_source_component.lock());
		}
	}

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	if (window::get_fbo().has_value()) {
		glBindFramebuffer(GL_FRAMEBUFFER, window::get_fbo().value());
	}
	else {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	render_lighting_pass(g_deferred_rendering_shaders["LightingPass"], light_data, light_space_matrices, depth_maps);

	const int current_texture = render_blur_pass(g_deferred_rendering_shaders["GaussianBlur"]);

	render_bloom_compositing(g_deferred_rendering_shaders["Bloom"], current_texture);

	render_additional_post_processing(g_deferred_rendering_shaders["PostProcessing"]);
}

gse::camera& gse::renderer::get_camera() {
	return g_camera;
}
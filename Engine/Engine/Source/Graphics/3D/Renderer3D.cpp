#include "Graphics/3D/Renderer3D.h"

#include <glm/gtx/string_cast.hpp>

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
	gse::camera camera;

	std::unordered_map<std::string, gse::material> materials;
	std::unordered_map<std::string, gse::shader> deferredRenderingShaders;
	std::unordered_map<std::string, gse::shader> forwardRenderingShaders;
	std::unordered_map<std::string, gse::shader> lightingShaders;

	GLuint gBuffer = 0;
	GLuint gPosition = 0;
	GLuint gNormal = 0;
	GLuint gAlbedoSpec = 0;
	GLuint ssboLights = 0;
	GLuint lightSpaceBlockUBO = 0;

	gse::cube_map reflectionCubeMap;

	float shadowWidth = 4096;
	float shadowHeight = 4096;

	gse::length nearPlane = gse::meters(10.0f);
	gse::length farPlane = gse::meters(1000.f);

	bool depthMapDebug = false;

	void loadShaders(const std::string& shaderPath, const std::string& shaderFileName, std::unordered_map<std::string, gse::shader>& shaders) {
		gse::json_parse::parse(
			gse::json_parse::load_json(shaderPath + shaderFileName),
			[&](const std::string& key, const nlohmann::json& value) {
				shaders.emplace(key, gse::shader(shaderPath + value["vertex"].get<std::string>(),
					shaderPath + value["fragment"].get<std::string>()));
			}
		);
	}
}

void gse::renderer::group::add_render_component(const std::shared_ptr<render_component>& render_component) {
	m_render_components.push_back(render_component);
}

void gse::renderer::group::add_light_source_component(const std::shared_ptr<light_source_component>& light_source_component) {
	m_light_source_components.push_back(light_source_component);

	if (const auto pointLight = std::dynamic_pointer_cast<point_light>(light_source_component); pointLight) {
		pointLight->get_shadow_map().create(static_cast<int>((shadowWidth + shadowHeight) / 2.f), true);

		depth_maps.push_back(pointLight->get_shadow_map().get_texture_id());
		depth_map_fbos.push_back(pointLight->get_shadow_map().get_frame_buffer_id());
	}
	else {
		GLuint depthMap, depthMapFbo;
		glGenTextures(1, &depthMap);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, static_cast<GLsizei>(shadowWidth), static_cast<GLsizei>(shadowHeight), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		constexpr float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

		glGenFramebuffers(1, &depthMapFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		depth_maps.push_back(depthMap);
		depth_map_fbos.push_back(depthMapFbo);
	}
}

void gse::renderer::group::remove_render_component(const std::shared_ptr<render_component>& render_component) {
	std::erase_if(m_render_components, [&](const std::weak_ptr<render_component>& component) {
		return !component.owner_before(render_component) && !render_component.owner_before(component);
		});
}

void gse::renderer::group::remove_light_source_component(const std::shared_ptr<light_source_component>& light_source_component) {
	const auto it = std::ranges::find_if(m_light_source_components,
		[&](const std::weak_ptr<light_source_component>& component) {
			return !component.owner_before(light_source_component) && !light_source_component.owner_before(component);
		});

	if (it != m_light_source_components.end()) {
		const size_t index = std::distance(m_light_source_components.begin(), it);

		glDeleteTextures(1, &depth_maps[index]);
		glDeleteFramebuffers(1, &depth_map_fbos[index]);

		depth_maps.erase(depth_maps.begin() + index);
		depth_map_fbos.erase(depth_map_fbos.begin() + index);
		m_light_source_components.erase(it);
	}
}

void gse::renderer::initialize3d() {
	enable_report_gl_errors();

	const std::string shaderPath = std::string(ENGINE_RESOURCES_PATH) + "Shaders/";

	const std::string objectShadersPath = shaderPath + "Object/";
	json_parse::parse(
		json_parse::load_json(objectShadersPath + "object_shaders.json"),
		[&](const std::string& key, const nlohmann::json& value) {
			materials.emplace(key, material(objectShadersPath + value["vertex"].get<std::string>(),
			objectShadersPath + value["fragment"].get<std::string>(), key));
		}
	);

	loadShaders(shaderPath + "DeferredRendering/", "deferred_rendering.json", deferredRenderingShaders);
	loadShaders(shaderPath + "ForwardRendering/", "forward_rendering.json", forwardRenderingShaders);
	loadShaders(shaderPath + "Lighting/", "light_shaders.json", lightingShaders);

	const GLsizei screenWidth = window::get_frame_buffer_size().x;
	const GLsizei screenHeight = window::get_frame_buffer_size().y;

	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

	// Set up G-buffer textures
	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

	glGenTextures(1, &gAlbedoSpec);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

	constexpr GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);

	GLuint rboDepth;
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth, screenHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Set up the UBO for light space matrices
	glGenBuffers(1, &lightSpaceBlockUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, lightSpaceBlockUBO);
	constexpr size_t bufferSize = sizeof(glm::mat4) * 10; // MAX_LIGHTS is 10 in the shader
	glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(bufferSize), nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 4, lightSpaceBlockUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// Initialize SSBO for lights
	glGenBuffers(1, &ssboLights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, ssboLights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	const auto& lightingShader = deferredRenderingShaders["LightingPass"];

	lightingShader.use();
	lightingShader.set_int("gPosition", 0);
	lightingShader.set_int("gNormal", 1);
	lightingShader.set_int("gAlbedoSpec", 2);
	lightingShader.set_bool("depthMapDebug", depthMapDebug);

	reflectionCubeMap.create(1024);
}

namespace {
	void renderObject(const gse::render_queue_entry& entry, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
		if (const auto it = materials.find(entry.material_key); it != materials.end()) {
			it->second.use(viewMatrix, projectionMatrix, entry.model_matrix);
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

	void renderObjectForward(const gse::shader& forwardRenderingShader, const gse::render_queue_entry& entry, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
		forwardRenderingShader.set_vec3("color", entry.color);
		forwardRenderingShader.set_bool("useTexture", entry.texture_id != 0);
		forwardRenderingShader.set_mat4("model", entry.model_matrix);
		forwardRenderingShader.set_mat4("view", viewMatrix);
		forwardRenderingShader.set_mat4("projection", projectionMatrix);
		forwardRenderingShader.set_vec3("viewPos", glm::vec3(0.f));

		if (entry.texture_id != 0) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, entry.texture_id);
			forwardRenderingShader.set_int("diffuseTexture", 0);
		}

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

		glBindVertexArray(entry.vao);
		glDrawElements(entry.draw_mode, entry.vertex_count, GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);

		if (entry.texture_id != 0) {
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	void renderObject(const gse::light_render_queue_entry& entry) {
		if (const auto it = lightingShaders.find(entry.shader_key); it != lightingShaders.end()) {
			it->second.use();
			it->second.set_mat4("model", glm::mat4(1.0f));
			it->second.set_mat4("view", camera.get_view_matrix());
			it->second.set_mat4("projection", camera.get_projection_matrix());

			it->second.set_vec3("color", entry.shader_entry.color);
			it->second.set_float("intensity", entry.shader_entry.intensity);
		}
		else {
			std::cerr << "Shader program key not found: " << entry.shader_key << '\n';
		}
	}

	void renderLightingPass(const gse::shader& lightingShader, const std::vector<gse::light_shader_entry>& lightData, const std::vector<glm::mat4>& lightSpaceMatrices, const std::vector<GLuint>& depthMapFBOs) {
		if (lightData.empty()) {
			return;
		}

		lightingShader.use();

		// Update SSBO with light data
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
		glBufferData(GL_SHADER_STORAGE_BUFFER, lightData.size() * sizeof(gse::light_shader_entry), lightData.data(), GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, ssboLights);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Update UBO with light space matrices
		glBindBuffer(GL_UNIFORM_BUFFER, lightSpaceBlockUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, lightSpaceMatrices.size() * sizeof(glm::mat4), lightSpaceMatrices.data());
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// Lighting pass
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);

		std::vector<GLint> shadowMapUnits(depthMapFBOs.size());
		for (size_t i = 0; i < depthMapFBOs.size(); ++i) {
			glActiveTexture(GL_TEXTURE3 + i);
			glBindTexture(GL_TEXTURE_2D, depthMapFBOs[i]);
			shadowMapUnits[i] = 3 + i;
		}

		lightingShader.set_int_array("shadowMaps", shadowMapUnits.data(), static_cast<unsigned>(shadowMapUnits.size()));
		lightingShader.set_mat4_array("lightSpaceMatrices", lightSpaceMatrices.data(), static_cast<unsigned>(lightSpaceMatrices.size()));

		// Pass other G-buffer textures
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gPosition);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gNormal);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

		constexpr GLuint bindingUnit = 5;
		reflectionCubeMap.bind(bindingUnit);
		lightingShader.set_int("environmentMap", bindingUnit);

		// Render a full-screen quad to process lighting
		static unsigned int quadVAO = 0;
		static unsigned int quadVBO = 0;

		if (quadVAO == 0) {
			constexpr float quadVertices[] = {
				// Positions   // TexCoords
				-1.0f,  1.0f,  0.0f, 1.0f,
				-1.0f, -1.0f,  0.0f, 0.0f,
				 1.0f, -1.0f,  1.0f, 0.0f,

				-1.0f,  1.0f,  0.0f, 1.0f,
				 1.0f, -1.0f,  1.0f, 0.0f,
				 1.0f,  1.0f,  1.0f, 1.0f
			};

			glGenVertexArrays(1, &quadVAO);
			glGenBuffers(1, &quadVBO);
			glBindVertexArray(quadVAO);
			glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), static_cast<void*>(nullptr));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
		}

		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		glEnable(GL_DEPTH_TEST);  // Re-enable depth testing after the lighting pass
	}

	void renderShadowPass(const gse::shader& shadowShader, const std::vector<std::weak_ptr<gse::render_component>>& renderComponents, const glm::mat4& lightSpaceMatrix, const GLuint depthMapFBO) {
		shadowShader.set_mat4("lightSpaceMatrix", lightSpaceMatrix);

		glViewport(0, 0, static_cast<GLsizei>(shadowWidth), static_cast<GLsizei>(shadowHeight)); 
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);


		for (const auto& renderComponent : renderComponents) {
			if (const auto renderComponentPtr = renderComponent.lock()) {
				for (const auto& entry : renderComponentPtr->get_queue_entries()) {
					shadowShader.set_mat4("model", entry.model_matrix);
					glBindVertexArray(entry.vao);
					glDrawElements(entry.draw_mode, entry.vertex_count, GL_UNSIGNED_INT, nullptr);
					glBindVertexArray(0);
				}
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, gse::window::get_frame_buffer_size().x, gse::window::get_frame_buffer_size().y); // Restore viewport
	}

	glm::vec3 ensureNonCollinearUp(const glm::vec3& direction, const glm::vec3& up) {
		constexpr float epsilon = 0.001f;

		const glm::vec3 normalizedDirection = normalize(direction);
		glm::vec3 normalizedUp = normalize(up);
		const float dotProduct = dot(normalizedDirection, normalizedUp);

		if (glm::abs(dotProduct) > 1.0f - epsilon) {
			if (glm::abs(normalizedDirection.y) > 0.9f) {
				normalizedUp = glm::vec3(0.0f, 0.0f, 1.0f); // Z-axis
			}
			else {
				normalizedUp = glm::vec3(0.0f, 1.0f, 0.0f); // Y-axis
			}
		}

		return normalizedUp;
	}


	glm::mat4 calculateLightSpaceMatrix(const std::shared_ptr<gse::light>& light) {
		const auto& entry = light->get_render_queue_entry().shader_entry;
		const glm::vec3 lightDirection = entry.direction;

		glm::vec3 lightPos(0.0f);
		glm::mat4 lightProjection(1.0f);

		if (entry.light_type == static_cast<int>(gse::light_type::directional)) {
			lightPos = -lightDirection * 10.0f;
			lightProjection = glm::ortho(-10000.0f, 10000.0f, -10000.0f, 1000.0f,
				nearPlane.as<gse::Meters>(), farPlane.as<gse::Meters>());
		}
		else if (entry.light_type == static_cast<int>(gse::light_type::spot)) {
			lightPos = entry.position;
			const float cutoff = entry.cut_off;
			lightProjection = glm::perspective(cutoff, 1.0f, nearPlane.as<gse::Meters>(), farPlane.as<gse::Meters>());
		}

		const glm::mat4 lightView = lookAt(
			lightPos,
			lightPos + lightDirection,
			ensureNonCollinearUp(lightDirection, glm::vec3(0.0f, 1.0f, 0.0f))
		);

		return lightProjection * lightView;
	}
}

void gse::renderer::render_objects(group& group) {
	const auto& renderComponents = group.get_render_components();
	const auto& lightSourceComponents = group.get_light_source_components();

	camera.updateCameraVectors();
	if (!window::is_mouse_visible()) camera.processMouseMovement(input::get_mouse().delta);

	debug::add_imgui_callback([] {
		ImGui::Begin("Near/Far Plane");
		debug::unit_slider<length, Meters>("Near Plane", nearPlane, meters(0.1f), meters(100.0f));
		debug::unit_slider<length, Meters>("Far Plane", farPlane, meters(10.0f), meters(10000.0f));
		ImGui::End();
		});

	// Grab all LightRenderQueueEntries from LightSourceComponents
	std::vector<light_shader_entry> lightData;
	lightData.reserve(lightSourceComponents.size());
	for (const auto& lightSourceComponent : lightSourceComponents) {
		if (const auto lightSourceComponentPtr = lightSourceComponent.lock()) {
			for (const auto& entry : lightSourceComponentPtr->get_render_queue_entries()) {
				lightData.push_back(entry.shader_entry);
			}
		}
		else {
			group.remove_light_source_component(lightSourceComponent.lock());
		}
	}

	const auto& forwardRenderingShader = forwardRenderingShaders["ForwardRendering"];
	forwardRenderingShader.use();

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
	glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(lightData.size()) * sizeof(light_shader_entry), lightData.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboLights);

	reflectionCubeMap.update(glm::vec3(0.f), glm::perspective(glm::radians(45.0f), 1.0f, nearPlane.as<Meters>(), farPlane.as<Meters>()),
		[&group, renderComponents, forwardRenderingShader](const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
			for (const auto& renderComponent : renderComponents) {
				if (const auto renderComponentPtr = renderComponent.lock()) {
					for (auto entries = renderComponentPtr->get_queue_entries(); const auto & entry : entries) {
						renderObjectForward(forwardRenderingShader, entry, viewMatrix, projectionMatrix);
					}
				}
				else {
					group.remove_render_component(renderComponent.lock());
				}
			}
		});

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	std::vector<glm::mat4> lightSpaceMatrices;

	const auto& shadowShader = deferredRenderingShaders["ShadowPass"];
	shadowShader.use();

	for (size_t i = 0; i < lightSourceComponents.size(); ++i) {
		for (const auto& light : lightSourceComponents[i].lock()->get_lights()) {
			if (const auto pointLight = std::dynamic_pointer_cast<point_light>(light); pointLight) {
				const auto lightPos = pointLight->get_render_queue_entry().shader_entry.position;

				pointLight->get_shadow_map().update(lightPos, glm::mat4(1.f), [&](const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
					shadowShader.use();
					shadowShader.set_mat4("view", viewMatrix);     
					shadowShader.set_mat4("projection", projectionMatrix);
					shadowShader.set_vec3("lightPos", lightPos);
					shadowShader.set_float("farPlane", farPlane.as<Meters>());

					for (const auto& renderComponent : renderComponents) {
						if (const auto renderComponentPtr = renderComponent.lock()) {
							for (const auto& entry : renderComponentPtr->get_queue_entries()) {
								shadowShader.set_mat4("model", entry.model_matrix);  // Object's model matrix
								glBindVertexArray(entry.vao);
								glDrawElements(entry.draw_mode, entry.vertex_count, GL_UNSIGNED_INT, nullptr);
							}
						}
					}
					});
			}
			else {
				lightSpaceMatrices.push_back(calculateLightSpaceMatrix(light));
				renderShadowPass(shadowShader, renderComponents, lightSpaceMatrices.back(), group.depth_map_fbos[i]);
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Geometry pass
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (const auto& renderComponent : renderComponents) {
		if (const auto renderComponentPtr = renderComponent.lock()) {
			for (auto entries = renderComponentPtr->get_queue_entries(); const auto & entry : entries) {
				renderObject(entry, camera.getViewMatrix(), camera.getProjectionMatrix());
			}
		}
		else {
			group.remove_render_component(renderComponent.lock());
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Light glow pass
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_DEPTH_TEST);

	for (const auto& lightSourceComponent : lightSourceComponents) {
		if (const auto lightSourceComponentPtr = lightSourceComponent.lock()) {
			for (const auto& entry : lightSourceComponentPtr->get_render_queue_entries()) {
				renderObject(entry); 
			}
		}
		else {
			group.remove_light_source_component(lightSourceComponent.lock());
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

	renderLightingPass(deferredRenderingShaders["LightingPass"], lightData, lightSpaceMatrices, group.depth_maps);
}

gse::camera& gse::renderer::get_camera() {
	return camera;
}
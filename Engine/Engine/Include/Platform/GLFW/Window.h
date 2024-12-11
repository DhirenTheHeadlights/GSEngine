#pragma once

#include <memory>
#include <optional>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "glm/glm.hpp"

namespace gse::window {
    struct rendering_interface {
        virtual ~rendering_interface() = default;
        virtual void on_pre_render() = 0;
        virtual void on_post_render() = 0;
    };

    void add_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface);
    void remove_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface);

    void initialize();
    void begin_frame();
    void update();
    void end_frame();
    void shutdown();

    GLFWwindow* get_window();

    bool is_window_closed();
    bool is_full_screen();
    bool is_focused();
	bool is_minimized();
    bool is_mouse_visible();
    int has_mouse_moved();

    GLFWmonitor* get_current_monitor();

    std::optional<GLuint> get_fbo();
    glm::ivec2 get_frame_buffer_size();
    glm::ivec2 get_rel_mouse_position();
    glm::ivec2 get_window_size();
    glm::ivec2 get_viewport_size();

    void set_fbo(GLuint fbo_in, const glm::ivec2& size);
    void set_mouse_pos_relative_to_window(const glm::ivec2& position);
    void set_full_screen(bool fs);
    void set_window_focused(bool focused);
    void set_mouse_visible(bool show);
}

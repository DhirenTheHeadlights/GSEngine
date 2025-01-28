module;

#include <glad/glad.h>
#include <GLFW/glfw3.h>

export module gse.platform.glfw.window;

import std;
import glm;

export namespace gse::window {
    struct rendering_interface {
        virtual ~rendering_interface() = default;
        virtual auto on_pre_render() -> void;
        virtual auto on_post_render() -> void = 0;
    };

    auto add_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) -> void;
    auto remove_rendering_interface(const std::shared_ptr<rendering_interface>& rendering_interface) -> void;

    auto initialize() -> void;
    auto begin_frame() -> void;
    auto update() -> void;
    auto end_frame() -> void;
    auto shutdown() -> void;

    auto get_window() -> GLFWwindow*;

    auto is_window_closed() -> bool;
    auto is_full_screen() -> bool;
    auto is_focused() -> bool;
	auto is_minimized() -> bool;
    auto is_mouse_visible() -> bool;
    auto has_mouse_moved() -> int;

    auto get_current_monitor() -> GLFWmonitor*;

    auto get_fbo() -> std::optional<GLuint>;
    auto get_frame_buffer_size() -> glm::ivec2;
    auto get_rel_mouse_position() -> glm::ivec2;
    auto get_window_size() -> glm::ivec2;
    auto get_viewport_size() -> glm::ivec2;

    auto set_fbo(GLuint fbo_in, const glm::ivec2& size) -> void;
    auto set_mouse_pos_relative_to_window(const glm::ivec2& position) -> void;
    auto set_full_screen(bool fs) -> void;
    auto set_window_focused(bool focused) -> void;
    auto set_mouse_visible(bool show) -> void;
}

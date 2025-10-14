export module gs:client_ui;

import std;
import gse;

export namespace gs {
	class client_ui final : public gse::hook<gse::engine> {
	public:
		using hook::hook;

		auto update() -> void override {
			if (gse::keyboard::pressed(gse::key::escape)) {
				gse::shutdown();
			}

			if (gse::keyboard::pressed(gse::key::n) || gse::mouse::pressed(gse::mouse_button::button_3)) {
				m_show_cross_hair = !m_show_cross_hair;
			}
		}

		auto render() -> void override {
			gse::gui::start(
				"Test",
				[&] {
					gse::gui::value("FPS", gse::system_clock::fps());
					gse::gui::value("Test Value", 42);
					gse::gui::value("Test Quantity", gse::meters(5.0f));
					gse::gui::vec("Test Vec", gse::vec3<gse::length>(1.f, 2.f, 3.f));
					gse::gui::vec("Mouse Position", gse::mouse::position());
					gse::gui::input("Input Test", m_buff);
					gse::gui::slider("Slider Test", m_slider_value, gse::meters(0.0f), gse::meters(10.0f));
					gse::gui::slider("Vec Slider Test", m_vec_value, gse::vec3<gse::length>(0.f), gse::vec3<gse::length>(10.f));
				}
			);

			gse::gui::start(
				"Profiler",
				[] {
					gse::gui::profiler();
				}
			);

			gse::renderer::set_ui_focus(m_show_cross_hair);
		}

	private:
		bool m_show_cross_hair = false;
		std::string m_buff;
		gse::length m_slider_value = gse::meters(0.0f);
		gse::vec3<gse::length> m_vec_value = { gse::meters(1.f), gse::meters(2.f), gse::meters(3.f) };
	};
}
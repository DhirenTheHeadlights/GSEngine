export module gs;

import std;
import gse;

import :main_test_scene;
import :skybox_scene;
import :second_test_scene;

export namespace gs {
	class game final : public gse::hook<gse::engine> {
	public:
		explicit game(gse::engine* owner) : hook(owner) {}

		auto initialize() -> void  override {
			auto scene1 = std::make_unique<gse::scene>("Scene1");
			scene1->add_hook(std::make_unique<main_test_scene>(scene1.get()));

			auto scene2 = std::make_unique<gse::scene>("Scene2");
			scene2->add_hook(std::make_unique<skybox_scene>(scene2.get()));

			auto scene3 = std::make_unique<gse::scene>("Scene3");
			scene3->add_hook(std::make_unique<second_test_scene>(scene3.get()));

			gse::scene_loader::add(scene1);
			gse::scene_loader::add(scene2);
			gse::scene_loader::add(scene3);

			gse::scene_loader::queue(gse::find("Scene1"), [] { return gse::keyboard::pressed(gse::key::f1); });
			gse::scene_loader::queue(gse::find("Scene2"), [] { return gse::keyboard::pressed(gse::key::f2); });
			gse::scene_loader::queue(gse::find("Scene3"), [] { return gse::keyboard::pressed(gse::key::f3); });
		}

		auto update() -> void  override {
			if (gse::keyboard::pressed(gse::key::escape)) {
				gse::shutdown();
			}

			if (gse::keyboard::pressed(gse::key::n) || gse::mouse::pressed(gse::mouse_button::button_3)) {
				m_show_cross_hair = !m_show_cross_hair;
			}
		}

		auto render() -> void  override {
			gse::gui::start(
				"Test",
				[&] {
					gse::gui::value("FPS", gse::main_clock::fps());
					gse::gui::value("Test Value", 42);
					gse::gui::value("Test Quantity", gse::meters(5.0f));
					gse::gui::vec("Test Vec", gse::vec3<gse::length>(1.f, 2.f, 3.f));
					gse::gui::vec("Test Vec2", gse::mouse::position());
					gse::gui::input("Input Test", m_buff);
					gse::gui::slider("Slider Test", m_slider_value, gse::meters(0.0f), gse::meters(10.0f));
					gse::gui::slider("Vec Slider Test", m_vec_value, gse::vec3<gse::length>(0.f), gse::vec3<gse::length>(10.f));
				}
			);

			gse::gui::start(
				"Test2",
				[] {
					gse::gui::text("Hello, world!");
					gse::gui::value("Test Value", 42);
					gse::gui::value("Test Quantity", gse::meters(5.0f));
					gse::gui::vec("Test Vec", gse::vec3<gse::length>(1.f, 2.f, 3.f));
					gse::gui::vec("Test Vec2", gse::mouse::position());
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

export module game;

export import game.config;

import gse;

export namespace gs {
	class game final : public gse::hook<gse::engine> {
	public:
		explicit game(gse::engine* owner) : hook(owner) {}

		auto initialize() -> void  override {

		}

		auto update() -> void  override {

		}

		auto render() -> void  override {

		}
	};
}

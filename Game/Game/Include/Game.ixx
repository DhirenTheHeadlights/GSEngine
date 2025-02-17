export module game;

export import game.config;

export namespace game {
	auto initialize() -> bool;
	auto update() -> bool;
	auto render() -> bool;
	auto close() -> bool;

	auto set_input_handling_flag(bool enabled) -> void;
}

export module ide:editor_app;

import std;
import gse;

import :workspace;
import :syntax_producer;
import :lsp_client;
import :command_registry;
import :commands;
import :config_system;

export namespace ide {
struct editor_app {
  struct data {
    std::vector<gse::gui::text_buffer> buffers;
    std::vector<gse::gui::text_area_state> views;
    workspace::data ws;
    syntax_producer::data syntax;
    lsp::client::data lsp;
    command_registry commands;
  };

  static auto run(gse::run_context &ctx, data &d,
                  const gse::gui::system::data &gui_d,
                  const gse::window::data &window_d) -> gse::async::task<>;
};
} // namespace ide

auto ide::editor_app::run(gse::run_context &ctx, data &d,
                          const gse::gui::system::data &gui_d,
                          const gse::window::data &window_d)
    -> gse::async::task<> {
  ide::discover_commands<^^ide::commands>(d.commands);
  while (true) {
    co_await ctx.next_tick();
  }
}

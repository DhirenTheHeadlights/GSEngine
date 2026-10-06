export module gse.os:clipboard;

import std;

import gse.log;
import gse.math;
import gse.win32;
import gse.win32.environment;

export namespace gse::clipboard {
	struct image {
		std::filesystem::path path;
		vec2u size;
		std::vector<std::byte> pixels;
	};

	[[nodiscard]] auto text() -> std::string;

	auto set_text(
		std::string value
	) -> void;

	[[nodiscard]] auto image_available() -> bool;

	auto request_image() -> void;

	[[nodiscard]] auto image_pending() -> bool;

	[[nodiscard]] auto take_image() -> std::optional<image>;

	auto sync(
		win32::HWND owner
	) -> void;
}

namespace gse::clipboard {
	struct bridge {
		std::mutex guard;
		std::string cached_text;
		std::optional<std::string> pending_text;
		std::optional<image> ready_image;
		bool primed = false;
		win32::DWORD sequence = 0;
		std::atomic<bool> image_present{ false };
		std::atomic<bool> image_wanted{ false };
	};

	inline bridge state;

	auto write_text(
		win32::HWND owner,
		const std::string& value
	) -> bool;

	auto read_text() -> std::optional<std::string>;

	auto read_dib() -> std::optional<image>;

	auto read_file() -> std::optional<image>;

	auto sync_text(
		win32::HWND owner
	) -> void;

	auto sync_image() -> void;
}

auto gse::clipboard::write_text(const win32::HWND owner, const std::string& value) -> bool {
	const std::wstring wide = win32::widen(value);

	const win32::HGLOBAL memory = win32::GlobalAlloc(win32::gmem_moveable, (wide.size() + 1) * sizeof(wchar_t));
	if (memory == nullptr) {
		return false;
	}

	auto* destination = static_cast<wchar_t*>(win32::GlobalLock(memory));
	if (destination == nullptr) {
		win32::GlobalFree(memory);
		return false;
	}
	std::ranges::copy(wide, destination);
	destination[wide.size()] = L'\0';
	win32::GlobalUnlock(memory);

	if (!win32::OpenClipboard(owner)) {
		win32::GlobalFree(memory);
		return false;
	}

	win32::EmptyClipboard();
	const bool placed = win32::SetClipboardData(win32::cf_unicodetext, memory) != nullptr;
	win32::CloseClipboard();

	if (!placed) {
		win32::GlobalFree(memory);
		log::println(log::level::warning, log::category::general, "[clipboard] could not place text on the clipboard: win32 error {}", win32::GetLastError());
	}
	return placed;
}

auto gse::clipboard::read_text() -> std::optional<std::string> {
	if (win32::IsClipboardFormatAvailable(win32::cf_unicodetext) == 0) {
		return std::string{};
	}
	if (!win32::OpenClipboard(nullptr)) {
		return std::nullopt;
	}

	std::optional<std::string> value;
	if (const win32::HANDLE memory = win32::GetClipboardData(win32::cf_unicodetext)) {
		if (const auto* source = static_cast<const wchar_t*>(win32::GlobalLock(static_cast<win32::HGLOBAL>(memory)))) {
			value = win32::narrow(source);
			win32::GlobalUnlock(static_cast<win32::HGLOBAL>(memory));
		}
	}

	win32::CloseClipboard();
	return value;
}

auto gse::clipboard::read_dib() -> std::optional<image> {
	win32::HANDLE handle = win32::GetClipboardData(win32::cf_dibv5);
	if (!handle) {
		handle = win32::GetClipboardData(win32::cf_dib);
	}
	if (!handle) {
		return std::nullopt;
	}

	const auto* memory = static_cast<const std::byte*>(win32::GlobalLock(static_cast<win32::HGLOBAL>(handle)));
	if (!memory) {
		return std::nullopt;
	}

	const std::size_t available = win32::GlobalSize(static_cast<win32::HGLOBAL>(handle));
	const auto* header = reinterpret_cast<const win32::BITMAPINFOHEADER*>(memory);
	const std::int32_t width = header->biWidth;
	const std::int32_t signed_height = header->biHeight;
	const std::int32_t height = signed_height < 0 ? -signed_height : signed_height;
	const std::uint32_t bits = header->biBitCount;
	const bool top_down = signed_height < 0;
	const bool layout_supported = (bits == 24 || bits == 32)
		&& (header->biCompression == win32::bi_rgb || header->biCompression == win32::bi_bitfields);

	if (width <= 0 || height <= 0 || !layout_supported) {
		win32::GlobalUnlock(static_cast<win32::HGLOBAL>(handle));
		return std::nullopt;
	}

	std::size_t offset = header->biSize;
	if (header->biCompression == win32::bi_bitfields && header->biSize == win32::bitmap_info_header_size) {
		offset += 12;
	}
	offset += static_cast<std::size_t>(header->biClrUsed) * 4;

	const std::size_t stride = ((static_cast<std::size_t>(width) * bits + 31) / 32) * 4;
	if (offset + stride * static_cast<std::size_t>(height) > available) {
		win32::GlobalUnlock(static_cast<win32::HGLOBAL>(handle));
		return std::nullopt;
	}

	const std::size_t source_pixel = bits / 8;
	std::vector<std::byte> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
	bool any_opaque = false;

	for (std::int32_t y = 0; y < height; ++y) {
		const std::size_t source_row = static_cast<std::size_t>(top_down ? y : height - 1 - y);
		const std::byte* source = memory + offset + source_row * stride;
		std::byte* destination = pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4;
		for (std::int32_t x = 0; x < width; ++x) {
			const std::byte* texel = source + static_cast<std::size_t>(x) * source_pixel;
			destination[x * 4 + 0] = texel[2];
			destination[x * 4 + 1] = texel[1];
			destination[x * 4 + 2] = texel[0];
			destination[x * 4 + 3] = bits == 32 ? texel[3] : std::byte{ 0xff };
			any_opaque = any_opaque || destination[x * 4 + 3] != std::byte{ 0 };
		}
	}

	win32::GlobalUnlock(static_cast<win32::HGLOBAL>(handle));

	if (!any_opaque) {
		for (std::size_t i = 3; i < pixels.size(); i += 4) {
			pixels[i] = std::byte{ 0xff };
		}
	}

	return image{
		.size = { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) },
		.pixels = std::move(pixels),
	};
}

auto gse::clipboard::read_file() -> std::optional<image> {
	const win32::HANDLE handle = win32::GetClipboardData(win32::cf_hdrop);
	if (!handle) {
		return std::nullopt;
	}

	constexpr std::array image_extensions = {
		std::string_view(".png"),
		std::string_view(".jpg"),
		std::string_view(".jpeg"),
		std::string_view(".bmp"),
		std::string_view(".tga"),
		std::string_view(".gif"),
		std::string_view(".webp"),
	};

	const auto drop = static_cast<win32::HDROP>(handle);
	const win32::UINT count = win32::DragQueryFileW(drop, win32::drag_query_count, nullptr, 0);

	for (win32::UINT i = 0; i < count; ++i) {
		const win32::UINT length = win32::DragQueryFileW(drop, i, nullptr, 0);
		if (length == 0) {
			continue;
		}
		std::wstring name(length + 1, L'\0');
		if (win32::DragQueryFileW(drop, i, name.data(), length + 1) == 0) {
			continue;
		}
		name.resize(length);

		std::filesystem::path path(name);
		std::string extension = path.extension().display_string();
		std::ranges::transform(extension, extension.begin(), [](const char c) {
			return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		});
		if (std::ranges::contains(image_extensions, extension)) {
			return image{ .path = std::move(path) };
		}
	}

	return std::nullopt;
}

auto gse::clipboard::sync_text(const win32::HWND owner) -> void {
	std::optional<std::string> to_write;
	{
		const std::scoped_lock _(state.guard);
		to_write = std::exchange(state.pending_text, std::nullopt);
	}
	if (to_write && write_text(owner, *to_write)) {
		const std::scoped_lock _(state.guard);
		state.cached_text = std::move(*to_write);
	}

	const win32::DWORD sequence = win32::GetClipboardSequenceNumber();
	if (state.primed && sequence == state.sequence) {
		return;
	}

	if (const auto contents = read_text()) {
		state.sequence = sequence;
		state.primed = true;
		const std::scoped_lock _(state.guard);
		state.cached_text = *contents;
	}
}

auto gse::clipboard::sync_image() -> void {
	state.image_present.store(
		win32::IsClipboardFormatAvailable(win32::cf_dibv5) != 0
			|| win32::IsClipboardFormatAvailable(win32::cf_dib) != 0,
		std::memory_order_release
	);

	if (!state.image_wanted.exchange(false, std::memory_order_acq_rel)) {
		return;
	}

	if (!win32::OpenClipboard(nullptr)) {
		return;
	}

	std::optional<image> found = read_dib();
	if (!found) {
		found = read_file();
	}
	win32::CloseClipboard();

	const std::scoped_lock _(state.guard);
	state.ready_image = std::move(found);
}

auto gse::clipboard::sync(const win32::HWND owner) -> void {
	sync_text(owner);
	sync_image();
}

auto gse::clipboard::text() -> std::string {
	const std::scoped_lock _(state.guard);
	if (state.pending_text) {
		return *state.pending_text;
	}
	return state.cached_text;
}

auto gse::clipboard::set_text(std::string value) -> void {
	const std::scoped_lock _(state.guard);
	state.pending_text = std::move(value);
}

auto gse::clipboard::image_available() -> bool {
	return state.image_present.load(std::memory_order_acquire);
}

auto gse::clipboard::request_image() -> void {
	state.image_wanted.store(true, std::memory_order_release);
}

auto gse::clipboard::image_pending() -> bool {
	return state.image_wanted.load(std::memory_order_acquire);
}

auto gse::clipboard::take_image() -> std::optional<image> {
	const std::scoped_lock _(state.guard);
	return std::exchange(state.ready_image, std::nullopt);
}

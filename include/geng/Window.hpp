#ifndef GENG_WINDOW_HPP
#define GENG_WINDOW_HPP

#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>
#include <veng/rhi/Enums.hpp>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace geng
{
/// The subset of keys the windowed front-end reports, translated from GLFW so an example never has to
/// include `<GLFW/glfw3.h>`. Anything geng does not name arrives as @ref Key::UNKNOWN.
enum class Key : std::uint8_t
{
	UNKNOWN,
	UP,
	DOWN,
	LEFT,
	RIGHT,
	SPACE,
	ENTER,
	ESCAPE
};

/// What happened to a @ref Key this event (GLFW's press / release / auto-repeat).
enum class KeyAction : std::uint8_t
{
	PRESS,
	RELEASE,
	REPEAT
};
/// A minimal GLFW window for windowed Vulkan rendering: it owns the `GLFWwindow`, reports the
/// instance extensions a surface needs, and creates a `VkSurfaceKHR` from an instance. The
/// surface itself is owned by the renderer's veng context, not by this window. (veng ships an
/// equivalent only as non-installed example code, so geng keeps its own.) Window and event
/// handling must stay on the main thread — a GLFW requirement.
class Window
{
	 public:
	/// @throws std::runtime_error if GLFW init, Vulkan support, or window creation fails.
	Window(const char* title, int width, int height);
	~Window();

	Window(const Window&)			 = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&)				 = delete;
	Window& operator=(Window&&)		 = delete;

	[[nodiscard]] bool should_close() const noexcept;
	static void		   poll() noexcept;

	/// The framebuffer size in pixels (may differ from the requested window size on HiDPI).
	[[nodiscard]] veng::rhi::Extent2D framebuffer_extent() const noexcept;

	/// Instance extensions required for a window surface (`VK_KHR_surface` + the platform
	/// surface extension), with a known-good fallback when GLFW's own query comes up empty.
	[[nodiscard]] std::span<const char* const> required_extensions() const noexcept { return m_required_extensions; }

	/// Create a surface from @p instance, or `VK_NULL_HANDLE` on failure.
	[[nodiscard]] VkSurfaceKHR create_surface(VkInstance instance) const noexcept;

	/// Window size in screen coordinates — the units cursor positions use (may differ from the
	/// framebuffer size on HiDPI). Divide a cursor position by this to get a [0,1] view-fraction.
	[[nodiscard]] veng::rhi::Extent2D window_size() const noexcept;

	/// Current cursor position in screen coordinates (origin top-left): {x, y}.
	[[nodiscard]] std::pair<double, double> cursor_pos() const noexcept;

	/// Whether the left mouse button is currently held.
	[[nodiscard]] bool mouse_held() const noexcept;

	/// Register a scroll handler (offsets in detents; fired during @ref poll).
	void on_scroll(std::function<void(double offset_x, double offset_y)> callback);

	/// Register a cursor-move handler (screen coordinates; fired during @ref poll).
	void on_cursor_pos(std::function<void(double pos_x, double pos_y)> callback);

	/// Register a framebuffer-resize handler (new size in pixels; fired during @ref poll).
	void on_resize(std::function<void(int width, int height)> callback);

	/// Register a key handler (translated @ref Key + @ref KeyAction; fired during @ref poll).
	void on_key(std::function<void(Key key, KeyAction action)> callback);

	 private:
	GLFWwindow*							m_window = nullptr;
	std::vector<const char*>			m_required_extensions;
	std::function<void(double, double)> m_scroll;
	std::function<void(double, double)> m_cursor;
	std::function<void(int, int)>		m_resize;
	std::function<void(Key, KeyAction)> m_key;
};
} // namespace geng

#endif // GENG_WINDOW_HPP

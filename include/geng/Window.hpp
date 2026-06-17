#ifndef GENG_WINDOW_HPP
#define GENG_WINDOW_HPP

#include <span>
#include <vector>

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace geng
{
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
	[[nodiscard]] VkExtent2D framebuffer_extent() const noexcept;

	/// Instance extensions required for a window surface (`VK_KHR_surface` + the platform
	/// surface extension), with a known-good fallback when GLFW's own query comes up empty.
	[[nodiscard]] std::span<const char* const> required_extensions() const noexcept { return m_required_extensions; }

	/// Create a surface from @p instance, or `VK_NULL_HANDLE` on failure.
	[[nodiscard]] VkSurfaceKHR create_surface(VkInstance instance) const noexcept;

	 private:
	GLFWwindow*				 m_window = nullptr;
	std::vector<const char*> m_required_extensions;
};
} // namespace geng

#endif // GENG_WINDOW_HPP

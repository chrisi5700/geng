#include <cstdint>
#include <functional>
#include <geng/Window.hpp>
#include <print>
#include <span>
#include <stdexcept>
#include <utility>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace geng
{
namespace
{
void on_glfw_error(int code, const char* description) noexcept
{
	std::println(stderr, "[geng::Window] GLFW error {}: {}", code, description != nullptr ? description : "(null)");
}

/// Translate a GLFW key code to geng's small @ref Key vocabulary (unmapped keys become UNKNOWN).
Key to_key(int glfw_key) noexcept
{
	switch (glfw_key)
	{
		case GLFW_KEY_UP: return Key::UP;
		case GLFW_KEY_DOWN: return Key::DOWN;
		case GLFW_KEY_LEFT: return Key::LEFT;
		case GLFW_KEY_RIGHT: return Key::RIGHT;
		case GLFW_KEY_SPACE: return Key::SPACE;
		case GLFW_KEY_ENTER: return Key::ENTER;
		case GLFW_KEY_ESCAPE: return Key::ESCAPE;
		default: return Key::UNKNOWN;
	}
}

KeyAction to_key_action(int glfw_action) noexcept
{
	switch (glfw_action)
	{
		case GLFW_RELEASE: return KeyAction::RELEASE;
		case GLFW_REPEAT: return KeyAction::REPEAT;
		default: return KeyAction::PRESS;
	}
}
} // namespace

Window::Window(const char* title, int width, int height)
{
	glfwSetErrorCallback(on_glfw_error);
	if (glfwInit() != GLFW_TRUE)
	{
		throw std::runtime_error("geng::Window: glfwInit failed");
	}
	if (glfwVulkanSupported() != GLFW_TRUE)
	{
		glfwTerminate();
		throw std::runtime_error("geng::Window: GLFW reports no Vulkan support");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan only — no OpenGL context.
	m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	if (m_window == nullptr)
	{
		glfwTerminate();
		throw std::runtime_error("geng::Window: glfwCreateWindow failed");
	}

	// Forward GLFW's C callbacks to the per-window std::functions via the user pointer.
	glfwSetWindowUserPointer(m_window, this);
	glfwSetScrollCallback(m_window,
						  [](GLFWwindow* win, double offset_x, double offset_y)
						  {
							  auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
							  if (self != nullptr && self->m_scroll)
							  {
								  self->m_scroll(offset_x, offset_y);
							  }
						  });
	glfwSetCursorPosCallback(m_window,
							 [](GLFWwindow* win, double pos_x, double pos_y)
							 {
								 auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
								 if (self != nullptr && self->m_cursor)
								 {
									 self->m_cursor(pos_x, pos_y);
								 }
							 });
	glfwSetFramebufferSizeCallback(m_window,
								   [](GLFWwindow* win, int width, int height)
								   {
									   auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
									   if (self != nullptr && self->m_resize)
									   {
										   self->m_resize(width, height);
									   }
								   });
	glfwSetKeyCallback(m_window,
					   [](GLFWwindow* win, int key, int /*scancode*/, int action, int /*mods*/)
					   {
						   auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
						   if (self != nullptr && self->m_key)
						   {
							   self->m_key(to_key(key), to_key_action(action));
						   }
					   });

	std::uint32_t	   count	  = 0;
	const char* const* extensions = glfwGetRequiredInstanceExtensions(&count);
	if (extensions != nullptr && count > 0)
	{
		const std::span<const char* const> view(extensions, count);
		m_required_extensions.assign(view.begin(), view.end());
	}
	else
	{
		// GLFW's query can come up empty on XWayland even though the loader exposes the surface
		// extensions; enable the known-good ones for the active platform instead.
		m_required_extensions.push_back("VK_KHR_surface");
		if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
		{
			m_required_extensions.push_back("VK_KHR_wayland_surface");
		}
		else
		{
			m_required_extensions.push_back("VK_KHR_xcb_surface");
			m_required_extensions.push_back("VK_KHR_xlib_surface");
		}
	}
}

Window::~Window()
{
	if (m_window != nullptr)
	{
		glfwDestroyWindow(m_window);
	}
	glfwTerminate();
}

bool Window::should_close() const noexcept
{
	return glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

void Window::poll() noexcept
{
	glfwPollEvents();
}

veng::rhi::Extent2D Window::framebuffer_extent() const noexcept
{
	int width  = 0;
	int height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	return veng::rhi::Extent2D{.width  = static_cast<std::uint32_t>(width),
							   .height = static_cast<std::uint32_t>(height)};
}

VkSurfaceKHR Window::create_surface(VkInstance instance) const noexcept
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (glfwCreateWindowSurface(instance, m_window, nullptr, &surface) != VK_SUCCESS)
	{
		return VK_NULL_HANDLE;
	}
	return surface;
}

veng::rhi::Extent2D Window::window_size() const noexcept
{
	int width  = 0;
	int height = 0;
	glfwGetWindowSize(m_window, &width, &height);
	return veng::rhi::Extent2D{.width  = static_cast<std::uint32_t>(width),
							   .height = static_cast<std::uint32_t>(height)};
}

std::pair<double, double> Window::cursor_pos() const noexcept
{
	double pos_x = 0.0;
	double pos_y = 0.0;
	glfwGetCursorPos(m_window, &pos_x, &pos_y);
	return {pos_x, pos_y};
}

bool Window::mouse_held() const noexcept
{
	return glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
}

void Window::on_scroll(std::function<void(double, double)> callback)
{
	m_scroll = std::move(callback);
}

void Window::on_cursor_pos(std::function<void(double, double)> callback)
{
	m_cursor = std::move(callback);
}

void Window::on_resize(std::function<void(int, int)> callback)
{
	m_resize = std::move(callback);
}

void Window::on_key(std::function<void(Key, KeyAction)> callback)
{
	m_key = std::move(callback);
}
} // namespace geng

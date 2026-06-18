#include <cstdint>
#include <geng/Window.hpp>
#include <print>
#include <span>
#include <stdexcept>

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
} // namespace geng

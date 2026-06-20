#ifndef GENG_WINDOWAPP_HPP
#define GENG_WINDOWAPP_HPP

#include <cstddef>
#include <expected>
#include <functional>
#include <geng/Figure.hpp>
#include <geng/Interactor.hpp>
#include <geng/Window.hpp>
#include <memory>
#include <string>

// Heavy veng managers are forward-declared and held by unique_ptr — the windowed front-end is the
// only place that owns a swapchain, so none of that machinery leaks into the public header.
namespace veng
{
class Context;
class SwapchainManager;
class CommandManager;
} // namespace veng

namespace geng
{
/// A GLFW window that presents a @ref Figure, frame by frame. WindowApp plays the exact role a host
/// (e.g. `QVulkanWindow`) plays: it owns the window, the swapchain, and presentation, and drives the
/// figure through @ref Figure::render_into — so this is also a faithful, runnable proxy for the Qt
/// embedding path. The figure renders on a device this app creates; populate it via @ref figure(),
/// then call @ref run.
///
/// Gated behind the `geng-glfw` CMake target: link it only when you want a window. The core graphing
/// library stays surface-agnostic.
class WindowApp
{
	 public:
	/// Window geometry plus the figure's content/style. Frames-in-flight and the swapchain are
	/// internal — a caller just describes the window and the plot.
	struct Config
	{
		std::string title  = "geng";
		int			width  = 1280;
		int			height = 720;
		FigureDesc	figure;
	};

	/// Open the window, create the device + swapchain, and build the embedded figure. Returns an
	/// error (rather than throwing) if any step fails. (Two overloads rather than a defaulted
	/// argument: a default `Config{}` argument would force the nested struct's member initializers
	/// at class scope, which the compiler rejects.)
	[[nodiscard]] static std::expected<WindowApp, Error> create();
	[[nodiscard]] static std::expected<WindowApp, Error> create(const Config& config);

	~WindowApp();
	WindowApp(WindowApp&&) noexcept;
	WindowApp& operator=(WindowApp&&) noexcept;
	WindowApp(const WindowApp&)			   = delete;
	WindowApp& operator=(const WindowApp&) = delete;

	/// The plot this window presents — add series, set the view/theme, etc.
	[[nodiscard]] Figure&		figure() noexcept;
	[[nodiscard]] const Figure& figure() const noexcept;

	/// The input mapper, pre-wired to this window's scroll (zoom) and left-drag (pan). Reconfigure or
	/// disable it here; it is a no-op whenever the figure's @ref Fit mode owns the view.
	[[nodiscard]] Interactor& interactor() noexcept;

	/// Render until the window closes. @p tick, if set, runs once per frame (after input is polled,
	/// before the frame is drawn) with the seconds elapsed since @ref run began — feed animation here.
	void run(const std::function<void(double elapsed_seconds)>& tick = {});

	 private:
	WindowApp();

	/// Wire the default scroll-zoom / left-drag-pan gestures from the window onto @ref m_interactor.
	void install_controls();
	/// Tear down and rebuild the swapchain at @p extent (device made idle first).
	void rebuild_swapchain(veng::rhi::Extent2D extent);
	/// Acquire, render the figure into, and present one swapchain image. Returns false to stop the
	/// loop (a hard device error); an out-of-date swapchain is rebuilt in place and returns true.
	[[nodiscard]] bool draw_frame(std::size_t counter);

	// Held by unique_ptr so their addresses stay stable across a WindowApp move (the input callbacks
	// and the Interactor capture pointers into these), and so the move is trivial.
	std::unique_ptr<Window>					m_window;
	std::unique_ptr<veng::Context>			m_ctx; ///< This app's own device (it plays the host).
	std::unique_ptr<veng::SwapchainManager> m_swap;
	std::unique_ptr<veng::CommandManager>	m_commands;
	std::unique_ptr<Figure>					m_figure;
	std::unique_ptr<Interactor>				m_interactor;
};
} // namespace geng

#endif // GENG_WINDOWAPP_HPP

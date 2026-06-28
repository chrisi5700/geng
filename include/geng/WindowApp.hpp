#ifndef GENG_WINDOWAPP_HPP
#define GENG_WINDOWAPP_HPP

#include <array>
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
class ResourcePool;
class HeadlessExecutor;
} // namespace veng

namespace feng
{
class FontAtlas; // text-rendering library, used for the fixed corner overlay (see set_overlay_text)
} // namespace feng

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

	/// Register a key handler on the window — translated @ref Key + @ref KeyAction, fired during @ref
	/// run's input poll. Use it to drive the figure from the keyboard (e.g. step a time index).
	void on_key(std::function<void(Key key, KeyAction action)> callback);

	/// Set a fixed text badge drawn in the top-right corner, in screen space — it never pans or zooms
	/// with the plot (a HUD label, not plot data; rendered with feng, the text library). Pass an empty
	/// string to clear it. The badge is re-rendered only on change, then blitted each frame. The first
	/// call lazily bakes a font; if that fails the overlay is silently skipped.
	void set_overlay_text(const std::string& text);

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

	/// Lazily bake the overlay font and wire its one-node feng graph (called by the first
	/// @ref set_overlay_text). Returns false — and leaves the overlay disabled — if the font won't load.
	[[nodiscard]] bool init_overlay();
	/// Blit the cached overlay badge into the top-right corner of @p swap_image (in @p cmd). No-op when
	/// no overlay text is set. The swap image must be in TRANSFER_DST (as render_into leaves it).
	void blit_overlay(VkCommandBuffer cmd_raw, VkImage swap_raw, veng::rhi::Extent2D extent);

	// Held by unique_ptr so their addresses stay stable across a WindowApp move (the input callbacks
	// and the Interactor capture pointers into these), and so the move is trivial.
	std::unique_ptr<Window>					m_window;
	std::unique_ptr<veng::Context>			m_ctx; ///< This app's own device (it plays the host).
	std::unique_ptr<veng::SwapchainManager> m_swap;
	std::unique_ptr<veng::CommandManager>	m_commands;
	std::unique_ptr<Figure>					m_figure;
	std::unique_ptr<Interactor>				m_interactor;

	// Fixed corner text overlay (feng). Rendered to a small badge image only when the text changes (via
	// the headless executor, like render_png), then blitted to the corner every frame. All null until
	// the first set_overlay_text. m_overlay_bg / m_overlay_font_path are captured from the figure config.
	std::unique_ptr<feng::FontAtlas>		m_overlay_font;
	std::unique_ptr<veng::ResourcePool>		m_overlay_pool;
	std::unique_ptr<veng::CommandManager>	m_overlay_commands;
	std::unique_ptr<veng::graph::Scheduler> m_overlay_scheduler;
	std::unique_ptr<veng::HeadlessExecutor> m_overlay_headless;
	std::unique_ptr<veng::graph::Graph>		m_overlay_graph;
	veng::graph::DataHandle					m_overlay_glyphs_src; ///< source<std::vector<feng::Glyph>>
	veng::graph::DataHandle					m_overlay_image;	  ///< the baked badge ImageRef
	std::string								m_overlay_font_path;  ///< resolved bundled/spec font for the badge
	float									m_overlay_font_height = 96.0F;
	std::array<float, 4>					m_overlay_bg{0.0F, 0.0F, 0.0F, 1.0F}; ///< opaque badge background (RGBA)
	bool									m_overlay_ready	 = false; ///< a badge has been baked and can blit
	bool									m_overlay_failed = false; ///< font load failed; stop retrying
};
} // namespace geng

#endif // GENG_WINDOWAPP_HPP

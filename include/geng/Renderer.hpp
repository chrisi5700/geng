#ifndef GENG_RENDERER_HPP
#define GENG_RENDERER_HPP

#include <geng/Window.hpp>
#include <memory>
#include <mutex>
#include <vector>
#include <veng/gpu/ImageRef.hpp>
#include <veng/rendergraph/Graph.hpp>
#include <veng/rhi/Enums.hpp>
#include <vulkan/vulkan.hpp>

namespace veng
{
class Context;
class SwapchainManager;
class ResourcePool;
class CommandManager;
class FrameExecutor;
namespace nodes
{
class PresentNode;
}
} // namespace veng

namespace geng
{
/// Owns the windowed veng stack — context, swapchain, resource pool, command manager, frame
/// executor — and a render graph pre-wired with the frame-closer tail (BlitNode → PresentNode).
/// A caller builds its scene by pointing `scene_image()`'s producer at a `GraphicsNode`, then
/// calls `run()`. This is geng's equivalent of veng's example `AppLoop`, minus the 3D camera.
///
/// Not the public graphing API — just the rendering plumbing the first slice is built on.
class Renderer
{
	 public:
	/// @throws std::runtime_error if any veng initialisation step fails.
	Renderer(const char* title, int width, int height);
	~Renderer();

	Renderer(const Renderer&)			 = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&)				 = delete;
	Renderer& operator=(Renderer&&)		 = delete;

	[[nodiscard]] veng::graph::Graph& graph() noexcept { return m_graph; }

	/// The veng context backing this renderer (device + allocator) — e.g. to build a FontAtlas.
	[[nodiscard]] veng::Context& context() noexcept;

	/// The window this renderer draws into — e.g. to register scroll/cursor callbacks for view control.
	[[nodiscard]] Window& window() noexcept { return m_window; }

	/// The screen-size edge (`Extent2D`); sizes render targets and updates on resize.
	[[nodiscard]] veng::graph::TypedHandle<veng::rhi::Extent2D> screen() const noexcept { return m_screen; }

	/// The edge a scene producer publishes its rendered color image on (blitted to the swapchain).
	[[nodiscard]] veng::graph::DataHandle scene_image() const noexcept { return m_scene_image; }

	/// The format a scene producer should render in (the swapchain format).
	[[nodiscard]] veng::rhi::Format scene_color_format() const noexcept { return m_scene_color_format; }

	/// Render until the window closes. OnDemand pacing: a frame is produced only when the graph
	/// changes, so a static plot renders once and then idles.
	void run();

	 private:
	void rebuild_swapchain(veng::rhi::Extent2D extent);

	Window										  m_window;
	std::unique_ptr<veng::Context>				  m_ctx;
	std::unique_ptr<veng::SwapchainManager>		  m_swap;
	std::unique_ptr<veng::ResourcePool>			  m_pool;
	std::unique_ptr<veng::CommandManager>		  m_commands;
	veng::graph::InlineScheduler				  m_scheduler;
	veng::graph::Graph							  m_graph;
	veng::graph::TypedHandle<veng::rhi::Extent2D> m_screen;
	veng::graph::TypedHandle<veng::gpu::ImageRef> m_swapchain_image;
	veng::graph::DataHandle						  m_scene_image;
	veng::graph::DataHandle						  m_presented_image;
	veng::graph::DataHandle						  m_frame_done;
	veng::nodes::PresentNode*					  m_present = nullptr;
	std::unique_ptr<veng::FrameExecutor>		  m_executor;
	std::vector<veng::graph::DataHandle>		  m_sinks;
	std::mutex									  m_graph_mutex;
	veng::rhi::Format							  m_scene_color_format = veng::rhi::Format::UNDEFINED;
};
} // namespace geng

#endif // GENG_RENDERER_HPP

#ifndef GENG_OFFSCREEN_RENDERER_HPP
#define GENG_OFFSCREEN_RENDERER_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <veng/gpu/ImageRef.hpp>
#include <veng/rendergraph/Graph.hpp>
#include <veng/rhi/Enums.hpp>

namespace veng
{
class Context;
class ResourcePool;
class CommandManager;
class HeadlessExecutor;
} // namespace veng

namespace geng
{
/// Headless, single-shot renderer: drives a veng graph with no window or swapchain, renders one
/// frame into an offscreen RGBA8 target, reads it back, and writes a PNG. The render-to-file
/// counterpart of the windowed Renderer — same graph interface (`graph()` / `screen()` /
/// `scene_image()` / `scene_color_format()`), so a scene wired for one renders on the other.
class OffscreenRenderer
{
	 public:
	/// @throws std::runtime_error if the headless veng context fails to initialise.
	OffscreenRenderer(std::uint32_t width, std::uint32_t height);
	~OffscreenRenderer();

	OffscreenRenderer(const OffscreenRenderer&)			   = delete;
	OffscreenRenderer& operator=(const OffscreenRenderer&) = delete;
	OffscreenRenderer(OffscreenRenderer&&)				   = delete;
	OffscreenRenderer& operator=(OffscreenRenderer&&)	   = delete;

	[[nodiscard]] veng::graph::Graph& graph() noexcept { return m_graph; }

	/// The veng context backing this renderer (device + allocator) — e.g. to build a FontAtlas.
	[[nodiscard]] veng::Context& context() noexcept;

	/// The fixed screen-size edge sizing the offscreen render target.
	[[nodiscard]] veng::graph::TypedHandle<veng::rhi::Extent2D> screen() const noexcept { return m_screen; }

	/// The edge a scene producer publishes its rendered color image on.
	[[nodiscard]] veng::graph::DataHandle scene_image() const noexcept { return m_scene_image; }

	/// The format a scene producer should render in (RGBA8, for readback).
	[[nodiscard]] veng::rhi::Format scene_color_format() const noexcept { return m_scene_color_format; }

	/// Render one frame of the wired graph and write it to @p png_path. Returns false on failure.
	[[nodiscard]] bool capture_png(const std::string& png_path);

	 private:
	std::unique_ptr<veng::Context>				  m_ctx;
	std::unique_ptr<veng::ResourcePool>			  m_pool;
	std::unique_ptr<veng::CommandManager>		  m_commands;
	veng::graph::InlineScheduler				  m_scheduler;
	std::unique_ptr<veng::HeadlessExecutor>		  m_executor;
	veng::graph::Graph							  m_graph;
	veng::graph::TypedHandle<veng::rhi::Extent2D> m_screen;
	veng::graph::DataHandle						  m_scene_image;
	veng::rhi::Extent2D							  m_extent;
	veng::rhi::Format							  m_scene_color_format = veng::rhi::Format::RGBA8_UNORM;
};
} // namespace geng

#endif // GENG_OFFSCREEN_RENDERER_HPP

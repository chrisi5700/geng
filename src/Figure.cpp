// geng::Figure — the public 2D plot. Owns a veng device (geng-created offscreen, or host-adopted),
// the reactive bake+composite graph (wired in FigureScene.hpp), and the scene model (series
// registry, view, theme, autoscale). This file is the orchestration; the glm-heavy scene DSL lives
// in the header so clang-tidy's union-access rule (which only bites .cpp main files) stays happy.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <geng/Figure.hpp>
#include <geng/FontAtlas.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <veng/context/Context.hpp>
#include <veng/gpu/GpuExecContext.hpp>
#include <veng/gpu/ImageRef.hpp>
#include <veng/managers/CommandManager.hpp>
#include <veng/managers/HeadlessExecutor.hpp>
#include <veng/nodes/ScreenshotNode.hpp>
#include <veng/rendergraph/data/Data.hpp>
#include <veng/rendergraph/Graph.hpp>
#include <veng/rendergraph/nodes/Node.hpp>
#include <veng/resources/ResourcePool.hpp>
#include <veng/rhi/CommandEncoder.hpp>

#include "FigureScene.hpp"

namespace geng
{
namespace
{
constexpr std::size_t FRAMES_IN_FLIGHT = 2; ///< N-buffering depth (covers a typical host swapchain).

/// geng's bundled shader directory: a `GENG_SHADER_DIR` env override (so an installed package can
/// relocate it away from the source tree), else the compile-time default baked at build time.
std::string shader_dir()
{
	const char* const env = std::getenv("GENG_SHADER_DIR");
	return (env != nullptr && *env != '\0') ? std::string(env) : std::string(GENG_SHADER_DIR);
}

/// geng's bundled asset directory (default tick-label font), with the same `GENG_ASSET_DIR` override.
std::string asset_dir()
{
	const char* const env = std::getenv("GENG_ASSET_DIR");
	return (env != nullptr && *env != '\0') ? std::string(env) : std::string(GENG_ASSET_DIR);
}
} // namespace

using veng::graph::TypedHandle;

Figure::Figure()							 = default;
Figure::~Figure()							 = default;
Figure::Figure(Figure&&) noexcept			 = default;
Figure& Figure::operator=(Figure&&) noexcept = default;

std::expected<Figure, Error> Figure::offscreen(const FigureDesc& desc)
{
	const std::string					  shader_root = shader_dir();
	const std::array<std::string_view, 1> shader_paths{shader_root};
	auto								  ctx_result = veng::Context::create("geng", {}, {}, shader_paths);
	if (!ctx_result.has_value())
	{
		return std::unexpected(Error::DEVICE_CREATION_FAILED);
	}
	return build(std::make_unique<veng::Context>(std::move(ctx_result.value())), desc);
}

std::expected<Figure, Error> Figure::embedded(const VulkanContext& host, const FigureDesc& desc)
{
	const std::string					  shader_root = shader_dir();
	const std::array<std::string_view, 1> shader_paths{shader_root};
	auto ctx_result = veng::Context::adopt(vk::Instance(host.instance), vk::PhysicalDevice(host.physical_device),
										   vk::Device(host.device), vk::Queue(host.graphics_queue),
										   host.graphics_family, shader_paths);
	if (!ctx_result.has_value())
	{
		return std::unexpected(Error::DEVICE_CREATION_FAILED);
	}
	return build(std::make_unique<veng::Context>(std::move(ctx_result.value())), desc);
}

std::expected<Figure, Error> Figure::build(std::unique_ptr<veng::Context> ctx, const FigureDesc& desc)
{
	const std::string font_path =
		desc.font.path.empty() ? (asset_dir() + "/fonts/FiraCodeNerdFontMono-Regular.ttf") : desc.font.path;
	auto atlas = FontAtlas::create(*ctx, font_path, desc.font.pixel_height);
	if (!atlas.has_value())
	{
		return std::unexpected(Error::FONT_LOAD_FAILED);
	}

	Figure fig;
	fig.m_theme	   = desc.theme;
	fig.m_view	   = View(desc.initial_view);
	fig.m_ctx	   = std::move(ctx);
	fig.m_pool	   = std::make_unique<veng::ResourcePool>(fig.m_ctx->device(), fig.m_ctx->rhi(), fig.m_ctx->allocator(),
														  FRAMES_IN_FLIGHT);
	fig.m_commands = std::make_unique<veng::CommandManager>(*fig.m_ctx);
	fig.m_scheduler = std::make_unique<veng::graph::InlineScheduler>();
	fig.m_headless =
		std::make_unique<veng::HeadlessExecutor>(*fig.m_ctx, *fig.m_pool, *fig.m_commands, *fig.m_scheduler);
	fig.m_graph = std::make_unique<veng::graph::Graph>();
	fig.m_font	= std::make_unique<FontAtlas>(std::move(atlas.value()));

	auto&	   graph	  = *fig.m_graph;
	const auto screen	  = graph.add_source<veng::rhi::Extent2D>(veng::rhi::Extent2D{.width = 1, .height = 1});
	const auto view_src	  = graph.add_source<Bounds2D>(desc.initial_view);
	const auto theme_src  = graph.add_source<Theme>(desc.theme);
	const auto curves_src = graph.add_source<std::vector<detail::Curve>>({});

	fig.m_screen	  = screen.handle;
	fig.m_view_src	  = view_src.handle;
	fig.m_theme_src	  = theme_src.handle;
	fig.m_curves_src  = curves_src.handle;
	fig.m_scene_image = detail::wire_scene(graph, screen, view_src, theme_src, curves_src, *fig.m_font, desc.theme);
	return fig;
}

SeriesId Figure::add_line(std::string name, LineStyle style)
{
	const std::uint64_t key = m_next_id++;
	SeriesData			data;
	data.name  = std::move(name);
	data.style = style;
	m_series.emplace(key, std::move(data));
	m_order.push_back(key);
	m_scene_dirty = true;
	return SeriesId{key};
}

void Figure::append(SeriesId series, std::span<const glm::vec2> points)
{
	if (auto it = m_series.find(series.value()); it != m_series.end())
	{
		it->second.points.insert(it->second.points.end(), points.begin(), points.end());
		m_scene_dirty = true;
	}
}

void Figure::set_data(SeriesId series, std::vector<glm::vec2> points)
{
	if (auto it = m_series.find(series.value()); it != m_series.end())
	{
		it->second.points = std::move(points);
		m_scene_dirty	  = true;
	}
}

void Figure::remove(SeriesId series)
{
	if (m_series.erase(series.value()) != 0U)
	{
		std::erase(m_order, series.value());
		m_scene_dirty = true;
	}
}

void Figure::clear()
{
	m_series.clear();
	m_order.clear();
	m_scene_dirty = true;
}

void Figure::set_style(SeriesId series, const LineStyle& style)
{
	if (auto it = m_series.find(series.value()); it != m_series.end())
	{
		it->second.style = style;
		m_scene_dirty	 = true;
	}
}

void Figure::set_name(SeriesId series, const std::string& name)
{
	if (auto it = m_series.find(series.value()); it != m_series.end())
	{
		it->second.name = name;
	}
}

bool Figure::contains(SeriesId series) const noexcept
{
	return m_series.contains(series.value());
}

std::size_t Figure::series_count() const noexcept
{
	return m_series.size();
}

const View& Figure::view() const noexcept
{
	return m_view;
}

Bounds2D Figure::data_bounds() const noexcept
{
	std::vector<std::span<const glm::vec2>> spans;
	spans.reserve(m_order.size());
	for (const std::uint64_t key : m_order)
	{
		if (const auto it = m_series.find(key); it != m_series.end() && it->second.style.visible)
		{
			spans.emplace_back(it->second.points);
		}
	}
	return detail::bounds_of(spans);
}

Bounds2D Figure::follow_bounds() const
{
	std::vector<std::span<const glm::vec2>> spans;
	spans.reserve(m_order.size());
	for (const std::uint64_t key : m_order)
	{
		if (const auto it = m_series.find(key); it != m_series.end() && it->second.style.visible)
		{
			spans.emplace_back(it->second.points);
		}
	}
	return detail::follow_bounds(spans, m_follow_width, detail::bounds_of(spans));
}

void Figure::pan(glm::vec2 delta_fraction)
{
	m_view.pan(delta_fraction);
}

void Figure::zoom(glm::vec2 anchor_fraction, float factor)
{
	m_view.zoom_at(anchor_fraction, factor);
}

void Figure::focus(const Bounds2D& rect)
{
	m_view.focus(rect);
}

void Figure::fit_data()
{
	m_view.focus(data_bounds());
}

void Figure::autoscale(Fit mode) noexcept
{
	if (mode == Fit::FOLLOW_LATEST && m_fit != Fit::FOLLOW_LATEST)
	{
		const Bounds2D rect = m_view.rect();
		m_follow_width		= rect.max_x - rect.min_x; // freeze the current view width as the follow window.
	}
	m_fit = mode;
}

Fit Figure::autoscale() const noexcept
{
	return m_fit;
}

void Figure::set_theme(const Theme& new_theme)
{
	m_theme		  = new_theme;
	m_scene_dirty = true;
}

const Theme& Figure::theme() const noexcept
{
	return m_theme;
}

void Figure::sync_scene()
{
	if (m_fit == Fit::ALL)
	{
		m_view.focus(data_bounds()); // autoscale owns the view while active.
	}
	else if (m_fit == Fit::FOLLOW_LATEST)
	{
		m_view.focus(follow_bounds()); // fixed-width window tracking the latest data.
	}
	if (!m_scene_dirty)
	{
		return;
	}
	std::vector<detail::Curve> curves;
	curves.reserve(m_order.size());
	for (const std::uint64_t key : m_order)
	{
		const auto it = m_series.find(key);
		if (it == m_series.end() || !it->second.style.visible)
		{
			continue;
		}
		curves.push_back(detail::Curve{.points = it->second.points,
									   .color  = detail::resolve_color(it->second.style, key - 1U, m_theme)});
	}
	m_graph->set(TypedHandle<std::vector<detail::Curve>>{m_curves_src}, std::move(curves));
	m_graph->set(TypedHandle<Theme>{m_theme_src}, m_theme);
	m_scene_dirty = false;
}

bool Figure::ensure_device_sized(std::uint32_t width, std::uint32_t height)
{
	if (width == 0 || height == 0)
	{
		return false;
	}
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	m_graph->set(TypedHandle<veng::rhi::Extent2D>{m_screen}, veng::rhi::Extent2D{.width = width, .height = height});
	// Project the aspect-corrected view so a unit circle renders round at any target size.
	m_graph->set(TypedHandle<Bounds2D>{m_view_src}, aspect_fit(m_view.rect(), aspect));
	return true;
}

std::expected<void, Error> Figure::render_png(std::uint32_t width, std::uint32_t height, const std::string& path)
{
	sync_scene();
	if (!ensure_device_sized(width, height))
	{
		return std::unexpected(Error::TARGET_INVALID);
	}
	const veng::graph::DataHandle done = m_graph->add(std::make_unique<veng::graph::ValueData<int>>(0));
	m_graph->set_producer(done, m_graph->add(std::make_unique<veng::nodes::ScreenshotNode>(
									m_scene_image, done, path, veng::nodes::ImageFormat::Png)));
	if (!m_headless->run_once(*m_graph, std::array{done}).has_value())
	{
		return std::unexpected(Error::RENDER_FAILED);
	}
	return {};
}

bool Figure::needs_redraw(std::uint32_t width, std::uint32_t height)
{
	// The same front half as render_into (sync the scene, size the device sources, resolve) but without
	// acquiring a target or recording any GPU work — a non-empty plan means the scene image would differ
	// from the one currently on screen. Re-resolving in render_into afterwards is cheap and idempotent.
	sync_scene();
	if (!ensure_device_sized(width, height))
	{
		return false;
	}
	const auto plan = m_graph->resolve(std::array{m_scene_image});
	return plan.has_value() && !plan->empty();
}

FrameStatus Figure::render_into(const VulkanTarget& target)
{
	// NOTE: the graph execute below is the proven path; the final blit into the host image makes
	// assumptions about the host frame contract (the command buffer is recording and outside a render
	// pass, and geng leaves the target in TRANSFER_DST). This path is compile-verified only — it must
	// be confirmed against a real QVulkanWindow before being trusted.
	if (target.command_buffer == VK_NULL_HANDLE || target.image == VK_NULL_HANDLE)
	{
		return FrameStatus::FAILED;
	}
	sync_scene();
	if (!ensure_device_sized(target.width, target.height))
	{
		return FrameStatus::FAILED;
	}

	auto plan = m_graph->resolve(std::array{m_scene_image});
	if (!plan.has_value())
	{
		return FrameStatus::FAILED;
	}
	if (plan->empty())
	{
		return FrameStatus::IDLED;
	}

	const std::size_t slot = target.frame_index % FRAMES_IN_FLIGHT;
	m_pool->begin_frame(target.frame_index);

	const vk::CommandBuffer	  cmd{target.command_buffer};
	veng::gpu::GpuExecContext gpu_ctx(*m_graph, *m_ctx, *m_pool, veng::rhi::CommandEncoder(cmd, m_ctx->rhi()), slot);
	if (!m_graph->execute(*plan, *m_scheduler, gpu_ctx))
	{
		return FrameStatus::FAILED;
	}

	const auto* scene = dynamic_cast<veng::graph::ValueData<veng::gpu::ImageRef>*>(m_graph->get_data(m_scene_image));
	if (scene == nullptr)
	{
		return FrameStatus::FAILED;
	}
	const veng::gpu::ImageRef ref = scene->value();
	const vk::Image			  src = m_ctx->rhi().image(ref.texture);
	const vk::Image			  dst{target.image};

	// The display pass left the scene image in color-attachment layout; move it to transfer-src.
	m_pool->transition_image(ref.pool_id, cmd, vk::ImageLayout::eTransferSrcOptimal,
							 vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);

	const auto range =
		vk::ImageSubresourceRange().setAspectMask(vk::ImageAspectFlagBits::eColor).setLevelCount(1).setLayerCount(1);
	const auto to_dst = vk::ImageMemoryBarrier2()
							.setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
							.setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
							.setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
							.setOldLayout(vk::ImageLayout::eUndefined)
							.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
							.setImage(dst)
							.setSubresourceRange(range);
	cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(to_dst));

	const std::array src_offsets{vk::Offset3D{0, 0, 0}, vk::Offset3D{static_cast<std::int32_t>(ref.extent.width),
																	 static_cast<std::int32_t>(ref.extent.height), 1}};
	const std::array dst_offsets{vk::Offset3D{0, 0, 0}, vk::Offset3D{static_cast<std::int32_t>(target.width),
																	 static_cast<std::int32_t>(target.height), 1}};
	const auto layers = vk::ImageSubresourceLayers().setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1);
	const auto blit	  = vk::ImageBlit2()
							.setSrcOffsets(src_offsets)
							.setDstOffsets(dst_offsets)
							.setSrcSubresource(layers)
							.setDstSubresource(layers);
	cmd.blitImage2(vk::BlitImageInfo2()
					   .setSrcImage(src)
					   .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
					   .setDstImage(dst)
					   .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
					   .setRegions(blit)
					   .setFilter(vk::Filter::eLinear));
	return FrameStatus::DRAWN;
}
} // namespace geng

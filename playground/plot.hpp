#ifndef GENG_PLAYGROUND_PLOT_HPP
#define GENG_PLAYGROUND_PLOT_HPP

// Shared scene wiring for the demos, built as a two-stage raster cache:
//
//   points ─► aabb = fit_bounds(points)            (reactive CPU node — auto-refits on data change)
//          └► [BAKE]  line.vert/line.frag GraphicsNode, target = screen x SUPERSAMPLE
//                     projection = ortho_view(aabb) ──► baked_plot (the cached raster)
//   baked_plot ─► [DISPLAY] fullscreen.vert/display.frag, samples the cache ──► scene_image
//
// The bake renders the curve once into an offscreen, supersampled texture; the display pass just
// resolves that cache onto the screen. The demand graph memoizes the bake, so a static plot bakes
// a single time and the per-frame cost is one fullscreen sample. Works unchanged against either
// the windowed Renderer or the headless OffscreenRenderer — same handle types, one graph.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <veng/gpu/BufferRef.hpp>
#include <veng/gpu/ImageRef.hpp>
#include <veng/nodes/GraphicsNode.hpp>
#include <veng/nodes/StorageBufferNode.hpp>
#include <veng/rendergraph/Graph.hpp>
#include <veng/rendergraph/data/Data.hpp>
#include <veng/rhi/Enums.hpp>

#include <geng/Bounds2D.hpp>

namespace demo
{
/// Matches `PushData` in shaders/line.vert.slang (std430: mat4 @0, vec2 @64, float @72).
struct LineUniforms
{
	glm::mat4 view_proj;
	glm::vec2 extent;
	float	  half_width = 0.0F;

	friend bool operator==(const LineUniforms&, const LineUniforms&) noexcept = default;
};

/// Supersampling factor for the bake: the cache texture is the screen size times this on each
/// axis, so a 1280x720 window bakes at ~4K. Higher = smoother (more downsample) but more VRAM.
inline constexpr std::uint32_t SUPERSAMPLE = 3;
/// On-screen half line width, in pixels; scaled into bake-texel space below.
inline constexpr float SCREEN_HALF_WIDTH_PX = 1.5F;
inline constexpr float BAKE_HALF_WIDTH_PX	= SCREEN_HALF_WIDTH_PX * static_cast<float>(SUPERSAMPLE);
inline constexpr std::size_t SAMPLE_COUNT	= 256;

inline std::vector<glm::vec2> sample_sin(const geng::Bounds2D& bounds, std::size_t samples)
{
	std::vector<glm::vec2> points;
	points.reserve(samples);
	for (std::size_t idx = 0; idx < samples; ++idx)
	{
		const float frac  = static_cast<float>(idx) / static_cast<float>(samples - 1);
		const float pos_x = bounds.min_x + (frac * (bounds.max_x - bounds.min_x));
		points.emplace_back(pos_x, std::sin(pos_x));
	}
	return points;
}

/// Tight axis-aligned bounding box of @p points, padded by a small margin so the curve does not
/// touch the frame edge. Used as a reactive CPU node — re-fits whenever the points change.
inline geng::Bounds2D fit_bounds(const std::vector<glm::vec2>& points)
{
	constexpr float MARGIN_FRAC = 0.08F;
	if (points.empty())
	{
		return geng::Bounds2D{.min_x = -1.0F, .max_x = 1.0F, .min_y = -1.0F, .max_y = 1.0F};
	}
	float min_x = points.front().x;
	float max_x = min_x;
	float min_y = points.front().y;
	float max_y = min_y;
	for (const glm::vec2& point : points)
	{
		min_x = std::min(min_x, point.x);
		max_x = std::max(max_x, point.x);
		min_y = std::min(min_y, point.y);
		max_y = std::max(max_y, point.y);
	}
	const float pad_x = std::max(max_x - min_x, 1e-6F) * MARGIN_FRAC;
	const float pad_y = std::max(max_y - min_y, 1e-6F) * MARGIN_FRAC;
	return geng::Bounds2D{
		.min_x = min_x - pad_x, .max_x = max_x + pad_x, .min_y = min_y - pad_y, .max_y = max_y + pad_y};
}

/// Wire a single sin(x) curve into @p graph as a raster cache, producing into @p scene_image.
inline void plot_sin(veng::graph::Graph& graph, veng::graph::TypedHandle<veng::rhi::Extent2D> screen,
					 veng::graph::DataHandle scene_image, veng::rhi::Format color_format, const geng::Bounds2D& bounds)
{
	using namespace veng;
	using namespace veng::graph;

	const std::vector<glm::vec2> points = sample_sin(bounds, SAMPLE_COUNT);

	// Points SSBO: a source vector uploaded once into a StructuredBuffer the bake shader indexes.
	const auto		 points_src = graph.add_source<std::vector<glm::vec2>>(points);
	const DataHandle points_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
	graph.set_producer(points_ref,
					   graph.add(std::make_unique<nodes::StorageBufferNode>(points_src, "points", points_ref)));

	// Reactive data extent and the bake projection derived from it.
	const auto aabb = graph.add_transform([](const std::vector<glm::vec2>& pts) { return fit_bounds(pts); }, points_src);
	const auto view_proj = graph.add_transform([](const geng::Bounds2D& box) { return geng::ortho_view(box); }, aabb);

	// Bake resolution = screen x SUPERSAMPLE, tracked reactively so a resize re-bakes at the right size.
	const auto bake_extent = graph.add_transform(
		[](const rhi::Extent2D& size)
		{ return rhi::Extent2D{.width = size.width * SUPERSAMPLE, .height = size.height * SUPERSAMPLE}; }, screen);

	// Bake uniforms: line width is in bake-texel space (pixel-constant within the cache texture).
	const auto bake_uniforms = graph.add_transform(
		[](const glm::mat4& proj, const rhi::Extent2D& size)
		{
			return LineUniforms{.view_proj = proj,
								.extent		= glm::vec2(static_cast<float>(size.width), static_cast<float>(size.height)),
								.half_width = BAKE_HALF_WIDTH_PX};
		},
		view_proj, bake_extent);

	// BAKE: render the curve into an offscreen RGBA8 cache, sized by bake_extent (not the screen).
	const DataHandle baked_plot = graph.add(std::make_unique<ValueData<gpu::ImageRef>>(gpu::ImageRef{}));
	auto			 bake = std::make_unique<nodes::GraphicsNode>("line.vert", "line.frag", rhi::Format::RGBA8_UNORM,
													  rhi::Format::UNDEFINED, 6, bake_extent, baked_plot);
	bake->add_storage_buffer(points_ref)
		.set_instances_from(points_ref)
		.push_constant<LineUniforms>(bake_uniforms, rhi::ShaderStage::VERTEX)
		.clear_color({0.06F, 0.06F, 0.09F, 1.0F});
	graph.set_producer(baked_plot, graph.add(std::move(bake)));

	// DISPLAY: a fullscreen pass that samples the cache onto the screen (downsampling resolve = SSAA).
	auto display = std::make_unique<nodes::GraphicsNode>("fullscreen.vert", "display.frag", color_format,
														 rhi::Format::UNDEFINED, 3, screen, scene_image);
	display->add_sampled_image(baked_plot, "plot");
	graph.set_producer(scene_image, graph.add(std::move(display)));
}
} // namespace demo

#endif // GENG_PLAYGROUND_PLOT_HPP

#ifndef GENG_PLAYGROUND_PLOT_HPP
#define GENG_PLAYGROUND_PLOT_HPP

// Shared scene wiring for the demos, built as a two-stage raster cache:
//
//   curve ─► box = fit_bounds(curve)               (reactive CPU node — auto-refits on data change)
//         ├► positions/colors = build_*(curve, box)  (coordinate axes + curve, one line-list)
//         └► [BAKE]  line.vert/line.frag GraphicsNode, target = screen x SUPERSAMPLE
//                    projection = ortho_view(box) ──► baked_plot (the cached raster)
//   baked_plot ─► [DISPLAY] fullscreen.vert/display.frag, samples the cache ──► scene_image
//
// All lines are one line-LIST in a single buffer/draw/pass: the axes (drawn first) then the curve
// expanded segment-by-segment on top, each segment coloured from a parallel `colors` buffer. The
// bake renders that once into an offscreen, supersampled texture; the display pass resolves it onto
// the screen. The demand graph memoizes the bake, so a static plot bakes once and the per-frame
// cost is one fullscreen sample. Works unchanged against the windowed Renderer or the headless
// OffscreenRenderer — same handle types, one graph.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

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

/// A "nice" tick step (1, 2 or 5 x 10^k) for @p range, aiming for roughly @p target divisions.
inline float nice_step(float range, int target)
{
	if (range <= 0.0F || target <= 0)
	{
		return 1.0F;
	}
	const float raw	 = range / static_cast<float>(target);
	const float mag	 = std::pow(10.0F, std::floor(std::log10(raw)));
	const float norm = raw / mag;
	float		step = 10.0F;
	if (norm < 1.5F)
	{
		step = 1.0F;
	}
	else if (norm < 3.0F)
	{
		step = 2.0F;
	}
	else if (norm < 7.0F)
	{
		step = 5.0F;
	}
	return step * mag;
}

/// Tick values covering [@p lo, @p hi] at @p step, ascending.
inline std::vector<float> axis_ticks(float lo, float hi, float step)
{
	std::vector<float> ticks;
	if (step <= 0.0F)
	{
		return ticks;
	}
	const float first = std::ceil(lo / step) * step;
	for (std::size_t idx = 0; idx < 1000U; ++idx)
	{
		const float val = first + (static_cast<float>(idx) * step);
		if (val > hi)
		{
			break;
		}
		ticks.push_back(val);
	}
	return ticks;
}

/// Grid tick positions on each axis for @p box, at "nice" steps.
struct GridTicks
{
	std::vector<float> xs;
	std::vector<float> ys;
};

inline GridTicks grid_ticks(const geng::Bounds2D& box)
{
	constexpr int TARGET = 6;
	return GridTicks{.xs = axis_ticks(box.min_x, box.max_x, nice_step(box.max_x - box.min_x, TARGET)),
					 .ys = axis_ticks(box.min_y, box.max_y, nice_step(box.max_y - box.min_y, TARGET))};
}

/// A line-LIST: two endpoints per segment in @ref positions, one colour per segment in @ref colors.
struct Segments
{
	std::vector<glm::vec2> positions;
	std::vector<glm::vec4> colors;

	friend bool operator==(const Segments&, const Segments&) = default;
};

inline void add_segment(Segments& out, glm::vec2 start, glm::vec2 end, const glm::vec4& color)
{
	out.positions.push_back(start);
	out.positions.push_back(end);
	out.colors.push_back(color);
}

/// Build the plot's whole line-LIST, back to front so painter order layers it correctly: faint
/// grid, then grey coordinate axes, then the cyan curve expanded segment-by-segment on top.
inline Segments build_segments(const std::vector<glm::vec2>& curve, const geng::Bounds2D& box)
{
	const glm::vec4 grid_color{0.16F, 0.17F, 0.22F, 1.0F};
	const glm::vec4 axis_color{0.40F, 0.42F, 0.50F, 1.0F};
	const glm::vec4 curve_color{0.30F, 0.80F, 1.00F, 1.0F};

	Segments		out;
	const GridTicks ticks = grid_ticks(box);
	// Grid lines, skipping x=0 / y=0 — the brighter axes draw those.
	for (const float tick_x : ticks.xs)
	{
		if (std::abs(tick_x) >= 1e-4F)
		{
			add_segment(out, {tick_x, box.min_y}, {tick_x, box.max_y}, grid_color);
		}
	}
	for (const float tick_y : ticks.ys)
	{
		if (std::abs(tick_y) >= 1e-4F)
		{
			add_segment(out, {box.min_x, tick_y}, {box.max_x, tick_y}, grid_color);
		}
	}
	// Coordinate axes through the origin, over the grid.
	add_segment(out, {box.min_x, 0.0F}, {box.max_x, 0.0F}, axis_color); // x-axis
	add_segment(out, {0.0F, box.min_y}, {0.0F, box.max_y}, axis_color); // y-axis
	// The curve, polyline expanded into independent segments, on top.
	for (std::size_t idx = 1; idx < curve.size(); ++idx)
	{
		add_segment(out, curve.at(idx - 1U), curve.at(idx), curve_color);
	}
	return out;
}

/// Wire a single sin(x) curve and its coordinate axes into @p graph as a raster cache, producing
/// into @p scene_image.
inline void plot_sin(veng::graph::Graph& graph, veng::graph::TypedHandle<veng::rhi::Extent2D> screen,
					 veng::graph::DataHandle scene_image, veng::rhi::Format color_format, const geng::Bounds2D& bounds)
{
	using namespace veng;
	using namespace veng::graph;

	const auto curve_src = graph.add_source<std::vector<glm::vec2>>(sample_sin(bounds, SAMPLE_COUNT));

	// Reactive data box, projection and line-list (axes + curve), all derived from the curve.
	const auto box		 = graph.add_transform([](const std::vector<glm::vec2>& pts) { return fit_bounds(pts); }, curve_src);
	const auto view_proj = graph.add_transform([](const geng::Bounds2D& bnd) { return geng::ortho_view(bnd); }, box);
	const auto segments	 = graph.add_transform(
		 [](const std::vector<glm::vec2>& pts, const geng::Bounds2D& bnd) { return build_segments(pts, bnd); }, curve_src,
		 box);
	const auto positions  = graph.add_transform([](const Segments& seg) { return seg.positions; }, segments);
	const auto seg_colors = graph.add_transform([](const Segments& seg) { return seg.colors; }, segments);

	// Upload the two line-list buffers (positions + per-segment colours) the bake shader indexes.
	const DataHandle positions_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
	graph.set_producer(positions_ref,
					   graph.add(std::make_unique<nodes::StorageBufferNode>(positions, "positions", positions_ref)));
	const DataHandle colors_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
	graph.set_producer(colors_ref,
					   graph.add(std::make_unique<nodes::StorageBufferNode>(seg_colors, "colors", colors_ref)));

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

	// BAKE: render the axes + curve line-list into an offscreen RGBA8 cache, sized by bake_extent.
	const DataHandle baked_plot = graph.add(std::make_unique<ValueData<gpu::ImageRef>>(gpu::ImageRef{}));
	auto			 bake = std::make_unique<nodes::GraphicsNode>("line.vert", "line.frag", rhi::Format::RGBA8_UNORM,
													  rhi::Format::UNDEFINED, 6, bake_extent, baked_plot);
	bake->add_storage_buffer(positions_ref)
		.add_storage_buffer(colors_ref)
		.set_instances_from(colors_ref) // one instance per segment
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

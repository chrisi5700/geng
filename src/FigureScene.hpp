#ifndef GENG_FIGURE_SCENE_HPP
#define GENG_FIGURE_SCENE_HPP

// Internal scene-building DSL for geng::Figure: the GPU-facing value types, the axis/grid/label
// builders, and the reactive graph wiring. Kept in a header (not Figure.cpp) on purpose — it is
// glm-component heavy, and the project's clang-tidy flags glm union access in .cpp main files but
// tolerates it in headers (the same reason View is header-only). Figure.cpp stays glm-free and just
// orchestrates: build the device, drive the sources, pick a target.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <feng/FontAtlas.hpp>
#include <feng/Text.hpp>	  // feng::Glyph, feng::append_text, feng::TextStyle
#include <feng/TextGraph.hpp> // feng::add_text_layer
#include <format>
#include <geng/Bounds2D.hpp>
#include <geng/Series.hpp>
#include <geng/Theme.hpp>
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <iterator>
#include <memory>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>
#include <veng/gpu/BufferRef.hpp>
#include <veng/gpu/ImageRef.hpp>
#include <veng/nodes/GraphicsNode.hpp>
#include <veng/nodes/StorageBufferNode.hpp>
#include <veng/rendergraph/data/Data.hpp>
#include <veng/rendergraph/Graph.hpp>
#include <veng/rhi/Enums.hpp>

namespace geng::detail
{
constexpr std::uint32_t SUPERSAMPLE	   = 3;	   ///< Bake at this multiple of the target on each axis (SSAA).
constexpr float			FALLBACK_WIDTH = 2.0F; ///< Line width when the theme sets none.
constexpr float			MARGIN_FRAC	   = 0.08F;

/// Matches `PushData` in shaders/line.vert.slang (std430: mat4 @0, vec2 @64, float @72).
struct LineUniforms
{
	glm::mat4 view_proj;
	glm::vec2 extent;
	float	  half_width = 0.0F;

	friend bool operator==(const LineUniforms&, const LineUniforms&) noexcept = default;
};

/// A resolved series: a data-space polyline plus the concrete color to draw it in.
struct Curve
{
	std::vector<glm::vec2> points;
	glm::vec4			   color;

	friend bool operator==(const Curve&, const Curve&) = default;
};

/// A line-LIST: two endpoints per segment in @ref positions, one color per segment in @ref colors.
struct Segments
{
	std::vector<glm::vec2> positions;
	std::vector<glm::vec4> colors;

	friend bool operator==(const Segments&, const Segments&) = default;
};

/// One scatter point as the GPU sees it. The memory layout is std430 (matches `Marker` in
/// shaders/marker.vert.slang): @ref center @0, @ref size_px @8, @ref thickness_px @12, @ref color
/// @16, @ref shape @32, then 12 bytes of pad so the stride is a 48-byte multiple of 16. @ref size_px
/// and @ref thickness_px are already scaled to the *bake* resolution (× @ref SUPERSAMPLE).
struct alignas(16) MarkerInstance
{
	glm::vec2	  center;			   ///< Data-space position; the vertex shader projects it.
	float		  size_px	   = 0.0F; ///< Bounding size in bake pixels.
	float		  thickness_px = 0.0F; ///< Stroke width in bake pixels (hollow shapes only).
	glm::vec4	  color;			   ///< Straight-alpha RGBA.
	std::uint32_t shape = 0;		   ///< @ref geng::MarkerShape value (see shader `SHAPE_*`).
	std::uint32_t pad0	= 0;
	std::uint32_t pad1	= 0;
	std::uint32_t pad2	= 0;

	friend bool operator==(const MarkerInstance&, const MarkerInstance&) = default;
};
static_assert(sizeof(MarkerInstance) == 48, "MarkerInstance must match the std430 stride in marker.vert.slang");

/// Matches `PushData` in shaders/marker.vert.slang (std430: mat4 @0, vec2 @64).
struct MarkerUniforms
{
	glm::mat4 view_proj;
	glm::vec2 extent; ///< Bake framebuffer size in pixels.

	friend bool operator==(const MarkerUniforms&, const MarkerUniforms&) noexcept = default;
};

/// A resolved scatter series: data-space points plus the style needed to stamp a marker at each.
/// @ref colors is either empty (every point uses @ref color) or one entry per point (an explicit
/// per-point color from @ref Figure::set_point_colors — the path the Mandelbrot example drives).
struct Scatter
{
	std::vector<glm::vec2> points;
	std::vector<glm::vec4> colors;
	glm::vec4			   color; ///< Fallback when @ref colors is empty.
	MarkerShape			   shape		= MarkerShape::POINT;
	float				   size_px		= 0.0F; ///< On-screen size (bake-scaled later, in build_markers).
	float				   thickness_px = 0.0F;

	friend bool operator==(const Scatter&, const Scatter&) = default;
};

/// One bar as the GPU sees it: an axis-aligned rectangle in *data* space, `[x_center ± half_width] ×
/// [y0, y1]`. std430 layout (matches `Bar` in shaders/bar.vert.slang): four floats @0..12, then @ref
/// color @16 — a tight 32-byte stride. The bar vertex shader projects the four corners directly, so
/// (unlike a marker) nothing here is in pixels.
struct alignas(16) BarInstance
{
	float	  x_center	 = 0.0F;
	float	  half_width = 0.0F;
	float	  y0		 = 0.0F; ///< Foot of the bar (the baseline).
	float	  y1		 = 0.0F; ///< Head of the bar (the value).
	glm::vec4 color;			 ///< Straight-alpha RGBA.

	friend bool operator==(const BarInstance&, const BarInstance&) = default;
};
static_assert(sizeof(BarInstance) == 32, "BarInstance must match the std430 stride in bar.vert.slang");

/// A resolved bar series: data-space points `(x, value)`, the bar geometry, and the colors. @ref colors
/// is either empty (every bar uses @ref color) or one entry per bar (a per-point override).
struct Bars
{
	std::vector<glm::vec2> points;
	std::vector<glm::vec4> colors;
	glm::vec4			   color; ///< Fallback when @ref colors is empty.
	float				   width	= 0.0F;
	float				   baseline = 0.0F;

	friend bool operator==(const Bars&, const Bars&) = default;
};

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

inline std::vector<float> axis_ticks(float low, float high, float step)
{
	std::vector<float> ticks;
	if (step <= 0.0F)
	{
		return ticks;
	}
	const float first = std::ceil(low / step) * step;
	for (std::size_t idx = 0; idx < 1000U; ++idx)
	{
		const float val = first + (static_cast<float>(idx) * step);
		if (val > high)
		{
			break;
		}
		ticks.push_back(val);
	}
	return ticks;
}

struct GridTicks
{
	std::vector<float> xs;
	std::vector<float> ys;
};

inline GridTicks grid_ticks(const Bounds2D& box, int target)
{
	return GridTicks{.xs = axis_ticks(box.min_x, box.max_x, nice_step(box.max_x - box.min_x, target)),
					 .ys = axis_ticks(box.min_y, box.max_y, nice_step(box.max_y - box.min_y, target))};
}

inline void add_segment(Segments& out, glm::vec2 start, glm::vec2 end, const glm::vec4& color)
{
	out.positions.push_back(start);
	out.positions.push_back(end);
	out.colors.push_back(color);
}

/// Build the whole line-LIST back to front: faint grid, then the brighter axes, then each curve
/// expanded segment-by-segment on top (later curves win, since there is no depth/blend).
inline Segments build_segments(const std::vector<Curve>& curves, const Theme& theme, const Bounds2D& box)
{
	Segments		out;
	const GridTicks ticks = grid_ticks(box, theme.grid.target_divisions);
	if (theme.grid.visible)
	{
		for (const float tick_x : ticks.xs)
		{
			if (std::abs(tick_x) >= 1e-4F)
			{
				add_segment(out, {tick_x, box.min_y}, {tick_x, box.max_y}, theme.grid.color);
			}
		}
		for (const float tick_y : ticks.ys)
		{
			if (std::abs(tick_y) >= 1e-4F)
			{
				add_segment(out, {box.min_x, tick_y}, {box.max_x, tick_y}, theme.grid.color);
			}
		}
	}
	if (theme.axes.visible)
	{
		add_segment(out, {box.min_x, 0.0F}, {box.max_x, 0.0F}, theme.axes.color);
		add_segment(out, {0.0F, box.min_y}, {0.0F, box.max_y}, theme.axes.color);
	}
	for (const Curve& curve : curves)
	{
		for (std::size_t idx = 1; idx < curve.points.size(); ++idx)
		{
			add_segment(out, curve.points.at(idx - 1U), curve.points.at(idx), curve.color);
		}
	}
	return out;
}

/// Lay out numeric tick labels at the grid positions, anchored in data space and sized in pixels so
/// they track the plot but stay a constant on-screen size. The per-string glyph layout is delegated
/// to feng::append_text; geng only decides which labels go where and how each axis aligns them.
inline std::vector<feng::Glyph> build_glyphs(const Theme& theme, const Bounds2D& box, const feng::FontAtlas& font)
{
	std::vector<feng::Glyph> out;
	if (!theme.labels.visible)
	{
		return out;
	}
	const float		scale = (theme.labels.pixel_height * static_cast<float>(SUPERSAMPLE)) / font.pixel_height();
	const float		gap	  = theme.labels.pixel_height * static_cast<float>(SUPERSAMPLE) * 0.35F;
	const GridTicks ticks = grid_ticks(box, theme.grid.target_divisions);

	for (const float tick_x : ticks.xs)
	{
		if (std::abs(tick_x) >= 1e-4F)
		{
			// x labels sit centred just below their tick on the x-axis.
			feng::append_text(out, font, std::format("{:g}", tick_x), glm::vec2{tick_x, 0.0F},
							  feng::TextStyle{.color  = theme.labels.color,
											  .scale  = scale,
											  .halign = feng::HAlign::CENTER,
											  .valign = feng::VAlign::TOP,
											  .gap	  = gap});
		}
	}
	for (const float tick_y : ticks.ys)
	{
		if (std::abs(tick_y) >= 1e-4F)
		{
			// y labels sit just left of their tick on the y-axis, vertically centred.
			feng::append_text(out, font, std::format("{:g}", tick_y), glm::vec2{0.0F, tick_y},
							  feng::TextStyle{.color  = theme.labels.color,
											  .scale  = scale,
											  .halign = feng::HAlign::RIGHT,
											  .valign = feng::VAlign::MIDDLE,
											  .gap	  = gap});
		}
	}
	return out;
}

inline glm::vec4 resolve_color(const LineStyle& style, std::uint64_t order, const Theme& theme)
{
	if (style.color.has_value())
	{
		return *style.color;
	}
	if (!theme.palette.empty())
	{
		return theme.palette[order % theme.palette.size()];
	}
	if (theme.line_defaults.color.has_value())
	{
		return *theme.line_defaults.color;
	}
	return glm::vec4{0.30F, 0.80F, 1.00F, 1.0F};
}

/// The per-series fallback color for a scatter marker: same resolution order as @ref resolve_color
/// (explicit, then palette by creation order, then the theme default). Per-point colors, when set,
/// override this in `Figure::sync_scene`.
inline glm::vec4 resolve_marker_color(const MarkerStyle& style, std::uint64_t order, const Theme& theme)
{
	if (style.color.has_value())
	{
		return *style.color;
	}
	if (!theme.palette.empty())
	{
		return theme.palette[order % theme.palette.size()];
	}
	if (theme.line_defaults.color.has_value())
	{
		return *theme.line_defaults.color;
	}
	return glm::vec4{0.30F, 0.80F, 1.00F, 1.0F};
}

/// Flatten the resolved scatter series into one instance buffer for a single batched draw — every
/// marker across every scatter series becomes one @ref MarkerInstance. Sizes are scaled to the bake
/// resolution here (× @ref SUPERSAMPLE) so the on-screen px the caller asked for survives the SSAA
/// downsample. A point takes its own color when the series carries a per-point list, else the
/// series' resolved fallback color.
inline std::vector<MarkerInstance> build_markers(const std::vector<Scatter>& scatters)
{
	std::vector<MarkerInstance> out;
	const std::size_t			total =
		std::accumulate(scatters.begin(), scatters.end(), std::size_t{0},
						[](std::size_t acc, const Scatter& scatter) { return acc + scatter.points.size(); });
	out.reserve(total);

	for (const Scatter& scatter : scatters)
	{
		const float			size	  = scatter.size_px * static_cast<float>(SUPERSAMPLE);
		const float			thickness = scatter.thickness_px * static_cast<float>(SUPERSAMPLE);
		const std::uint32_t shape	  = static_cast<std::uint32_t>(scatter.shape);
		const bool			per_point = scatter.colors.size() == scatter.points.size();
		for (std::size_t idx = 0; idx < scatter.points.size(); ++idx)
		{
			out.push_back(MarkerInstance{.center	   = scatter.points[idx],
										 .size_px	   = size,
										 .thickness_px = thickness,
										 .color		   = per_point ? scatter.colors[idx] : scatter.color,
										 .shape		   = shape});
		}
	}
	return out;
}

/// The per-series fallback color for a bar: same resolution order as @ref resolve_color (explicit,
/// then palette by creation order, then the theme default). Per-bar colors override this in sync_scene.
inline glm::vec4 resolve_bar_color(const BarStyle& style, std::uint64_t order, const Theme& theme)
{
	if (style.color.has_value())
	{
		return *style.color;
	}
	if (!theme.palette.empty())
	{
		return theme.palette[order % theme.palette.size()];
	}
	if (theme.line_defaults.color.has_value())
	{
		return *theme.line_defaults.color;
	}
	return glm::vec4{0.30F, 0.80F, 1.00F, 1.0F};
}

/// Flatten the resolved bar series into one instance buffer for a single batched draw — every bar
/// across every bar series becomes one @ref BarInstance (a data-space rectangle). A bar takes its own
/// color when the series carries a per-point list, else the series' resolved fallback color.
inline std::vector<BarInstance> build_bars(const std::vector<Bars>& series)
{
	std::vector<BarInstance> out;
	const std::size_t		 total =
		std::accumulate(series.begin(), series.end(), std::size_t{0},
						[](std::size_t acc, const Bars& bars) { return acc + bars.points.size(); });
	out.reserve(total);

	for (const Bars& bars : series)
	{
		const float half	  = bars.width * 0.5F;
		const bool	per_point = bars.colors.size() == bars.points.size();
		for (std::size_t idx = 0; idx < bars.points.size(); ++idx)
		{
			out.push_back(BarInstance{.x_center	  = bars.points[idx].x,
									  .half_width = half,
									  .y0		  = bars.baseline,
									  .y1		  = bars.points[idx].y,
									  .color	  = per_point ? bars.colors[idx] : bars.color});
		}
	}
	return out;
}

/// The corner points a bar series contributes to the figure's data bounds: each bar widens the
/// x-extent by its half-width and pins the y-extent to include both the baseline (the foot) and its
/// value. Returned as owned points so @ref Figure::data_bounds can fold them through @ref bounds_of
/// like any other series — a bar chart whose bars stop short of y=0 still frames the baseline.
inline std::vector<glm::vec2> bar_extent_points(const std::vector<glm::vec2>& bars, float width, float baseline)
{
	std::vector<glm::vec2> out;
	out.reserve(bars.size() * 2);
	const float half = width * 0.5F;
	for (const glm::vec2& bar : bars)
	{
		out.emplace_back(bar.x - half, baseline);
		out.emplace_back(bar.x + half, bar.y);
	}
	return out;
}

/// Axis-aligned bounds of every span of points, padded by a small margin (a unit box if empty).
inline Bounds2D bounds_of(std::span<const std::span<const glm::vec2>> series_points)
{
	bool  any	= false;
	float min_x = 0.0F;
	float max_x = 0.0F;
	float min_y = 0.0F;
	float max_y = 0.0F;
	for (const std::span<const glm::vec2> points : series_points)
	{
		for (const glm::vec2& point : points)
		{
			if (!any)
			{
				min_x = max_x = point.x;
				min_y = max_y = point.y;
				any			  = true;
				continue;
			}
			min_x = std::min(min_x, point.x);
			max_x = std::max(max_x, point.x);
			min_y = std::min(min_y, point.y);
			max_y = std::max(max_y, point.y);
		}
	}
	if (!any)
	{
		return Bounds2D{.min_x = -1.0F, .max_x = 1.0F, .min_y = -1.0F, .max_y = 1.0F};
	}
	const float pad_x = std::max(max_x - min_x, 1e-6F) * MARGIN_FRAC;
	const float pad_y = std::max(max_y - min_y, 1e-6F) * MARGIN_FRAC;
	return Bounds2D{.min_x = min_x - pad_x, .max_x = max_x + pad_x, .min_y = min_y - pad_y, .max_y = max_y + pad_y};
}

/// Sliding-window bounds for Fit::FOLLOW_LATEST: a window of x-width @p width anchored to the latest x
/// (@p full's max_x), with y fit and margin-padded to just the points inside that window. Falls back to
/// @p full's y when the window holds no points; @p width <= 0 uses the full x-extent.
inline Bounds2D follow_bounds(std::span<const std::span<const glm::vec2>> series_points, float width,
							  const Bounds2D& full)
{
	const float max_x  = full.max_x;
	const float window = width > 0.0F ? width : (full.max_x - full.min_x);
	const float min_x  = max_x - window;

	bool  any	= false;
	float min_y = 0.0F;
	float max_y = 0.0F;
	for (const std::span<const glm::vec2> points : series_points)
	{
		for (const glm::vec2& point : points)
		{
			if (point.x < min_x || point.x > max_x)
			{
				continue;
			}
			if (!any)
			{
				min_y = max_y = point.y;
				any			  = true;
				continue;
			}
			min_y = std::min(min_y, point.y);
			max_y = std::max(max_y, point.y);
		}
	}
	if (!any)
	{
		return Bounds2D{.min_x = min_x, .max_x = max_x, .min_y = full.min_y, .max_y = full.max_y};
	}
	const float pad_y = std::max(max_y - min_y, 1e-6F) * MARGIN_FRAC;
	return Bounds2D{.min_x = min_x, .max_x = max_x, .min_y = min_y - pad_y, .max_y = max_y + pad_y};
}

/// Wire the bake + text + composite pipeline into @p graph from the four sources, returning the
/// scene-image handle the composite produces.
inline veng::graph::DataHandle wire_scene(
	veng::graph::Graph& graph, veng::graph::TypedHandle<veng::rhi::Extent2D> screen,
	veng::graph::TypedHandle<Bounds2D> view, veng::graph::TypedHandle<Theme> theme,
	veng::graph::TypedHandle<std::vector<Curve>> curves, veng::graph::TypedHandle<std::vector<MarkerInstance>> markers,
	veng::graph::TypedHandle<std::vector<BarInstance>> bars, const feng::FontAtlas& font, const Theme& initial)
{
	using namespace veng;
	using namespace veng::graph;

	const auto view_proj  = graph.add_transform([](const Bounds2D& bnd) { return ortho_view(bnd); }, view);
	const auto segments	  = graph.add_transform([](const std::vector<Curve>& crv, const Theme& thm, const Bounds2D& bnd)
												{ return build_segments(crv, thm, bnd); }, curves, theme, view);
	const auto positions  = graph.add_transform([](const Segments& seg) { return seg.positions; }, segments);
	const auto seg_colors = graph.add_transform([](const Segments& seg) { return seg.colors; }, segments);

	const DataHandle positions_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
	graph.set_producer(positions_ref,
					   graph.add(std::make_unique<nodes::StorageBufferNode>(positions, "positions", positions_ref)));
	const DataHandle colors_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
	graph.set_producer(colors_ref,
					   graph.add(std::make_unique<nodes::StorageBufferNode>(seg_colors, "colors", colors_ref)));

	const auto bake_extent = graph.add_transform(
		[](const rhi::Extent2D& size)
		{ return rhi::Extent2D{.width = size.width * SUPERSAMPLE, .height = size.height * SUPERSAMPLE}; }, screen);

	const auto bake_uniforms = graph.add_transform(
		[](const glm::mat4& proj, const rhi::Extent2D& size, const Theme& thm)
		{
			const float width = thm.line_defaults.width_px > 0.0F ? thm.line_defaults.width_px : FALLBACK_WIDTH;
			return LineUniforms{.view_proj = proj,
								.extent	   = glm::vec2(static_cast<float>(size.width), static_cast<float>(size.height)),
								.half_width = width * 0.5F * static_cast<float>(SUPERSAMPLE)};
		},
		view_proj, bake_extent, theme);

	const DataHandle baked_plot = graph.add(std::make_unique<ValueData<gpu::ImageRef>>(gpu::ImageRef{}));
	auto			 bake = std::make_unique<nodes::GraphicsNode>("line.vert", "line.frag", rhi::Format::RGBA8_UNORM,
																  rhi::Format::UNDEFINED, 6, bake_extent, baked_plot);
	bake->add_storage_buffer(positions_ref)
		.add_storage_buffer(colors_ref)
		.set_instances_from(colors_ref)
		.push_constant<LineUniforms>(bake_uniforms, rhi::ShaderStage::VERTEX)
		.clear_color({initial.background.r, initial.background.g, initial.background.b, initial.background.a});
	graph.set_producer(baked_plot, graph.add(std::move(bake)));

	// Scatter markers: one batched instanced draw of every point across every scatter series
	// (build_markers flattened them into a single buffer in sync_scene). Each instance is an
	// SDF-shaded quad sized in pixels; the layer clears transparent and blends, so it composites over
	// the line/grid plot but under the tick labels. The draw emits zero instances (a cleared, no-op
	// layer) when there are no scatter series — the marker node is always wired in.
	const DataHandle markers_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
	graph.set_producer(markers_ref,
					   graph.add(std::make_unique<nodes::StorageBufferNode>(markers, "markers", markers_ref)));

	const auto marker_uniforms = graph.add_transform(
		[](const glm::mat4& proj, const rhi::Extent2D& size)
		{
			return MarkerUniforms{.view_proj = proj,
								  .extent = glm::vec2(static_cast<float>(size.width), static_cast<float>(size.height))};
		},
		view_proj, bake_extent);

	const DataHandle marker_cache = graph.add(std::make_unique<ValueData<gpu::ImageRef>>(gpu::ImageRef{}));
	auto marker_node = std::make_unique<nodes::GraphicsNode>("marker.vert", "marker.frag", rhi::Format::RGBA8_UNORM,
															 rhi::Format::UNDEFINED, 6, bake_extent, marker_cache);
	marker_node->add_storage_buffer(markers_ref)
		.set_instances_from(markers_ref)
		.push_constant<MarkerUniforms>(marker_uniforms, rhi::ShaderStage::VERTEX)
		.clear_color({0.0F, 0.0F, 0.0F, 0.0F})
		.blend(true);
	graph.set_producer(marker_cache, graph.add(std::move(marker_node)));

	// Bar charts: one batched instanced draw of every bar (axis-aligned rectangles in data space, so
	// just the view_proj is pushed — no pixel sizing). Like the markers it clears transparent and
	// blends, compositing over the plot but under markers and labels; zero bars => a cleared no-op layer.
	const DataHandle bars_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
	graph.set_producer(bars_ref, graph.add(std::make_unique<nodes::StorageBufferNode>(bars, "bars", bars_ref)));

	const DataHandle bar_cache = graph.add(std::make_unique<ValueData<gpu::ImageRef>>(gpu::ImageRef{}));
	auto bar_node = std::make_unique<nodes::GraphicsNode>("bar.vert", "bar.frag", rhi::Format::RGBA8_UNORM,
														  rhi::Format::UNDEFINED, 6, bake_extent, bar_cache);
	bar_node->add_storage_buffer(bars_ref)
		.set_instances_from(bars_ref)
		.push_constant<glm::mat4>(view_proj, rhi::ShaderStage::VERTEX)
		.clear_color({0.0F, 0.0F, 0.0F, 0.0F})
		.blend(true);
	graph.set_producer(bar_cache, graph.add(std::move(bar_node)));

	// Tick-label text: geng lays the glyph instances out (build_glyphs) and feng wires the subgraph
	// that uploads them, samples the atlas, and rasterises a transparent text layer at the bake size.
	const auto		 glyph_data = graph.add_transform([&font](const Theme& thm, const Bounds2D& bnd)
													  { return build_glyphs(thm, bnd, font); }, theme, view);
	const DataHandle text_cache = feng::add_text_layer(graph, glyph_data, font.atlas_ref(), view_proj, bake_extent);

	// Composite the supersampled layers back to front and resolve onto the screen: opaque plot, then the
	// bar layer, the marker layer, and the labels (the display sampler downsamples each cache for SSAA).
	const DataHandle scene_image = graph.add(std::make_unique<ValueData<gpu::ImageRef>>(gpu::ImageRef{}));
	auto display = std::make_unique<nodes::GraphicsNode>("fullscreen.vert", "display.frag", rhi::Format::RGBA8_UNORM,
														 rhi::Format::UNDEFINED, 3, screen, scene_image);
	display->add_sampled_image(baked_plot, "plot")
		.add_sampled_image(bar_cache, "bars")
		.add_sampled_image(marker_cache, "markers")
		.add_sampled_image(text_cache, "text");
	graph.set_producer(scene_image, graph.add(std::move(display)));
	return scene_image;
}
} // namespace geng::detail

#endif // GENG_FIGURE_SCENE_HPP

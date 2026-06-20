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
#include <format>
#include <geng/Bounds2D.hpp>
#include <geng/FontAtlas.hpp>
#include <geng/Series.hpp>
#include <geng/Theme.hpp>
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <iterator>
#include <memory>
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

/// One glyph instance for shaders/text.vert.slang (std430, float4 forces a 16-byte stride).
struct alignas(16) Glyph
{
	glm::vec4 color;
	glm::vec2 anchor;
	glm::vec2 min_px;
	glm::vec2 max_px;
	glm::vec2 uv0;
	glm::vec2 uv1;

	friend bool operator==(const Glyph&, const Glyph&) = default;
};
static_assert(sizeof(Glyph) == 64, "Glyph must match the std430 StructuredBuffer stride");

/// Matches `PushData` in shaders/text.vert.slang (std430: mat4 @0, vec2 @64).
struct TextPush
{
	glm::mat4 view_proj;
	glm::vec2 extent;

	friend bool operator==(const TextPush&, const TextPush&) noexcept = default;
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
/// they track the plot but stay a constant on-screen size.
inline std::vector<Glyph> build_glyphs(const Theme& theme, const Bounds2D& box, const FontAtlas& font)
{
	std::vector<Glyph> out;
	if (!theme.labels.visible)
	{
		return out;
	}
	const float		scale  = (theme.labels.pixel_height * static_cast<float>(SUPERSAMPLE)) / font.pixel_height();
	const float		margin = theme.labels.pixel_height * static_cast<float>(SUPERSAMPLE) * 0.35F;
	const GridTicks ticks  = grid_ticks(box, theme.grid.target_divisions);

	const auto emit_label = [&](std::string_view text, glm::vec2 anchor, bool below)
	{
		const std::vector<GlyphQuad> quads = font.layout(text);
		glm::vec2					 low{1e9F, 1e9F};
		glm::vec2					 high{-1e9F, -1e9F};
		for (const GlyphQuad& quad : quads)
		{
			low	 = glm::min(low, quad.min_px * scale);
			high = glm::max(high, quad.max_px * scale);
		}
		const glm::vec2 shift = below ? glm::vec2{-(low.x + high.x) * 0.5F, margin - low.y}
									  : glm::vec2{-high.x - margin, -(low.y + high.y) * 0.5F};
		std::ranges::transform(quads, std::back_inserter(out),
							   [&](const GlyphQuad& quad)
							   {
								   return Glyph{.color	= theme.labels.color,
												.anchor = anchor,
												.min_px = (quad.min_px * scale) + shift,
												.max_px = (quad.max_px * scale) + shift,
												.uv0	= quad.uv0,
												.uv1	= quad.uv1};
							   });
	};

	for (const float tick_x : ticks.xs)
	{
		if (std::abs(tick_x) >= 1e-4F)
		{
			emit_label(std::format("{:g}", tick_x), glm::vec2{tick_x, 0.0F}, true);
		}
	}
	for (const float tick_y : ticks.ys)
	{
		if (std::abs(tick_y) >= 1e-4F)
		{
			emit_label(std::format("{:g}", tick_y), glm::vec2{0.0F, tick_y}, false);
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
inline veng::graph::DataHandle wire_scene(veng::graph::Graph&							graph,
										  veng::graph::TypedHandle<veng::rhi::Extent2D> screen,
										  veng::graph::TypedHandle<Bounds2D>			view,
										  veng::graph::TypedHandle<Theme>				theme,
										  veng::graph::TypedHandle<std::vector<Curve>> curves, const FontAtlas& font,
										  const Theme& initial)
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

	const auto		 glyph_data = graph.add_transform([&font](const Theme& thm, const Bounds2D& bnd)
													  { return build_glyphs(thm, bnd, font); }, theme, view);
	const DataHandle glyphs_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
	graph.set_producer(glyphs_ref,
					   graph.add(std::make_unique<nodes::StorageBufferNode>(glyph_data, "glyphs", glyphs_ref)));
	const auto atlas_src = graph.add_source<gpu::ImageRef>(font.atlas_ref());
	const auto text_push = graph.add_transform(
		[](const glm::mat4& proj, const rhi::Extent2D& size)
		{
			return TextPush{.view_proj = proj,
							.extent	   = glm::vec2(static_cast<float>(size.width), static_cast<float>(size.height))};
		},
		view_proj, bake_extent);

	const DataHandle text_cache = graph.add(std::make_unique<ValueData<gpu::ImageRef>>(gpu::ImageRef{}));
	auto text_node = std::make_unique<nodes::GraphicsNode>("text.vert", "text.frag", rhi::Format::RGBA8_UNORM,
														   rhi::Format::UNDEFINED, 6, bake_extent, text_cache);
	text_node->add_storage_buffer(glyphs_ref)
		.add_sampled_image(atlas_src, "atlas")
		.set_instances_from(glyphs_ref)
		.push_constant<TextPush>(text_push, rhi::ShaderStage::VERTEX)
		.clear_color({0.0F, 0.0F, 0.0F, 0.0F})
		.blend(true);
	graph.set_producer(text_cache, graph.add(std::move(text_node)));

	const DataHandle scene_image = graph.add(std::make_unique<ValueData<gpu::ImageRef>>(gpu::ImageRef{}));
	auto display = std::make_unique<nodes::GraphicsNode>("fullscreen.vert", "display.frag", rhi::Format::RGBA8_UNORM,
														 rhi::Format::UNDEFINED, 3, screen, scene_image);
	display->add_sampled_image(baked_plot, "plot").add_sampled_image(text_cache, "text");
	graph.set_producer(scene_image, graph.add(std::move(display)));
	return scene_image;
}
} // namespace geng::detail

#endif // GENG_FIGURE_SCENE_HPP

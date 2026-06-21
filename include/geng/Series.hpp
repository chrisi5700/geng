#ifndef GENG_SERIES_HPP
#define GENG_SERIES_HPP

#include <cstdint>
#include <glm/vec4.hpp>
#include <optional>

namespace geng
{
/// Opaque, stable identity for a series in a @ref Figure. Values are never recycled, so a handle to
/// a removed series stays permanently unmatched (it never aliases a later series). The internal
/// value is not part of the contract — compare it and check validity, nothing else.
class SeriesId
{
	 public:
	SeriesId() = default; ///< The invalid id.

	[[nodiscard]] bool valid() const noexcept { return m_value != 0; }
	friend bool		   operator==(const SeriesId&, const SeriesId&) noexcept = default;

	 private:
	friend class Figure;
	explicit SeriesId(std::uint64_t value) noexcept
		: m_value(value)
	{
	}
	[[nodiscard]] std::uint64_t value() const noexcept { return m_value; }

	std::uint64_t m_value = 0;
};

/// Stroke pattern for a line series. `SOLID` needs no new shaders; the dashed variants add a
/// dash-distance varying to the line shader (a later phase).
enum class Dash : std::uint8_t
{
	SOLID,
	DASH,
	DOT,
	DASH_DOT
};

/// Per-series visual style. An empty @ref color resolves from the theme palette (by creation order),
/// then the theme's line default — so a caller can name a color, let the theme pick one, or ignore
/// it entirely.
struct LineStyle
{
	std::optional<glm::vec4> color;
	float					 width_px = 2.0F; ///< On-screen stroke width in pixels (resize-stable).
	Dash					 dash	  = Dash::SOLID;
	bool					 visible  = true;

	friend bool operator==(const LineStyle&, const LineStyle&) = default;
};

/// The symbol drawn at each point of a scatter series. Every shape is rasterised from a signed
/// distance field in `shaders/marker.frag.slang`, so the constant values here are the contract with
/// that shader — keep them in sync (the order matches `SHAPE_*` there).
enum class MarkerShape : std::uint8_t
{
	POINT,	 ///< Filled disc.
	CIRCLE,	 ///< Hollow ring (outline only; @ref MarkerStyle::thickness_px wide).
	SQUARE,	 ///< Filled axis-aligned square.
	DIAMOND, ///< Filled square rotated 45°.
	CROSS,	 ///< Diagonal ✕ strokes (@ref MarkerStyle::thickness_px wide).
	PLUS,	 ///< Upright ✚ strokes (@ref MarkerStyle::thickness_px wide).
	TRIANGLE ///< Filled upward equilateral triangle.
};

/// Per-series style for a scatter plot (one @ref MarkerShape stamped at every data point). An empty
/// @ref color resolves the same way as @ref LineStyle (palette by creation order, then the default);
/// it is the fallback when a point has no explicit color from @ref Figure::set_point_colors.
struct MarkerStyle
{
	std::optional<glm::vec4> color;
	MarkerShape				 shape	 = MarkerShape::POINT;
	float					 size_px = 8.0F; ///< On-screen marker bounding size in pixels (resize-stable).
	float thickness_px				 = 1.5F; ///< Stroke width in pixels for the hollow shapes (CIRCLE / CROSS / PLUS).
	bool  visible					 = true;

	friend bool operator==(const MarkerStyle&, const MarkerStyle&) = default;
};
} // namespace geng

#endif // GENG_SERIES_HPP

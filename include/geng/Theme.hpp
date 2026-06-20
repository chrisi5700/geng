#ifndef GENG_THEME_HPP
#define GENG_THEME_HPP

#include <cstdint>
#include <geng/Series.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace geng
{
/// How the view tracks data over time. Interactive pan/zoom is suspended while any mode other than
/// `OFF` is active.
enum class Fit : std::uint8_t
{
	OFF,		  ///< View is driven only by the user / programmatic focus().
	ALL,		  ///< Re-fit to the bounds of all series whenever data changes.
	FOLLOW_LATEST ///< Keep a fixed-width window anchored to the most recent data.
};

/// The grid lines behind the plot. `target_divisions` feeds the "nice" tick-step solver per axis.
struct GridStyle
{
	bool	  visible = true;
	glm::vec4 color{0.16F, 0.17F, 0.22F, 1.0F};
	int		  target_divisions = 6;

	friend bool operator==(const GridStyle&, const GridStyle&) = default;
};

/// The brighter axes through the origin.
struct AxisStyle
{
	bool	  visible = true;
	glm::vec4 color{0.40F, 0.42F, 0.50F, 1.0F};

	friend bool operator==(const AxisStyle&, const AxisStyle&) = default;
};

/// The numeric tick labels. The font itself comes from the @ref Figure (a GPU resource); this is
/// just how the labels are drawn.
struct LabelStyle
{
	bool	  visible = true;
	glm::vec4 color{0.65F, 0.67F, 0.72F, 1.0F};
	float	  pixel_height = 18.0F;

	friend bool operator==(const LabelStyle&, const LabelStyle&) = default;
};

/// The overall look of a figure. @ref palette is the auto-cycled color source for series that do not
/// set an explicit @ref LineStyle::color.
struct Theme
{
	glm::vec4			   background{0.06F, 0.06F, 0.09F, 1.0F};
	GridStyle			   grid;
	AxisStyle			   axes;
	LabelStyle			   labels;
	std::vector<glm::vec4> palette;		  ///< Series colors, cycled by creation order.
	LineStyle			   line_defaults; ///< Applied under each series' own LineStyle.

	[[nodiscard]] static Theme dark();
	[[nodiscard]] static Theme light();

	friend bool operator==(const Theme&, const Theme&) = default;
};
} // namespace geng

#endif // GENG_THEME_HPP

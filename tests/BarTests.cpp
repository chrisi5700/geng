// Device-free tests for the bar renderer's CPU side. Like the scatter suite, the geng::Figure methods
// need a Vulkan device and stay out of ctest; the header-only functions that turn resolved bar series
// into the GPU instance buffer — geng::detail::{build_bars, bar_extent_points, resolve_bar_color} — are
// tested directly. Whole glm objects are compared (never .x/.y/.r) and indexing uses .at(), so this
// main TU keeps clang-tidy's union-access and unchecked-container-access rules happy.

#include <catch2/catch_test_macros.hpp>
#include <FigureScene.hpp> // geng::detail::{Bars, BarInstance, build_bars, bar_extent_points, resolve_bar_color}
#include <geng/Series.hpp>
#include <geng/Theme.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <utility>
#include <vector>

namespace
{
geng::detail::Bars make_bars(std::vector<glm::vec2> points, std::vector<glm::vec4> colors, glm::vec4 fallback,
							 float width, float baseline)
{
	geng::detail::Bars bars;
	bars.points	  = std::move(points);
	bars.colors	  = std::move(colors);
	bars.color	  = fallback;
	bars.width	  = width;
	bars.baseline = baseline;
	return bars;
}
} // namespace

TEST_CASE("BarInstance matches the std430 GPU stride", "[bar][layout]")
{
	REQUIRE(sizeof(geng::detail::BarInstance) == 32);
}

TEST_CASE("build_bars turns each point into a baseline-to-value rectangle", "[bar][bars]")
{
	SECTION("no series yields no instances")
	{
		REQUIRE(geng::detail::build_bars({}).empty());
	}

	SECTION("geometry: x_center from the point, half the width, baseline foot, value head")
	{
		const geng::detail::Bars bars = make_bars({glm::vec2{3.0F, 0.5F}, glm::vec2{4.0F, 0.2F}}, {},
												  glm::vec4{0.1F, 0.2F, 0.3F, 1.0F}, 0.8F, 0.1F);

		const std::vector<geng::detail::BarInstance> out = geng::detail::build_bars({bars});
		REQUIRE(out.size() == 2);
		REQUIRE(out.at(0).x_center == 3.0F);
		REQUIRE(out.at(0).half_width == 0.4F); // width / 2
		REQUIRE(out.at(0).y0 == 0.1F);		   // baseline
		REQUIRE(out.at(0).y1 == 0.5F);		   // value
		REQUIRE(out.at(1).x_center == 4.0F);
		REQUIRE(out.at(1).y1 == 0.2F);
		REQUIRE(out.at(0).color == glm::vec4{0.1F, 0.2F, 0.3F, 1.0F}); // fallback color (no per-bar list)
	}

	SECTION("per-bar colors override the series color when the count matches")
	{
		const geng::detail::Bars bars =
			make_bars({glm::vec2{0.0F, 1.0F}, glm::vec2{1.0F, 1.0F}},
					  {glm::vec4{1.0F, 0.0F, 0.0F, 1.0F}, glm::vec4{0.0F, 1.0F, 0.0F, 1.0F}},
					  glm::vec4{0.5F, 0.5F, 0.5F, 1.0F}, 0.8F, 0.0F);

		const std::vector<geng::detail::BarInstance> out = geng::detail::build_bars({bars});
		REQUIRE(out.size() == 2);
		REQUIRE(out.at(0).color == glm::vec4{1.0F, 0.0F, 0.0F, 1.0F});
		REQUIRE(out.at(1).color == glm::vec4{0.0F, 1.0F, 0.0F, 1.0F});
	}

	SECTION("a per-bar color count that does not match falls back to the series color")
	{
		const geng::detail::Bars bars =
			make_bars({glm::vec2{0.0F, 1.0F}, glm::vec2{1.0F, 1.0F}}, {glm::vec4{1.0F, 0.0F, 0.0F, 1.0F}},
					  glm::vec4{0.5F, 0.5F, 0.5F, 1.0F}, 0.8F, 0.0F);

		const std::vector<geng::detail::BarInstance> out = geng::detail::build_bars({bars});
		REQUIRE(out.size() == 2);
		REQUIRE(out.at(0).color == glm::vec4{0.5F, 0.5F, 0.5F, 1.0F});
		REQUIRE(out.at(1).color == glm::vec4{0.5F, 0.5F, 0.5F, 1.0F});
	}
}

TEST_CASE("bar_extent_points frames the bar rectangles and the baseline", "[bar][bounds]")
{
	// Two bars, width 0.8 (half 0.4), baseline 0: each contributes (x-0.4, 0) and (x+0.4, value).
	const std::vector<glm::vec2> corners =
		geng::detail::bar_extent_points({glm::vec2{1.0F, 0.5F}, glm::vec2{2.0F, 0.8F}}, 0.8F, 0.0F);
	REQUIRE(corners.size() == 4);
	REQUIRE(corners.at(0) == glm::vec2{0.6F, 0.0F}); // left foot of bar 1 (on the baseline)
	REQUIRE(corners.at(1) == glm::vec2{1.4F, 0.5F}); // right head of bar 1 (at the value)
	REQUIRE(corners.at(2) == glm::vec2{1.6F, 0.0F});
	REQUIRE(corners.at(3) == glm::vec2{2.4F, 0.8F});
}

TEST_CASE("resolve_bar_color follows explicit → palette → default", "[bar][color]")
{
	geng::Theme theme;
	theme.palette = {glm::vec4{1.0F, 0.0F, 0.0F, 1.0F}, glm::vec4{0.0F, 1.0F, 0.0F, 1.0F}};

	SECTION("an explicit color wins")
	{
		geng::BarStyle style;
		style.color = glm::vec4{0.2F, 0.4F, 0.6F, 1.0F};
		REQUIRE(geng::detail::resolve_bar_color(style, 0, theme) == glm::vec4{0.2F, 0.4F, 0.6F, 1.0F});
	}

	SECTION("the palette cycles by creation order when no color is set")
	{
		const geng::BarStyle style;
		REQUIRE(geng::detail::resolve_bar_color(style, 0, theme) == theme.palette.at(0));
		REQUIRE(geng::detail::resolve_bar_color(style, 1, theme) == theme.palette.at(1));
		REQUIRE(geng::detail::resolve_bar_color(style, 2, theme) == theme.palette.at(0)); // wraps
	}

	SECTION("an empty palette falls back to the built-in default")
	{
		geng::Theme bare;
		bare.palette			 = {};
		bare.line_defaults.color = {};
		const geng::BarStyle style;
		REQUIRE(geng::detail::resolve_bar_color(style, 0, bare) == glm::vec4{0.30F, 0.80F, 1.00F, 1.0F});
	}
}

// Device-free tests for the scatter renderer's CPU side. The geng::Figure methods themselves need a
// Vulkan device (so they stay out of this suite, like the rest of geng's ctest), but the pure
// functions that turn resolved scatter series into the GPU instance buffer — geng::detail::build_markers
// and geng::detail::resolve_marker_color — are header-only and testable in isolation. We compare whole
// glm objects (never .x/.y/.r) and index with .at() so this .cpp keeps clang-tidy's union-access and
// unchecked-container-access rules happy (this file is a main TU, so it is linted; headers are not).

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <FigureScene.hpp> // geng::detail::{Scatter, MarkerInstance, build_markers, resolve_marker_color}
#include <geng/Series.hpp>
#include <geng/Theme.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace
{
geng::detail::Scatter make_scatter(std::vector<glm::vec2> points, std::vector<glm::vec4> colors,
								   geng::MarkerShape shape, float size_px, float thickness_px, glm::vec4 fallback)
{
	geng::detail::Scatter scatter;
	scatter.points		 = std::move(points);
	scatter.colors		 = std::move(colors);
	scatter.color		 = fallback;
	scatter.shape		 = shape;
	scatter.size_px		 = size_px;
	scatter.thickness_px = thickness_px;
	return scatter;
}
} // namespace

TEST_CASE("MarkerShape constants are the shader SDF contract", "[scatter][api]")
{
	// marker.frag.slang switches on these exact values — they must not drift.
	REQUIRE(static_cast<std::uint8_t>(geng::MarkerShape::POINT) == 0);
	REQUIRE(static_cast<std::uint8_t>(geng::MarkerShape::CIRCLE) == 1);
	REQUIRE(static_cast<std::uint8_t>(geng::MarkerShape::SQUARE) == 2);
	REQUIRE(static_cast<std::uint8_t>(geng::MarkerShape::DIAMOND) == 3);
	REQUIRE(static_cast<std::uint8_t>(geng::MarkerShape::CROSS) == 4);
	REQUIRE(static_cast<std::uint8_t>(geng::MarkerShape::PLUS) == 5);
	REQUIRE(static_cast<std::uint8_t>(geng::MarkerShape::TRIANGLE) == 6);
}

TEST_CASE("MarkerInstance matches the std430 GPU stride", "[scatter][layout]")
{
	REQUIRE(sizeof(geng::detail::MarkerInstance) == 48);
}

TEST_CASE("build_markers flattens resolved scatter series into instances", "[scatter][markers]")
{
	const auto scale = static_cast<float>(geng::detail::SUPERSAMPLE);

	SECTION("no series yields no instances")
	{
		REQUIRE(geng::detail::build_markers({}).empty());
	}

	SECTION("each point becomes one instance with bake-scaled size and the series shape")
	{
		const geng::detail::Scatter scatter =
			make_scatter({glm::vec2{1.0F, 2.0F}, glm::vec2{3.0F, 4.0F}}, {}, geng::MarkerShape::SQUARE, 8.0F, 2.0F,
						 glm::vec4{0.1F, 0.2F, 0.3F, 1.0F});

		const std::vector<geng::detail::MarkerInstance> out = geng::detail::build_markers({scatter});
		REQUIRE(out.size() == 2);
		REQUIRE(out.at(0).center == glm::vec2{1.0F, 2.0F});
		REQUIRE(out.at(1).center == glm::vec2{3.0F, 4.0F});
		REQUIRE(out.at(0).size_px == 8.0F * scale); // scaled to the bake resolution
		REQUIRE(out.at(0).thickness_px == 2.0F * scale);
		REQUIRE(out.at(0).shape == static_cast<std::uint32_t>(geng::MarkerShape::SQUARE));
		REQUIRE(out.at(0).color == glm::vec4{0.1F, 0.2F, 0.3F, 1.0F}); // fallback (no per-point colors)
		REQUIRE(out.at(1).color == glm::vec4{0.1F, 0.2F, 0.3F, 1.0F});
	}

	SECTION("per-point colors override the series color when the count matches")
	{
		const geng::detail::Scatter scatter =
			make_scatter({glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 1.0F}},
						 {glm::vec4{1.0F, 0.0F, 0.0F, 1.0F}, glm::vec4{0.0F, 1.0F, 0.0F, 1.0F}},
						 geng::MarkerShape::POINT, 4.0F, 1.0F, glm::vec4{0.5F, 0.5F, 0.5F, 1.0F});

		const std::vector<geng::detail::MarkerInstance> out = geng::detail::build_markers({scatter});
		REQUIRE(out.size() == 2);
		REQUIRE(out.at(0).color == glm::vec4{1.0F, 0.0F, 0.0F, 1.0F});
		REQUIRE(out.at(1).color == glm::vec4{0.0F, 1.0F, 0.0F, 1.0F});
	}

	SECTION("a per-point color count that does not match falls back to the series color")
	{
		const geng::detail::Scatter scatter =
			make_scatter({glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 1.0F}}, {glm::vec4{1.0F, 0.0F, 0.0F, 1.0F}},
						 geng::MarkerShape::CROSS, 6.0F, 1.5F, glm::vec4{0.5F, 0.5F, 0.5F, 1.0F});

		const std::vector<geng::detail::MarkerInstance> out = geng::detail::build_markers({scatter});
		REQUIRE(out.size() == 2);
		REQUIRE(out.at(0).color == glm::vec4{0.5F, 0.5F, 0.5F, 1.0F});
		REQUIRE(out.at(1).color == glm::vec4{0.5F, 0.5F, 0.5F, 1.0F});
	}

	SECTION("multiple series are concatenated in order")
	{
		const geng::detail::Scatter first = make_scatter({glm::vec2{0.0F, 0.0F}}, {}, geng::MarkerShape::POINT, 4.0F,
														 1.0F, glm::vec4{1.0F, 1.0F, 1.0F, 1.0F});
		const geng::detail::Scatter second =
			make_scatter({glm::vec2{1.0F, 0.0F}, glm::vec2{2.0F, 0.0F}}, {}, geng::MarkerShape::DIAMOND, 5.0F, 1.0F,
						 glm::vec4{1.0F, 0.0F, 0.0F, 1.0F});

		const std::vector<geng::detail::MarkerInstance> out = geng::detail::build_markers({first, second});
		REQUIRE(out.size() == 3);
		REQUIRE(out.at(0).shape == static_cast<std::uint32_t>(geng::MarkerShape::POINT));
		REQUIRE(out.at(1).shape == static_cast<std::uint32_t>(geng::MarkerShape::DIAMOND));
		REQUIRE(out.at(2).shape == static_cast<std::uint32_t>(geng::MarkerShape::DIAMOND));
	}
}

TEST_CASE("resolve_marker_color follows explicit → palette → default", "[scatter][color]")
{
	geng::Theme theme;
	theme.palette = {glm::vec4{1.0F, 0.0F, 0.0F, 1.0F}, glm::vec4{0.0F, 1.0F, 0.0F, 1.0F}};

	SECTION("an explicit color wins")
	{
		geng::MarkerStyle style;
		style.color = glm::vec4{0.2F, 0.4F, 0.6F, 1.0F};
		REQUIRE(geng::detail::resolve_marker_color(style, 0, theme) == glm::vec4{0.2F, 0.4F, 0.6F, 1.0F});
	}

	SECTION("the palette cycles by creation order when no color is set")
	{
		const geng::MarkerStyle style;
		REQUIRE(geng::detail::resolve_marker_color(style, 0, theme) == theme.palette.at(0));
		REQUIRE(geng::detail::resolve_marker_color(style, 1, theme) == theme.palette.at(1));
		REQUIRE(geng::detail::resolve_marker_color(style, 2, theme) == theme.palette.at(0)); // wraps
	}

	SECTION("an empty palette falls back to the built-in default")
	{
		geng::Theme bare;
		bare.palette			 = {};
		bare.line_defaults.color = {};
		const geng::MarkerStyle style;
		REQUIRE(geng::detail::resolve_marker_color(style, 0, bare) == glm::vec4{0.30F, 0.80F, 1.00F, 1.0F});
	}
}

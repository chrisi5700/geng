// A gallery of every geng scatter marker, shown in a window. Each of the seven MarkerShape symbols is
// drawn as its own scatter series — a column of three markers at three sizes — so the shapes (and the
// hollow ones' stroke width) can be compared side by side. The scene is static; scroll to zoom and
// drag to pan (the WindowApp interactor is wired by default).

#include <array>
#include <cstddef>
#include <exception>
#include <geng/WindowApp.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <utility>
#include <vector>

namespace
{
struct ShapeEntry
{
	geng::MarkerShape shape = geng::MarkerShape::POINT;
	glm::vec4		  color{};
};

// One column per shape, in MarkerShape order, each a distinct palette color.
constexpr std::array<ShapeEntry, 7> SHAPES{{
	{.shape = geng::MarkerShape::POINT, .color = glm::vec4{0.30F, 0.80F, 0.95F, 1.0F}},
	{.shape = geng::MarkerShape::CIRCLE, .color = glm::vec4{0.95F, 0.45F, 0.55F, 1.0F}},
	{.shape = geng::MarkerShape::SQUARE, .color = glm::vec4{0.55F, 0.85F, 0.45F, 1.0F}},
	{.shape = geng::MarkerShape::DIAMOND, .color = glm::vec4{0.95F, 0.75F, 0.30F, 1.0F}},
	{.shape = geng::MarkerShape::CROSS, .color = glm::vec4{0.70F, 0.55F, 0.95F, 1.0F}},
	{.shape = geng::MarkerShape::PLUS, .color = glm::vec4{0.40F, 0.85F, 0.80F, 1.0F}},
	{.shape = geng::MarkerShape::TRIANGLE, .color = glm::vec4{0.95F, 0.60F, 0.40F, 1.0F}},
}};

// Three on-screen sizes per column so the size_px scaling is visible at a glance.
constexpr std::array<float, 3> ROW_SIZES{36.0F, 22.0F, 12.0F};
} // namespace

int main()
{
	try
	{
		geng::WindowApp::Config config;
		config.title			   = "geng — Scatter markers";
		config.figure.initial_view = {.min_x = -1.0F, .max_x = 7.0F, .min_y = -1.0F, .max_y = 3.0F};

		auto app_result = geng::WindowApp::create(config);
		if (!app_result.has_value())
		{
			return 1;
		}
		geng::WindowApp app = std::move(app_result.value());

		for (std::size_t col = 0; col < SHAPES.size(); ++col)
		{
			const ShapeEntry& entry = SHAPES.at(col);
			for (std::size_t row = 0; row < ROW_SIZES.size(); ++row)
			{
				geng::MarkerStyle style;
				style.color		   = entry.color;
				style.shape		   = entry.shape;
				style.size_px	   = ROW_SIZES.at(row);
				style.thickness_px = 3.0F; // visible stroke on the hollow shapes (circle / cross / plus)

				const geng::SeriesId series = app.figure().add_scatter("shape", style);
				app.figure().set_data(
					series, std::vector<glm::vec2>{glm::vec2{static_cast<float>(col), 2.0F - static_cast<float>(row)}});
			}
		}

		app.run(); // static scene; the default interactor handles scroll-zoom and drag-pan
	}
	catch (const std::exception&)
	{
		return 1;
	}
	return 0;
}

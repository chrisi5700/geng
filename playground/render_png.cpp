// geng — render the sin(x) plot to a PNG headlessly (no window): the same graph as the windowed
// demo, but driven by the OffscreenRenderer and captured to a file.

#include <exception>
#include <geng/Bounds2D.hpp>
#include <geng/FontAtlas.hpp>
#include <geng/OffscreenRenderer.hpp>
#include <geng/View.hpp>
#include <glm/vec2.hpp>
#include <numbers>
#include <print>
#include <string>
#include <vector>

#include "plot.hpp"

namespace
{
constexpr std::uint32_t WIDTH  = 1280;
constexpr std::uint32_t HEIGHT = 720;
} // namespace

int main()
{
	constexpr float		 X_LIMIT = 2.0F * std::numbers::pi_v<float>;
	const geng::Bounds2D bounds{.min_x = -X_LIMIT, .max_x = X_LIMIT, .min_y = -1.5F, .max_y = 1.5F};
	const std::string	 out_path = "geng_sin.png";

	try
	{
		geng::OffscreenRenderer renderer(WIDTH, HEIGHT);
		auto					atlas = geng::FontAtlas::create(
			renderer.context(), std::string(GENG_ASSET_DIR) + "/fonts/FiraCodeNerdFontMono-Regular.ttf", 96.0F);
		if (!atlas.has_value())
		{
			std::println("geng: font load failed: {}", geng::to_string(atlas.error()));
			return 1;
		}
		const auto					   sine = demo::sample_sin(bounds, demo::SAMPLE_COUNT);
		const std::vector<demo::Curve> curves{
			{.points = sine, .color = demo::SINE_COLOR},
			{.points = demo::sample_circle(demo::SAMPLE_COUNT), .color = demo::CIRCLE_COLOR}};
		const auto curves_src = renderer.graph().add_source<std::vector<demo::Curve>>(curves);
		geng::View view(demo::fit_bounds(sine));
		const auto view_src = renderer.graph().add_source<geng::Bounds2D>(view.rect());
		demo::plot_curves(renderer.graph(), renderer.screen(), renderer.scene_image(), renderer.scene_color_format(),
						  curves_src, view_src, atlas.value());
		if (!renderer.capture_png(out_path))
		{
			std::println("geng: failed to render {}", out_path);
			return 1;
		}
		std::println("geng: wrote {}", out_path);
	}
	catch (const std::exception& error)
	{
		std::println("geng png demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

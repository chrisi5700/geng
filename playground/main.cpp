// geng first slice: plot sin(x) as a thin-quad polyline through the veng render graph, in a
// window. The scene wiring lives in plot.hpp (shared with the headless PNG demo); this file just
// opens a window and runs the loop. The public Figure/Plot API comes later.

#include <exception>
#include <geng/Bounds2D.hpp>
#include <geng/FontAtlas.hpp>
#include <geng/Renderer.hpp>
#include <numbers>
#include <print>
#include <string>

#include "plot.hpp"

int main()
{
	constexpr float		 X_LIMIT = 2.0F * std::numbers::pi_v<float>;
	const geng::Bounds2D bounds{.min_x = -X_LIMIT, .max_x = X_LIMIT, .min_y = -1.5F, .max_y = 1.5F};

	try
	{
		geng::Renderer renderer("geng — sin(x)", 1280, 720);
		auto		   atlas = geng::FontAtlas::create(
			renderer.context(), std::string(GENG_ASSET_DIR) + "/fonts/FiraCodeNerdFontMono-Regular.ttf", 48.0F);
		if (!atlas.has_value())
		{
			std::println("geng: font load failed: {}", geng::to_string(atlas.error()));
			return 1;
		}
		demo::plot_sin(renderer.graph(), renderer.screen(), renderer.scene_image(), renderer.scene_color_format(),
					   bounds, atlas.value());
		renderer.run();
	}
	catch (const std::exception& error)
	{
		std::println("geng demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

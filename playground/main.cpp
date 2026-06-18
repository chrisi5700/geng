// geng first slice: plot sin(x) as a thin-quad polyline through the veng render graph, in a
// window. The scene wiring lives in plot.hpp (shared with the headless PNG demo); this file just
// opens a window and runs the loop. The public Figure/Plot API comes later.

#include <exception>
#include <numbers>
#include <print>

#include <geng/Bounds2D.hpp>
#include <geng/Renderer.hpp>

#include "plot.hpp"

int main()
{
	constexpr float		 X_LIMIT = 2.0F * std::numbers::pi_v<float>;
	const geng::Bounds2D bounds{.min_x = -X_LIMIT, .max_x = X_LIMIT, .min_y = -1.5F, .max_y = 1.5F};

	try
	{
		geng::Renderer renderer("geng — sin(x)", 1280, 720);
		demo::plot_sin(renderer.graph(), renderer.screen(), renderer.scene_image(), renderer.scene_color_format(),
					   bounds);
		renderer.run();
	}
	catch (const std::exception& error)
	{
		std::println("geng demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

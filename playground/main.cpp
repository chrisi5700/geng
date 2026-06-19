// geng first slice: plot sin(x) as a thin-quad polyline through the veng render graph, in a
// window. The scene wiring lives in plot.hpp (shared with the headless PNG demo); this file just
// opens a window and runs the loop. The public Figure/Plot API comes later.

#include <cmath>
#include <exception>
#include <geng/Bounds2D.hpp>
#include <geng/FontAtlas.hpp>
#include <geng/Renderer.hpp>
#include <geng/View.hpp>
#include <glm/vec2.hpp>
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
		geng::View view(demo::fit_bounds(demo::sample_sin(bounds, demo::SAMPLE_COUNT)));
		const auto view_src = renderer.graph().add_source<geng::Bounds2D>(view.rect());
		demo::plot_sin(renderer.graph(), renderer.screen(), renderer.scene_image(), renderer.scene_color_format(),
					   bounds, view_src, atlas.value());

		// Interactive view control — the windowing stays here in the demo, not the library. Scroll
		// zooms toward the cursor, left-drag pans. Each input mutates the View and re-sets the graph's
		// view-rect source, which the OnDemand loop then renders.
		geng::Window& window = renderer.window();
		double		  last_x = 0.0;
		double		  last_y = 0.0;
		window.on_scroll(
			[&](double /*offset_x*/, double offset_y)
			{
				const auto		cursor = window.cursor_pos();
				const auto		size   = window.window_size();
				const glm::vec2 frac{static_cast<float>(cursor.first) / static_cast<float>(size.width),
									 static_cast<float>(cursor.second) / static_cast<float>(size.height)};
				view.zoom_at(frac, std::pow(0.9F, static_cast<float>(offset_y)));
				renderer.graph().set(view_src, view.rect());
			});
		window.on_cursor_pos(
			[&](double pos_x, double pos_y)
			{
				if (window.mouse_held())
				{
					const auto	size	= window.window_size();
					const float frac_dx = static_cast<float>(pos_x - last_x) / static_cast<float>(size.width);
					const float frac_dy = static_cast<float>(pos_y - last_y) / static_cast<float>(size.height);
					view.pan(glm::vec2{-frac_dx, frac_dy});
					renderer.graph().set(view_src, view.rect());
				}
				last_x = pos_x;
				last_y = pos_y;
			});
		renderer.run();
	}
	catch (const std::exception& error)
	{
		std::println("geng demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

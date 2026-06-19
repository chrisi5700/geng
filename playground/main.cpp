// geng first slice: plot sin(x) as a thin-quad polyline through the veng render graph, in a
// window. The scene wiring lives in plot.hpp (shared with the headless PNG demo); this file just
// opens a window and runs the loop. The public Figure/Plot API comes later.

#include <chrono>
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
#include <vector>

#include "plot.hpp"

namespace
{
/// Wire scroll-zoom (toward the cursor), left-drag pan, and resize-reframe to @p view, re-setting
/// @p view_src after each change so the OnDemand loop renders. The windowing stays here in the demo,
/// not the library; the View math is windowing-agnostic.
template <class ViewSrc>
void install_view_controls(geng::Renderer& renderer, geng::View& view, ViewSrc view_src)
{
	geng::Window& window = renderer.window();
	window.on_scroll(
		[&renderer, &view, view_src](double /*offset_x*/, double offset_y)
		{
			const auto		cursor = renderer.window().cursor_pos();
			const auto		size   = renderer.window().window_size();
			const glm::vec2 frac{static_cast<float>(cursor.first) / static_cast<float>(size.width),
								 static_cast<float>(cursor.second) / static_cast<float>(size.height)};
			view.zoom_at(frac, std::pow(0.9F, static_cast<float>(offset_y)));
			renderer.graph().set(view_src, view.rect());
		});
	window.on_cursor_pos(
		[&renderer, &view, view_src, last_x = 0.0, last_y = 0.0](double pos_x, double pos_y) mutable
		{
			if (renderer.window().mouse_held())
			{
				const auto	size	= renderer.window().window_size();
				const float frac_dx = static_cast<float>(pos_x - last_x) / static_cast<float>(size.width);
				const float frac_dy = static_cast<float>(pos_y - last_y) / static_cast<float>(size.height);
				view.pan(glm::vec2{-frac_dx, frac_dy});
				renderer.graph().set(view_src, view.rect());
			}
			last_x = pos_x;
			last_y = pos_y;
		});
	window.on_resize(
		[&renderer, &view, view_src](int width, int height)
		{
			if (width > 0 && height > 0)
			{
				const float ratio = static_cast<float>(width) / static_cast<float>(height);
				view.focus(geng::reframe_aspect(view.rect(), ratio));
				renderer.graph().set(view_src, view.rect());
			}
		});
}
} // namespace

int main()
{
	constexpr float		 X_LIMIT = 2.0F * std::numbers::pi_v<float>;
	const geng::Bounds2D bounds{.min_x = -X_LIMIT, .max_x = X_LIMIT, .min_y = -1.5F, .max_y = 1.5F};

	try
	{
		geng::Renderer renderer("geng — sin(x)", 1280, 720);
		auto		   atlas = geng::FontAtlas::create(
			renderer.context(), std::string(GENG_ASSET_DIR) + "/fonts/FiraCodeNerdFontMono-Regular.ttf", 96.0F);
		if (!atlas.has_value())
		{
			std::println("geng: font load failed: {}", geng::to_string(atlas.error()));
			return 1;
		}
		const auto				 sine	   = demo::sample_sin(bounds, demo::SAMPLE_COUNT);
		const auto				 fb_extent = renderer.window().framebuffer_extent();
		const float				 aspect	   = static_cast<float>(fb_extent.width) / static_cast<float>(fb_extent.height);
		geng::View				 view(geng::aspect_fit(demo::fit_bounds(sine), aspect));
		std::vector<demo::Curve> curves{
			{.points = sine, .color = demo::SINE_COLOR},
			{.points = demo::sample_circle(demo::SAMPLE_COUNT), .color = demo::CIRCLE_COLOR}};
		const auto curves_src = renderer.graph().add_source<std::vector<demo::Curve>>(curves);
		const auto view_src	  = renderer.graph().add_source<geng::Bounds2D>(view.rect());
		demo::plot_curves(renderer.graph(), renderer.screen(), renderer.scene_image(), renderer.scene_color_format(),
						  curves_src, view_src, atlas.value());

		// Interactive view control (scroll-zoom toward cursor, left-drag pan, resize-reframe).
		install_view_controls(renderer, view, view_src);

		// Stream a new sine sample into the curve every 0.1 s; the reactive graph re-bakes the line
		// cache live. The view is independent, so the user can pan/zoom to follow the growth.
		const float step   = (bounds.max_x - bounds.min_x) / static_cast<float>(demo::SAMPLE_COUNT - 1);
		float		next_x = bounds.max_x + step;
		auto		last   = std::chrono::steady_clock::now();
		renderer.run(
			[&]()
			{
				const auto now = std::chrono::steady_clock::now();
				if (now - last >= std::chrono::milliseconds(100))
				{
					last = now;
					curves.front().points.emplace_back(next_x, std::sin(next_x));
					next_x += step;
					renderer.graph().set(curves_src, curves);
				}
			});
	}
	catch (const std::exception& error)
	{
		std::println("geng demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

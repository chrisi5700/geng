// geng — render the sin(x) plot to a PNG headlessly (no window): the same graph as the windowed
// demo, but driven by the OffscreenRenderer and captured to a file.

#include <exception>
#include <geng/Bounds2D.hpp>
#include <geng/FontAtlas.hpp>
#include <geng/OffscreenRenderer.hpp>
#include <numbers>
#include <print>
#include <string>

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
			renderer.context(), std::string(GENG_ASSET_DIR) + "/fonts/FiraCodeNerdFontMono-Regular.ttf", 48.0F);
		if (!atlas.has_value())
		{
			std::println("geng: font load failed: {}", geng::to_string(atlas.error()));
			return 1;
		}
		demo::plot_sin(renderer.graph(), renderer.screen(), renderer.scene_image(), renderer.scene_color_format(),
					   bounds, atlas.value());
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

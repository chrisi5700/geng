// The simplest live example: a sine wave traced left to right, looping forever. Each frame the
// series' points are rebuilt for x in [0, phase] and the view is held fixed, so the curve is seen
// sweeping across the plot. Scroll to zoom, left-drag to pan.

#include <cmath>
#include <cstddef>
#include <exception>
#include <geng/WindowApp.hpp>
#include <glm/vec2.hpp>
#include <numbers>
#include <print>
#include <utility>
#include <vector>

int main()
{
	constexpr float		  PERIODS		= 2.0F; // sine periods that fill the window width
	constexpr float		  SPAN			= PERIODS * 2.0F * std::numbers::pi_v<float>;
	constexpr float		  SWEEP_SECONDS = 4.0F; // time to draw the full span before looping
	constexpr std::size_t RESOLUTION	= 512;	// samples across the full span

	try
	{
		geng::WindowApp::Config config;
		config.title			   = "geng — growing sine";
		config.figure.initial_view = {.min_x = 0.0F, .max_x = SPAN, .min_y = -1.3F, .max_y = 1.3F};

		auto app_result = geng::WindowApp::create(config);
		if (!app_result.has_value())
		{
			std::println("geng: window init failed");
			return 1;
		}
		geng::WindowApp app = std::move(app_result.value());

		const geng::SeriesId sine = app.figure().add_line("sin");

		app.run(
			[&app, sine](double seconds)
			{
				const float phase = std::fmod(static_cast<float>(seconds) / SWEEP_SECONDS, 1.0F) * SPAN;
				const auto	count = static_cast<std::size_t>((phase / SPAN) * static_cast<float>(RESOLUTION));

				std::vector<glm::vec2> points;
				points.reserve(count + 1);
				for (std::size_t idx = 0; idx <= count; ++idx)
				{
					const float pos_x = (static_cast<float>(idx) / static_cast<float>(RESOLUTION)) * SPAN;
					points.emplace_back(pos_x, std::sin(pos_x));
				}
				app.figure().set_data(sine, std::move(points));
			});
	}
	catch (const std::exception& error)
	{
		std::println("geng growing-sine example failed: {}", error.what());
		return 1;
	}
	return 0;
}

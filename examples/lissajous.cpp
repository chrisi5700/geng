// Lissajous curves: x = sin(a·t + δ), y = sin(b·t). Two integer frequency ratios drawn as closed
// curves whose phase δ drifts over time, so each figure continuously morphs through its family. A
// fixed square view keeps the curves undistorted; scroll to zoom, drag to pan.

#include <cmath>
#include <cstddef>
#include <exception>
#include <geng/WindowApp.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <numbers>
#include <utility>
#include <vector>

namespace
{
constexpr float		  TAU		 = 2.0F * std::numbers::pi_v<float>;
constexpr std::size_t SAMPLES	 = 700;	  // points around one closed curve
constexpr float		  DRIFT_RATE = 0.35F; // phase revolutions per second

/// One closed Lissajous curve at phase @p delta for the frequency pair (@p freq_x, @p freq_y).
std::vector<glm::vec2> lissajous(float freq_x, float freq_y, float delta)
{
	std::vector<glm::vec2> points;
	points.reserve(SAMPLES + 1);
	for (std::size_t idx = 0; idx <= SAMPLES; ++idx)
	{
		const float param = TAU * static_cast<float>(idx) / static_cast<float>(SAMPLES);
		points.emplace_back(std::sin((freq_x * param) + delta), std::sin(freq_y * param));
	}
	return points;
}
} // namespace

int main()
{
	try
	{
		geng::WindowApp::Config config;
		config.title			   = "geng — Lissajous";
		config.figure.initial_view = {.min_x = -1.15F, .max_x = 1.15F, .min_y = -1.15F, .max_y = 1.15F};

		auto app_result = geng::WindowApp::create(config);
		if (!app_result.has_value())
		{
			return 1;
		}
		geng::WindowApp app = std::move(app_result.value());

		const geng::SeriesId curve_a = app.figure().add_line("3:2", {.color = glm::vec4{0.30F, 0.80F, 0.95F, 1.0F}});
		const geng::SeriesId curve_b = app.figure().add_line("5:4", {.color = glm::vec4{0.95F, 0.45F, 0.55F, 1.0F}});

		app.run(
			[&app, curve_a, curve_b](double seconds)
			{
				const float delta = static_cast<float>(seconds) * DRIFT_RATE * TAU;
				app.figure().set_data(curve_a, lissajous(3.0F, 2.0F, delta));
				app.figure().set_data(curve_b, lissajous(5.0F, 4.0F, delta * 0.5F));
			});
	}
	catch (const std::exception&)
	{
		return 1;
	}
	return 0;
}

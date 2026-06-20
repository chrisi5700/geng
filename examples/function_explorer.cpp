// A static, densely-sampled "chirp" whose frequency grows with x — lots of structure at every scale,
// so it rewards zooming. The data is set once and never changes, so this exercises (and is a handy
// manual regression for) the view-change re-bake path: every scroll/drag re-runs the bake with the
// line/glyph storage buffers cached. Scroll to zoom toward the cursor, left-drag to pan.

#include <cmath>
#include <cstddef>
#include <exception>
#include <geng/WindowApp.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <utility>
#include <vector>

namespace
{
constexpr float		  X_MAX	  = 10.0F;
constexpr std::size_t SAMPLES = 4000; // dense enough that deep zooms stay smooth

std::vector<glm::vec2> sample(float (*fun)(float))
{
	std::vector<glm::vec2> points;
	points.reserve(SAMPLES + 1);
	for (std::size_t idx = 0; idx <= SAMPLES; ++idx)
	{
		const float pos_x = X_MAX * static_cast<float>(idx) / static_cast<float>(SAMPLES);
		points.emplace_back(pos_x, fun(pos_x));
	}
	return points;
}
} // namespace

int main()
{
	try
	{
		geng::WindowApp::Config config;
		config.title			   = "geng — zoom/pan explorer (scroll=zoom, drag=pan)";
		config.figure.initial_view = {.min_x = 0.0F, .max_x = X_MAX, .min_y = -1.25F, .max_y = 1.25F};

		auto app_result = geng::WindowApp::create(config);
		if (!app_result.has_value())
		{
			return 1;
		}
		geng::WindowApp app = std::move(app_result.value());

		// chirp and its slowly-decaying envelope — both static.
		const geng::SeriesId chirp = app.figure().add_line("sin(x^2)", {.color = glm::vec4{0.30F, 0.80F, 0.95F, 1.0F}});
		const geng::SeriesId envelope =
			app.figure().add_line("envelope", {.color = glm::vec4{0.95F, 0.55F, 0.30F, 0.7F}});

		app.figure().set_data(chirp, sample([](float pos_x) { return std::sin(pos_x * pos_x); }));
		app.figure().set_data(envelope, sample([](float pos_x) { return std::cos(0.4F * pos_x); }));

		app.run(); // no per-frame work — this one is all interaction
	}
	catch (const std::exception&)
	{
		return 1;
	}
	return 0;
}

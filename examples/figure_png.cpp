// Headless demo of the public geng::Figure API: build an offscreen figure, add a couple of named
// series (one palette-colored, one explicitly colored), autoscale to the data, and write a PNG.

#include <cmath>
#include <cstddef>
#include <exception>
#include <geng/Figure.hpp>
#include <glm/vec2.hpp>
#include <numbers>
#include <print>
#include <vector>

namespace
{
std::vector<glm::vec2> sample_sin(float low, float high, std::size_t samples)
{
	std::vector<glm::vec2> points;
	points.reserve(samples);
	for (std::size_t idx = 0; idx < samples; ++idx)
	{
		const float frac  = static_cast<float>(idx) / static_cast<float>(samples - 1);
		const float pos_x = low + (frac * (high - low));
		points.emplace_back(pos_x, std::sin(pos_x));
	}
	return points;
}

std::vector<glm::vec2> sample_circle(std::size_t samples)
{
	std::vector<glm::vec2> points;
	points.reserve(samples);
	for (std::size_t idx = 0; idx < samples; ++idx)
	{
		const float angle =
			(2.0F * std::numbers::pi_v<float>)*static_cast<float>(idx) / static_cast<float>(samples - 1);
		points.emplace_back(std::cos(angle), std::sin(angle));
	}
	return points;
}
} // namespace

int main()
{
	constexpr std::size_t SAMPLES = 256;
	constexpr float		  X_LIMIT = 2.0F * std::numbers::pi_v<float>;

	try
	{
		auto built = geng::Figure::offscreen();
		if (!built.has_value())
		{
			std::println("geng: figure device init failed");
			return 1;
		}
		geng::Figure figure = std::move(built.value());

		const geng::SeriesId sine = figure.add_line("sin"); // palette color 0 (cyan)
		figure.set_data(sine, sample_sin(-X_LIMIT, X_LIMIT, SAMPLES));

		const geng::SeriesId circle = figure.add_line("circle", {.color = glm::vec4{0.30F, 0.85F, 0.45F, 1.0F}});
		figure.set_data(circle, sample_circle(SAMPLES));

		figure.autoscale(geng::Fit::ALL); // frame all data; aspect-correct so the circle stays round

		if (!figure.render_png(1280, 720, "geng_figure.png").has_value())
		{
			std::println("geng: failed to render geng_figure.png");
			return 1;
		}
		std::println("geng: wrote geng_figure.png");
	}
	catch (const std::exception& error)
	{
		std::println("geng figure demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

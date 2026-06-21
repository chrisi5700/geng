// Mandelbrot set rendered as a scatter plot. We sample a dense grid of complex points c, iterate
// z ← z² + c counting how long each takes to escape |z| > R, and stamp a colored Points{} marker at
// every c: black inside the set, a smooth cosine-palette color (by fractional escape count) outside.
// It is a deliberate stress test of geng's scatter renderer — ~115k point instances in one batched
// draw, each with its own color through Figure::set_point_colors.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <geng/Figure.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <numbers>
#include <print>
#include <utility>
#include <vector>

namespace
{
// The classic view of the whole set; the grid is sized to the same aspect so the samples are square.
constexpr float RE_MIN = -2.5F;
constexpr float RE_MAX = 1.0F;
constexpr float IM_MIN = -1.15F;
constexpr float IM_MAX = 1.15F;

constexpr std::size_t	GRID_W	 = 420;
constexpr std::size_t	GRID_H	 = 276;
constexpr std::uint32_t MAX_ITER = 250;
constexpr float			ESCAPE2	 = 256.0F; // squared escape radius (large ⇒ smoother coloring)

constexpr std::uint32_t WIDTH  = 1500;
constexpr std::uint32_t HEIGHT = 986;

constexpr float TAU = 6.28318530718F;

/// A smooth cosine palette (after Inigo Quilez): three phase-shifted cosines give a continuous, vivid
/// color ramp with no lookup table. Scalar math, so the marker color is built without glm member access.
glm::vec4 palette(float param)
{
	const auto chan = [param](float phase) { return 0.5F + (0.5F * std::cos(TAU * (param + phase))); };
	return glm::vec4{chan(0.0F), chan(0.33F), chan(0.67F), 1.0F};
}

/// Escape-time color for c = (real, imag): black if the orbit stays bounded for MAX_ITER iterations,
/// else a palette color keyed on the smooth (fractional) iteration count, cycled a few times for banding.
glm::vec4 escape_color(float real, float imag)
{
	float		  zr   = 0.0F;
	float		  zi   = 0.0F;
	float		  mag2 = 0.0F;
	std::uint32_t iter = 0;
	while (iter < MAX_ITER)
	{
		const float zr_next = (zr * zr) - (zi * zi) + real;
		const float zi_next = (2.0F * zr * zi) + imag;
		zr					= zr_next;
		zi					= zi_next;
		mag2				= (zr * zr) + (zi * zi);
		if (mag2 > ESCAPE2)
		{
			break;
		}
		++iter;
	}
	if (iter >= MAX_ITER)
	{
		return glm::vec4{0.0F, 0.0F, 0.0F, 1.0F}; // interior of the set
	}
	// Continuous (fractional) iteration count: iter + 1 − log₂(log|z|). Smooth ⇒ no color banding edges.
	const float smooth =
		static_cast<float>(iter) + 1.0F - (std::log(std::log(std::sqrt(mag2))) / std::numbers::ln2_v<float>);
	return palette(smooth / static_cast<float>(MAX_ITER) * 4.0F);
}
} // namespace

int main()
{
	try
	{
		geng::FigureDesc desc;
		desc.theme				  = geng::Theme::dark();
		desc.theme.background	  = glm::vec4{0.0F, 0.0F, 0.0F, 1.0F};
		desc.theme.grid.visible	  = false; // a clean fractal — no grid/axes/labels over the image
		desc.theme.axes.visible	  = false;
		desc.theme.labels.visible = false;
		desc.initial_view		  = {.min_x = RE_MIN, .max_x = RE_MAX, .min_y = IM_MIN, .max_y = IM_MAX};

		auto built = geng::Figure::offscreen(desc);
		if (!built.has_value())
		{
			std::println("geng: figure device init failed");
			return 1;
		}
		geng::Figure figure = std::move(built.value());

		std::vector<glm::vec2> points;
		std::vector<glm::vec4> colors;
		points.reserve(GRID_W * GRID_H);
		colors.reserve(GRID_W * GRID_H);
		for (std::size_t row = 0; row < GRID_H; ++row)
		{
			const float imag = IM_MIN + ((IM_MAX - IM_MIN) * static_cast<float>(row) / static_cast<float>(GRID_H - 1));
			for (std::size_t col = 0; col < GRID_W; ++col)
			{
				const float real =
					RE_MIN + ((RE_MAX - RE_MIN) * static_cast<float>(col) / static_cast<float>(GRID_W - 1));
				points.emplace_back(real, imag);
				colors.push_back(escape_color(real, imag));
			}
		}

		// Filled discs slightly larger than the grid pitch, so the cloud reads as a continuous image.
		geng::MarkerStyle marker;
		marker.shape			 = geng::MarkerShape::POINT;
		marker.size_px			 = 4.5F;
		const geng::SeriesId set = figure.add_scatter("mandelbrot", marker);
		figure.set_data(set, std::move(points));
		figure.set_point_colors(set, std::move(colors));

		if (!figure.render_png(WIDTH, HEIGHT, "geng_mandelbrot.png").has_value())
		{
			std::println("geng: failed to render geng_mandelbrot.png");
			return 1;
		}
		std::println("geng: wrote geng_mandelbrot.png ({} points)", GRID_W * GRID_H);
	}
	catch (const std::exception& error)
	{
		std::println("geng mandelbrot demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

// A Monte-Carlo histogram of a d20 rolled "with advantage" (roll twice, keep the higher), drawn as a
// bar chart. We actually simulate 100 rolls — random_device seeds a Mersenne-Twister, so every run is
// different — and tally how often each face wins. The underlying law is P(max = k) = (2k - 1) / 400, a
// clean linear ramp toward the top of the die, but 100 samples leave plenty of noise on top of it.
//
// The chart turns equal-aspect framing off (FigureDesc::equal_aspect) — the axes carry unrelated units
// (a face index vs. a tally), so the bars should fill the plot, not be forced to a square scale.

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <geng/Figure.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <print>
#include <random>
#include <utility>
#include <vector>

namespace
{
constexpr int FACES = 20;
constexpr int ROLLS = 100;
} // namespace

int main()
{
	try
	{
		geng::FigureDesc desc;
		desc.equal_aspect = false; // a distribution: fill the plot, don't force square data units

		auto built = geng::Figure::offscreen(desc);
		if (!built.has_value())
		{
			std::println("geng: figure device init failed");
			return 1;
		}
		geng::Figure figure = std::move(built.value());

		// Roll a d20 with advantage ROLLS times and tally the winning face. Seeded non-deterministically,
		// so the noise on the bars is different every run.
		std::random_device				   seed;
		std::mt19937					   rng(seed());
		std::uniform_int_distribution<int> d20(1, FACES);

		std::array<int, FACES> counts{};
		for (int roll = 0; roll < ROLLS; ++roll)
		{
			const int winner = std::max(d20(rng), d20(rng));
			++counts.at(winner - 1);
		}

		std::vector<glm::vec2> histogram;
		histogram.reserve(FACES);
		for (int face = 1; face <= FACES; ++face)
		{
			histogram.emplace_back(static_cast<float>(face), static_cast<float>(counts.at(face - 1)));
		}

		const geng::SeriesId bars =
			figure.add_bar("max of 2d20 (100 rolls)", {.color = glm::vec4{0.35F, 0.70F, 0.95F, 1.0F}, .width = 0.85F});
		figure.set_data(bars, std::move(histogram));
		figure.fit_data(); // frame the bars (data_bounds folds in the bar widths and the y=0 baseline)

		if (!figure.render_png(3840, 2160, "geng_d20_advantage.png").has_value())
		{
			std::println("geng: failed to render geng_d20_advantage.png");
			return 1;
		}
		std::println("geng: wrote geng_d20_advantage.png");
	}
	catch (const std::exception& error)
	{
		std::println("geng d20 demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

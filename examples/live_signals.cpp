// A live multi-signal monitor — the closest mirror of feeding simulation data to a plot. Four noisy
// streams are sampled at a fixed step into rolling buffers, and the view uses Fit::FOLLOW_LATEST to
// hold a fixed-width window that slides to track the newest samples (a strip chart). This also keeps
// the per-series storage buffers churning every frame (the path the buffer-retention fix covers).

#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <geng/WindowApp.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <random>
#include <utility>
#include <vector>

namespace
{
constexpr float		  WINDOW	   = 8.0F;	// sim-time units kept on screen
constexpr float		  STEP		   = 0.03F; // sim-time advanced per frame
constexpr std::size_t MAX_POINTS   = 1200;	// rolling buffer cap per series (>> WINDOW / STEP)
constexpr std::size_t SIGNAL_COUNT = 4;

/// Deterministic-shape value for signal @p which at time @p time, before noise is added.
float base_signal(std::size_t which, float time)
{
	switch (which)
	{
		case 0: return std::sin(time);
		case 1: return 0.6F * std::sin((2.3F * time) + 1.0F);
		case 2: return (0.4F * std::sin(0.7F * time)) + (0.3F * std::sin(5.0F * time));
		default: return 0.8F * std::sin(0.45F * time);
	}
}
} // namespace

int main()
{
	try
	{
		geng::WindowApp::Config config;
		config.title			   = "geng — live signals (strip chart)";
		config.figure.initial_view = {.min_x = 0.0F, .max_x = WINDOW, .min_y = -1.6F, .max_y = 1.6F};

		auto app_result = geng::WindowApp::create(config);
		if (!app_result.has_value())
		{
			return 1;
		}
		geng::WindowApp app = std::move(app_result.value());

		const std::array<glm::vec4, SIGNAL_COUNT> colors{
			glm::vec4{0.30F, 0.80F, 0.95F, 1.0F}, glm::vec4{0.40F, 0.85F, 0.45F, 1.0F},
			glm::vec4{0.95F, 0.65F, 0.25F, 1.0F}, glm::vec4{0.85F, 0.45F, 0.80F, 1.0F}};
		const std::array<const char*, SIGNAL_COUNT> names{"signal-0", "signal-1", "signal-2", "signal-3"};

		std::array<geng::SeriesId, SIGNAL_COUNT>		 series{};
		std::array<std::vector<glm::vec2>, SIGNAL_COUNT> buffers;
		for (std::size_t idx = 0; idx < SIGNAL_COUNT; ++idx)
		{
			series.at(idx) = app.figure().add_line(names.at(idx), {.color = colors.at(idx)});
		}
		app.figure().autoscale(geng::Fit::FOLLOW_LATEST); // freezes the WINDOW-wide view to track the latest data

		std::mt19937						  rng{std::random_device{}()};
		std::uniform_real_distribution<float> noise{-0.08F, 0.08F};
		float								  sim_time = 0.0F;

		app.run(
			[&](double /*seconds*/)
			{
				for (std::size_t idx = 0; idx < SIGNAL_COUNT; ++idx)
				{
					auto& buffer = buffers.at(idx);
					buffer.emplace_back(sim_time, base_signal(idx, sim_time) + noise(rng));
					if (buffer.size() > MAX_POINTS)
					{
						buffer.erase(buffer.begin());
					}
					app.figure().set_data(series.at(idx), buffer);
				}
				sim_time += STEP;
			});
	}
	catch (const std::exception&)
	{
		return 1;
	}
	return 0;
}

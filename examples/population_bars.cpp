// A bar-chart "race" of the world's population, in a window. EVERY country is a bar (categorical
// x-axis, names underneath), sorted by population and colored by a stable per-country hue, so a country
// keeps its color as it climbs or slips the ranking. They deliberately do not all fit: the view opens
// on the top dozen and you drag to scroll right through the long tail (scroll wheel zooms). Press
// Up / Down to step the year — the chart re-sorts under your current scroll/zoom, which stays put.
//
// Data is the World Bank country_data.xml in media/ (see examples/population_data.hpp for the parse and
// the aggregate filter). equal_aspect is off so the bars fill the window on each axis.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <geng/Bounds2D.hpp>
#include <geng/WindowApp.hpp>
#include <print>
#include <string>
#include <utility>

#include "population_data.hpp"

namespace
{
constexpr std::size_t ALL_COUNTRIES = 1000;	 ///< Larger than the ~200-country set: draw them all.
constexpr float		  VISIBLE_BARS	= 11.0F; ///< How many bars the view opens on (drag to see the rest).

/// media/ directory: a GENG_MEDIA_DIR env override, else the compile-time default baked at build time.
std::string media_dir()
{
	const char* const env = std::getenv("GENG_MEDIA_DIR");
	return (env != nullptr && *env != '\0') ? std::string(env) : std::string(GENG_MEDIA_DIR);
}
} // namespace

int main()
{
	try
	{
		const std::optional<population::Dataset> data = population::load(media_dir() + "/country_data.xml");
		if (!data.has_value())
		{
			std::println("geng: could not load {}/country_data.xml", media_dir());
			return 1;
		}

		geng::WindowApp::Config config;
		config.title							= "geng - most populated countries (Up/Down: change year)";
		config.width							= 1600;
		config.height							= 900;
		config.figure.equal_aspect				= false; // a ranking: fill the window, don't force square units
		config.figure.theme.labels.pixel_height = 15.0F; // a touch smaller so the country names fit

		auto app_result = geng::WindowApp::create(config);
		if (!app_result.has_value())
		{
			return 1;
		}
		geng::WindowApp app	   = std::move(app_result.value());
		geng::Figure&	figure = app.figure();

		geng::BarStyle style;
		style.width				  = 0.78F;
		const geng::SeriesId bars = figure.add_bar("population", style);
		int					 year = data->max_year;
		population::show_year(figure, bars, *data, year, ALL_COUNTRIES);
		app.set_overlay_text(std::to_string(year)); // fixed year badge, top-right (feng, not the plot)

		// Open the view on the first VISIBLE_BARS bars with the y-axis scaled to the most populous
		// country (a fixed scale, so scrolling the years shows populations growing). The view is set
		// once; panning/zooming and year changes leave it where the user put it.
		const float peak = population::peak_millions(*data, year);
		figure.focus(
			{.min_x = -1.0F, .max_x = VISIBLE_BARS, .min_y = -peak * 0.07F, .max_y = peak * 1.08F}); // room for labels

		// Up / Down step the year (PRESS and REPEAT, so holding the key scrubs through time). Only the
		// data changes — the scroll/zoom position is left untouched.
		app.on_key(
			[&app, &figure, &data, &year, bars](geng::Key key, geng::KeyAction action)
			{
				if (action == geng::KeyAction::RELEASE)
				{
					return;
				}
				if (key == geng::Key::UP)
				{
					year = std::min(data->max_year, year + 1);
				}
				else if (key == geng::Key::DOWN)
				{
					year = std::max(data->min_year, year - 1);
				}
				else
				{
					return;
				}
				population::show_year(figure, bars, *data, year, ALL_COUNTRIES);
				app.set_overlay_text(std::to_string(year));
			});

		app.run();
	}
	catch (const std::exception&)
	{
		return 1;
	}
	return 0;
}

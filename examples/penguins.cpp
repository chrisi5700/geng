// A categorical bar chart: penguin counts by species, with the species *names* labelling the x-axis
// (matplotlib's canonical categorical example). Passing (key, value) data is the whole story — geng
// maps the keys to x positions, shares that mapping across series, and replaces the numeric x ticks
// with the names. Headless: renders a PNG.

#include <exception>
#include <geng/Figure.hpp>
#include <glm/vec4.hpp>
#include <print>
#include <string>
#include <utility>
#include <vector>

int main()
{
	try
	{
		geng::FigureDesc desc;
		desc.equal_aspect = false; // counts vs. species are unrelated units — fill the plot

		auto built = geng::Figure::offscreen(desc);
		if (!built.has_value())
		{
			std::println("geng: figure device init failed");
			return 1;
		}
		geng::Figure figure = std::move(built.value());

		const geng::SeriesId counts =
			figure.add_bar("penguins", {.color = glm::vec4{0.40F, 0.72F, 0.55F, 1.0F}, .width = 0.7F});
		figure.set_data(counts, std::vector<std::pair<std::string, float>>{
									{"Adelie", 146.0F}, {"Chinstrap", 68.0F}, {"Gentoo", 119.0F}});
		figure.fit_data(); // frames the bars; the category names land under each one

		if (!figure.render_png(1280, 720, "geng_penguins.png").has_value())
		{
			std::println("geng: failed to render geng_penguins.png");
			return 1;
		}
		std::println("geng: wrote geng_penguins.png");
	}
	catch (const std::exception& error)
	{
		std::println("geng penguins demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

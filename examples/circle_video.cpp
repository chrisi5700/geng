// Headless animation -> PNG frame sequence -> mp4. Renders the classic circle / sine / cosine
// relationship one frame at a time through Figure::offscreen + render_png (no window needed), sweeping
// the angle 0..2pi across the run, then prints the ffmpeg command to stitch the frames into a video.
//
// After running:
//   ffmpeg -y -framerate 30 -i circle_frames/frame_%04d.png -c:v libx264 -pix_fmt yuv420p -crf 18 circle.mp4

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <geng/Figure.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <numbers>
#include <print>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
constexpr float			TAU				   = 2.0F * std::numbers::pi_v<float>;
constexpr float			SINE_X0			   = 2.0F;	// x at which the unrolled sine begins
constexpr float			COS_Y0			   = -2.0F; // y at which the unrolled cosine begins
constexpr float			MARKER_RADIUS	   = 0.045F;
constexpr std::size_t	REVOLUTION_SAMPLES = 360;
constexpr std::uint32_t WIDTH			   = 1280;
constexpr std::uint32_t HEIGHT			   = 720;
constexpr int			FRAMERATE		   = 30;
constexpr std::size_t	FRAME_COUNT		   = 90; // one full revolution; bump for a longer clip

/// The seven series that make up the figure (handles into one offscreen Figure).
struct Series
{
	geng::SeriesId circle;
	geng::SeriesId arm;
	geng::SeriesId sine;
	geng::SeriesId cosine;
	geng::SeriesId h_link;
	geng::SeriesId v_link;
	geng::SeriesId marker;
};

/// Sample t in [0, angle] into points via make(t), always at least the two endpoints.
template <class Fn>
std::vector<glm::vec2> trace(float angle, const Fn& make)
{
	const auto			   count = static_cast<std::size_t>((angle / TAU) * static_cast<float>(REVOLUTION_SAMPLES));
	std::vector<glm::vec2> points;
	points.reserve(count + 1);
	for (std::size_t idx = 0; idx <= count; ++idx)
	{
		const float param = (count == 0) ? 0.0F : angle * static_cast<float>(idx) / static_cast<float>(count);
		points.push_back(make(param));
	}
	return points;
}

Series add_series(geng::Figure& fig)
{
	const glm::vec4 dim{0.50F, 0.52F, 0.58F, 0.9F};
	return Series{.circle = fig.add_line("circle", {.color = glm::vec4{0.30F, 0.80F, 0.95F, 1.0F}}),
				  .arm	  = fig.add_line("radius", {.color = glm::vec4{0.82F, 0.84F, 0.90F, 1.0F}}),
				  .sine	  = fig.add_line("sin", {.color = glm::vec4{0.35F, 0.85F, 0.45F, 1.0F}}),
				  .cosine = fig.add_line("cos", {.color = glm::vec4{0.95F, 0.65F, 0.25F, 1.0F}}),
				  .h_link = fig.add_line("sin-link", {.color = dim}),
				  .v_link = fig.add_line("cos-link", {.color = dim}),
				  .marker = fig.add_line("point", {.color = glm::vec4{1.0F, 1.0F, 1.0F, 1.0F}})};
}

void draw(geng::Figure& fig, const Series& series, float angle)
{
	const float cos_a = std::cos(angle);
	const float sin_a = std::sin(angle);
	fig.set_data(series.circle, trace(angle, [](float param) { return glm::vec2{std::cos(param), std::sin(param)}; }));
	fig.set_data(series.sine, trace(angle, [](float param) { return glm::vec2{SINE_X0 + param, std::sin(param)}; }));
	fig.set_data(series.cosine, trace(angle, [](float param) { return glm::vec2{std::cos(param), COS_Y0 - param}; }));
	fig.set_data(series.arm, std::vector<glm::vec2>{{0.0F, 0.0F}, {cos_a, sin_a}});
	fig.set_data(series.h_link, std::vector<glm::vec2>{{cos_a, sin_a}, {SINE_X0 + angle, sin_a}});
	fig.set_data(series.v_link, std::vector<glm::vec2>{{cos_a, sin_a}, {cos_a, COS_Y0 - angle}});
	fig.set_data(series.marker, std::vector<glm::vec2>{{cos_a + MARKER_RADIUS, sin_a},
													   {cos_a, sin_a + MARKER_RADIUS},
													   {cos_a - MARKER_RADIUS, sin_a},
													   {cos_a, sin_a - MARKER_RADIUS},
													   {cos_a + MARKER_RADIUS, sin_a}});
}

bool render_frame(geng::Figure& fig, const Series& series, std::size_t frame, const std::string& dir)
{
	const float angle =
		(FRAME_COUNT <= 1) ? TAU : TAU * static_cast<float>(frame) / static_cast<float>(FRAME_COUNT - 1);
	draw(fig, series, angle);
	const std::string path = std::format("{}/frame_{:04}.png", dir, frame);
	if (!fig.render_png(WIDTH, HEIGHT, path).has_value())
	{
		std::println("geng: failed to render {}", path);
		return false;
	}
	return true;
}
} // namespace

int main()
{
	const std::string out_dir = "circle_frames";
	try
	{
		std::error_code dir_error;
		std::filesystem::create_directories(out_dir, dir_error);
		if (dir_error)
		{
			std::println("geng: could not create {}: {}", out_dir, dir_error.message());
			return 1;
		}

		geng::FigureDesc desc;
		desc.initial_view = {
			.min_x = -1.3F, .max_x = SINE_X0 + TAU + 0.3F, .min_y = COS_Y0 - TAU - 0.3F, .max_y = 1.3F};

		auto built = geng::Figure::offscreen(desc);
		if (!built.has_value())
		{
			std::println("geng: figure device init failed");
			return 1;
		}
		geng::Figure fig	= std::move(built.value());
		const Series series = add_series(fig);

		for (std::size_t frame = 0; frame < FRAME_COUNT; ++frame)
		{
			if (!render_frame(fig, series, frame, out_dir))
			{
				return 1;
			}
		}

		std::println("geng: wrote {} frames to {}/", FRAME_COUNT, out_dir);
		std::println("stitch into an mp4 with:");
		std::println("  ffmpeg -y -framerate {} -i {}/frame_%04d.png -c:v libx264 -pix_fmt yuv420p -crf 18 circle.mp4",
					 FRAMERATE, out_dir);
	}
	catch (const std::exception& error)
	{
		std::println("geng circle-video render failed: {}", error.what());
		return 1;
	}
	return 0;
}

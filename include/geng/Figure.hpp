#ifndef GENG_FIGURE_HPP
#define GENG_FIGURE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <geng/Bounds2D.hpp>
#include <geng/Series.hpp>
#include <geng/Theme.hpp>
#include <geng/View.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <veng/rendergraph/RenderGraphCommon.hpp> // graph::DataHandle (a small POD) for the source members
#include <veng/rhi/Enums.hpp>					  // rhi::Extent2D / rhi::Format for the target description
#include <vulkan/vulkan.h>						  // only for the embedding seam (VulkanContext / VulkanTarget)

// Heavy veng collaborators are forward-declared and held by unique_ptr, so this header pulls in no
// veng graph/pool/executor machinery and no veng type ever appears in a public signature.
namespace veng
{
class Context;
class ResourcePool;
class CommandManager;
class HeadlessExecutor;
} // namespace veng
namespace veng::graph
{
class Graph;
class Scheduler;
} // namespace veng::graph

namespace feng
{
class FontAtlas; // text-rendering library: the baked tick-label font atlas (see <feng/FontAtlas.hpp>)
} // namespace feng

namespace geng
{
/// A hard, non-frame error from a @ref Figure operation.
enum class Error : std::uint8_t
{
	DEVICE_CREATION_FAILED,
	FONT_LOAD_FAILED,
	TARGET_INVALID,
	RENDER_FAILED
};

/// Outcome of a single @ref Figure::render_into.
enum class FrameStatus : std::uint8_t
{
	DRAWN, ///< The scene was dirty; the figure rendered into the target this call.
	IDLED, ///< Nothing changed since the last render; the target was left untouched.
	FAILED ///< A node failed (e.g. shader/pipeline build); nothing was drawn.
};

/// Tick-label font. An empty @ref path uses geng's bundled default; labels are disabled if no font
/// resolves.
struct FontSpec
{
	std::string path;
	float		pixel_height = 96.0F; ///< Atlas bake height; on-screen label size lives in @ref LabelStyle.
};

/// Content and style only — no surface, no device, no in-flight depth (all inferred or supplied per
/// render).
struct FigureDesc
{
	Theme	 theme = Theme::dark();
	FontSpec font;
	Bounds2D initial_view{.min_x = -1.0F, .max_x = 1.0F, .min_y = -1.0F, .max_y = 1.0F};
	/// Keep one data unit the same on-screen length on both axes (circles round). Off fills the viewport
	/// per axis — the right default for bar charts / distributions (see @ref Figure::set_equal_aspect).
	bool equal_aspect = true;
};

/// Host Vulkan objects geng adopts for the embedded path (e.g. from `QVulkanInstance` /
/// `QVulkanWindow`). geng creates no device of its own in this mode and owns none of these.
struct VulkanContext
{
	VkInstance		 instance		 = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice		 device			 = VK_NULL_HANDLE;
	VkQueue			 graphics_queue	 = VK_NULL_HANDLE;
	std::uint32_t	 graphics_family = 0;
};

/// The image geng draws one frame into, plus the host's command buffer and in-flight index. geng
/// records into @ref command_buffer; the host owns acquisition, submission, and synchronization.
struct VulkanTarget
{
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
	VkImage			image		   = VK_NULL_HANDLE;
	VkImageView		view		   = VK_NULL_HANDLE;
	VkFormat		format		   = VK_FORMAT_UNDEFINED;
	std::uint32_t	width		   = 0;
	std::uint32_t	height		   = 0;
	std::uint32_t	frame_index	   = 0; ///< Host in-flight index; geng sizes its N-buffering from the max seen.
};

/// A 2D plot: the reactive scene (series + view + theme) and its GPU realization. Bound to a device
/// at construction — one geng owns (offscreen) or one the host owns (embedded). Targets are chosen
/// per render call, so one figure can drive a Qt frame now and write a PNG later on the same device.
class Figure
{
	 public:
	/// Build on a geng-owned headless device (PNG / buffer output).
	[[nodiscard]] static std::expected<Figure, Error> offscreen(const FigureDesc& desc = {});
	/// Build on a host-owned device (Qt). geng adopts @p host and creates no device or swapchain.
	[[nodiscard]] static std::expected<Figure, Error> embedded(const VulkanContext& host, const FigureDesc& desc = {});

	~Figure();
	Figure(Figure&&) noexcept;
	Figure& operator=(Figure&&) noexcept;
	Figure(const Figure&)			 = delete;
	Figure& operator=(const Figure&) = delete;

	// --- series CRUD (handle-based; no whole-figure re-upload) ---------------------------------

	/// Add a line series. Color resolves from @p style, else the theme palette, else the default.
	[[nodiscard]] SeriesId add_line(std::string name, LineStyle style = {});

	/// Add a scatter series: the @ref MarkerStyle symbol stamped at every point. Color resolves like
	/// @ref add_line; per-point colors (e.g. a colormap) are supplied separately via @ref set_point_colors.
	[[nodiscard]] SeriesId add_scatter(std::string name, MarkerStyle style = {});

	/// Add a bar series: each point `(x, y)` becomes one @ref BarStyle bar (data-unit rectangle from the
	/// baseline to `y`). Bar charts usually want @ref set_equal_aspect off so the bars fill the plot.
	[[nodiscard]] SeriesId add_bar(std::string name, BarStyle style = {});

	void append(SeriesId series, std::span<const glm::vec2> points); ///< Streaming append; no-op if invalid.
	void set_data(SeriesId series, std::vector<glm::vec2> points);
	/// Categorical data: each `(key, value)` becomes a point at `(slot, value)`, where the string key's
	/// slot is its index in the figure's x-axis category registry (assigned on first sight, shared across
	/// series, so the same key lines up everywhere). The x-axis then shows the names instead of numbers.
	void set_data(SeriesId series, const std::vector<std::pair<std::string, float>>& keyed);
	/// Per-point colors for a scatter or bar series — one entry per point, overriding the series color
	/// (the path the Mandelbrot example uses). An empty vector clears the override; a length that does
	/// not match the point count is ignored at sync time and the series color is used instead.
	void set_point_colors(SeriesId series, std::vector<glm::vec4> colors);
	void remove(SeriesId series);
	void clear();
	/// Reset the x-axis category registry without touching the series. Use it when the category set
	/// changes wholesale between frames (e.g. a bar-chart race over time), so the next keyed @ref
	/// set_data numbers its keys from 0 again instead of appending to the accumulated set.
	void clear_categories();

	void					  set_style(SeriesId series, const LineStyle& style);
	void					  set_style(SeriesId series, const MarkerStyle& style);
	void					  set_style(SeriesId series, const BarStyle& style);
	void					  set_name(SeriesId series, const std::string& name);
	[[nodiscard]] bool		  contains(SeriesId series) const noexcept;
	[[nodiscard]] std::size_t series_count() const noexcept;

	// --- view & controls (programmatic; an Interactor wraps these for input) --------------------

	[[nodiscard]] const View& view() const noexcept;
	/// Bounds enclosing every series' data (an empty figure gives a unit box). The source for Fit::ALL.
	[[nodiscard]] Bounds2D data_bounds() const noexcept;

	void pan(glm::vec2 delta_fraction);					///< Translate the view by a fraction of its extent.
	void zoom(glm::vec2 anchor_fraction, float factor); ///< Scale about the data point under a [0,1] anchor.
	void focus(const Bounds2D& rect);					///< Jump the view to a data rect (aspect-corrected).
	void fit_data();									///< One-shot focus() on data_bounds().

	void			  autoscale(Fit mode) noexcept; ///< Continuous tracking; suspends interaction.
	[[nodiscard]] Fit autoscale() const noexcept;

	// --- styling -------------------------------------------------------------------------------

	void					   set_theme(const Theme& new_theme);
	[[nodiscard]] const Theme& theme() const noexcept;

	/// Equal-aspect framing: when on (the default), the view is grown to the viewport aspect so one data
	/// unit is the same length on both axes (circles stay round). Turn it off for charts whose axes carry
	/// unrelated units — bar charts, distributions — so the view fills the viewport on each axis
	/// independently and the data is not squashed. Takes effect on the next render.
	void			   set_equal_aspect(bool enabled) noexcept;
	[[nodiscard]] bool equal_aspect() const noexcept;

	// --- targets -------------------------------------------------------------------------------

	/// Whether the scene would produce a new image at @p width × @p height — i.e. the data, view, or
	/// theme changed since the last render. A host doing on-demand presentation calls this *before*
	/// acquiring a target: if false it can skip the frame entirely and leave the last presented image
	/// on screen (acquiring then presenting an un-redrawn swapchain image is the multi-buffer trap).
	/// Cheap: it resolves the graph but records no GPU work. @ref render_into stays safe to call
	/// regardless — it returns @ref FrameStatus::IDLED when there is nothing to draw.
	[[nodiscard]] bool needs_redraw(std::uint32_t width, std::uint32_t height);

	/// Render once into @p target (the embedded / Qt path), recording into the host command buffer.
	[[nodiscard]] FrameStatus render_into(const VulkanTarget& target);
	/// Render to an RGBA8 PNG at @p width × @p height (valid on any device binding).
	[[nodiscard]] std::expected<void, Error> render_png(std::uint32_t width, std::uint32_t height,
														const std::string& path);

	 private:
	Figure(); // constructed by the factories

	/// Shared construction once a context exists (used by both factories): build the device helpers,
	/// load the font, and wire the scene graph.
	[[nodiscard]] static std::expected<Figure, Error> build(std::unique_ptr<veng::Context> ctx, const FigureDesc& desc);

	/// Push the resolved series + theme into the reactive sources (rebuilt only when the scene changed).
	void sync_scene();
	/// Size the target sources for @p width × @p height (aspect-corrects the projected view).
	[[nodiscard]] bool ensure_device_sized(std::uint32_t width, std::uint32_t height);
	/// The sliding-window bounds for Fit::FOLLOW_LATEST: a fixed-x-width window anchored to the latest
	/// data, with y fit to the points inside it.
	[[nodiscard]] Bounds2D follow_bounds() const;

	// --- scene model (the figure's own state) ---
	/// Which renderer a series drives. The style member matching the kind is the live one.
	enum class SeriesKind : std::uint8_t
	{
		LINE,
		SCATTER,
		BAR
	};
	struct SeriesData
	{
		std::string			   name;
		std::vector<glm::vec2> points;
		std::vector<glm::vec4> point_colors; ///< Per-point scatter/bar colors (empty => use the series color).
		SeriesKind			   kind = SeriesKind::LINE;
		LineStyle			   line;   ///< Live when @ref kind is LINE.
		MarkerStyle			   marker; ///< Live when @ref kind is SCATTER.
		BarStyle			   bar;	   ///< Live when @ref kind is BAR.
	};
	/// Whether @p series is currently drawn (reads the visibility of its live style).
	[[nodiscard]] static bool					  is_visible(const SeriesData& series) noexcept;
	std::unordered_map<std::uint64_t, SeriesData> m_series;
	std::vector<std::uint64_t>					  m_order;		 ///< Creation order, for palette cycling.
	std::uint64_t								  m_next_id = 1; ///< Monotonic; never recycled.
	/// The x-axis category registry: ordered keys (by first appearance) that keyed set_data resolves
	/// against. Empty means a numeric x-axis; non-empty switches the x ticks to these names.
	std::vector<std::string> m_x_categories;
	View					 m_view{Bounds2D{.min_x = -1.0F, .max_x = 1.0F, .min_y = -1.0F, .max_y = 1.0F}};
	Theme					 m_theme;
	Fit						 m_fit			= Fit::OFF;
	float					 m_follow_width = 0.0F; ///< x-window width frozen when Fit::FOLLOW_LATEST is enabled.
	bool					 m_scene_dirty	= true;
	bool m_equal_aspect = true; ///< Grow the view to the viewport aspect (round circles) vs. fill per axis.

	// --- GPU realization (veng-backed; forward-declared, held by pointer — named collaborators) ---
	std::unique_ptr<veng::Context>			m_ctx; ///< Owned (offscreen) or adopting (embedded).
	std::unique_ptr<veng::ResourcePool>		m_pool;
	std::unique_ptr<veng::CommandManager>	m_commands;
	std::unique_ptr<veng::graph::Scheduler> m_scheduler;
	std::unique_ptr<veng::HeadlessExecutor> m_headless; ///< Drives render_png (and embedded readback).
	std::unique_ptr<veng::graph::Graph>		m_graph;
	std::unique_ptr<feng::FontAtlas>		m_font;

	veng::graph::DataHandle m_screen;	   ///< source<Extent2D>
	veng::graph::DataHandle m_view_src;	   ///< source<Bounds2D>
	veng::graph::DataHandle m_theme_src;   ///< source<Theme>
	veng::graph::DataHandle m_curves_src;  ///< source<std::vector<Curve>>
	veng::graph::DataHandle m_markers_src; ///< source<std::vector<MarkerInstance>>
	veng::graph::DataHandle m_bars_src;	   ///< source<std::vector<BarInstance>>
	veng::graph::DataHandle m_x_ticks_src; ///< source<AxisTicks> (empty => numeric x-axis)
	veng::graph::DataHandle m_scene_image; ///< the composited scene the graph produces
};
} // namespace geng

#endif // GENG_FIGURE_HPP

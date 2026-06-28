// geng::WindowApp — the GLFW windowed front-end. It plays the host role (owns the window, device,
// swapchain, and presentation) and drives a Figure through Figure::render_into, exactly as a
// QVulkanWindow would. The per-frame contract: acquire a swapchain image, hand the figure a begun
// command buffer to record its scene + blit into, transition the image to PRESENT, submit, present.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <feng/FontAtlas.hpp>
#include <feng/Text.hpp>
#include <feng/TextGraph.hpp>
#include <functional>
#include <geng/Bounds2D.hpp>
#include <geng/WindowApp.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <veng/context/Context.hpp>
#include <veng/gpu/BufferRef.hpp>
#include <veng/gpu/ImageRef.hpp>
#include <veng/managers/CommandManager.hpp>
#include <veng/managers/HeadlessExecutor.hpp>
#include <veng/managers/QueueKind.hpp>
#include <veng/managers/SwapchainManager.hpp>
#include <veng/nodes/GraphicsNode.hpp>
#include <veng/nodes/StorageBufferNode.hpp>
#include <veng/rendergraph/data/Data.hpp>
#include <veng/rendergraph/Graph.hpp>
#include <veng/resources/ResourcePool.hpp>
#include <veng/rhi/Convert.hpp>

namespace geng
{
namespace
{
/// N-buffering depth. Must match the Figure's own internal depth so its ResourcePool slot (chosen
/// as frame_index % N inside render_into) stays in step with the swapchain slot used here.
constexpr std::size_t FRAMES_IN_FLIGHT = 2;
constexpr int		  IDLE_SLEEP_MS	   = 10;

// Fixed corner-overlay badge (see set_overlay_text): a small opaque image rendered once per text
// change with feng, then blitted into the top-right corner each frame.
constexpr std::uint32_t BADGE_W		  = 240;   ///< Badge image width in pixels.
constexpr std::uint32_t BADGE_H		  = 96;	   ///< Badge image height in pixels.
constexpr std::int32_t	BADGE_MARGIN  = 24;	   ///< Gap from the badge to the window's top/right edges.
constexpr float			BADGE_TEXT_PX = 56.0F; ///< On-screen height of the overlay text.

/// Matches `PushData` in feng's shaders/text.vert.slang (std430: mat4 @0, vec2 @64).
struct TextPush
{
	glm::mat4 view_proj;
	glm::vec2 extent;

	friend bool operator==(const TextPush&, const TextPush&) noexcept = default;
};

/// geng's bundled asset directory (the default overlay font), with a `GENG_ASSET_DIR` env override —
/// the same resolution Figure uses, so the overlay shares geng's bundled font.
std::string asset_dir()
{
	const char* const env = std::getenv("GENG_ASSET_DIR");
	return (env != nullptr && *env != '\0') ? std::string(env) : std::string(GENG_ASSET_DIR);
}
} // namespace

WindowApp::WindowApp()								  = default;
WindowApp::WindowApp(WindowApp&&) noexcept			  = default;
WindowApp& WindowApp::operator=(WindowApp&&) noexcept = default;

WindowApp::~WindowApp()
{
	if (m_ctx)
	{
		(void)m_ctx->device().waitIdle(); // never tear down while the GPU is still consuming a frame.
	}
}

std::expected<WindowApp, Error> WindowApp::create()
{
	return create(Config{});
}

std::expected<WindowApp, Error> WindowApp::create(const Config& config)
{
	WindowApp app;
	try
	{
		app.m_window = std::make_unique<Window>(config.title.c_str(), config.width, config.height);
	}
	catch (const std::exception&)
	{
		return std::unexpected(Error::DEVICE_CREATION_FAILED);
	}

	// This app owns its device (it is the host). It carries feng's shader path so the optional corner
	// text overlay (set_overlay_text) can run feng's text shaders on this context; the figure's adopted
	// context (built below) carries geng's own shaders for the plot itself.
	const std::string					  feng_shaders = feng::shader_dir();
	const std::array<std::string_view, 1> shader_paths{feng_shaders};
	auto								  ctx_result = veng::Context::create(
		"geng-window", app.m_window->required_extensions(),
		[&app](VkInstance instance) { return app.m_window->create_surface(instance); }, shader_paths);
	if (!ctx_result.has_value())
	{
		return std::unexpected(Error::DEVICE_CREATION_FAILED);
	}
	app.m_ctx = std::make_unique<veng::Context>(std::move(ctx_result.value()));

	auto swap_result = veng::SwapchainManager::create(*app.m_ctx, app.m_window->framebuffer_extent(), FRAMES_IN_FLIGHT);
	if (!swap_result.has_value())
	{
		return std::unexpected(Error::DEVICE_CREATION_FAILED);
	}
	app.m_swap	   = std::make_unique<veng::SwapchainManager>(std::move(swap_result.value()));
	app.m_commands = std::make_unique<veng::CommandManager>(*app.m_ctx);

	const VulkanContext host{.instance		  = static_cast<VkInstance>(app.m_ctx->instance()),
							 .physical_device = static_cast<VkPhysicalDevice>(app.m_ctx->physical_device()),
							 .device		  = static_cast<VkDevice>(app.m_ctx->device()),
							 .graphics_queue  = static_cast<VkQueue>(app.m_ctx->graphics_queue()),
							 .graphics_family = app.m_ctx->queue_indices().graphics};
	auto				figure = Figure::embedded(host, config.figure);
	if (!figure.has_value())
	{
		return std::unexpected(figure.error());
	}
	app.m_figure	 = std::make_unique<Figure>(std::move(figure.value()));
	app.m_interactor = std::make_unique<Interactor>(*app.m_figure);

	// Capture what a corner overlay would need (it is lazily built on the first set_overlay_text): the
	// badge background (the plot background, so the badge reads as floating text) and the font spec. The
	// RGBA components are memcpy'd out of the glm vec4 — glm's .x/[]" accessors are union/container reads
	// the project's clang-tidy rejects in a .cpp.
	std::memcpy(app.m_overlay_bg.data(), glm::value_ptr(config.figure.theme.background), sizeof(float) * 4);
	app.m_overlay_font_path	  = config.figure.font.path;
	app.m_overlay_font_height = config.figure.font.pixel_height;

	app.install_controls();
	return app;
}

Figure& WindowApp::figure() noexcept
{
	return *m_figure;
}
const Figure& WindowApp::figure() const noexcept
{
	return *m_figure;
}
Interactor& WindowApp::interactor() noexcept
{
	return *m_interactor;
}

void WindowApp::on_key(std::function<void(Key, KeyAction)> callback)
{
	m_window->on_key(std::move(callback));
}

bool WindowApp::init_overlay()
{
	using namespace veng;
	using namespace veng::graph;

	const std::string font_path =
		m_overlay_font_path.empty() ? (asset_dir() + "/fonts/FiraCodeNerdFontMono-Regular.ttf") : m_overlay_font_path;
	auto atlas = feng::FontAtlas::create(*m_ctx, font_path, m_overlay_font_height);
	if (!atlas.has_value())
	{
		m_overlay_failed = true; // remember the failure so we do not retry on every set_overlay_text
		return false;
	}

	m_overlay_font = std::make_unique<feng::FontAtlas>(std::move(atlas.value()));
	m_overlay_pool =
		std::make_unique<ResourcePool>(m_ctx->device(), m_ctx->rhi(), m_ctx->allocator(), FRAMES_IN_FLIGHT);
	m_overlay_commands	= std::make_unique<CommandManager>(*m_ctx);
	m_overlay_scheduler = std::make_unique<InlineScheduler>();
	m_overlay_headless =
		std::make_unique<HeadlessExecutor>(*m_ctx, *m_overlay_pool, *m_overlay_commands, *m_overlay_scheduler);
	m_overlay_graph = std::make_unique<Graph>();

	auto&			graph	  = *m_overlay_graph;
	const auto		glyphs	  = graph.add_source<std::vector<feng::Glyph>>({});
	const auto		extent	  = graph.add_source<rhi::Extent2D>(rhi::Extent2D{.width = BADGE_W, .height = BADGE_H});
	const auto		atlas_src = graph.add_source<gpu::ImageRef>(m_overlay_font->atlas_ref());
	const glm::mat4 proj	  = ortho_view(Bounds2D{
		.min_x = 0.0F, .max_x = static_cast<float>(BADGE_W), .min_y = 0.0F, .max_y = static_cast<float>(BADGE_H)});
	const auto		push	  = graph.add_source<TextPush>(
		TextPush{.view_proj = proj, .extent = glm::vec2(static_cast<float>(BADGE_W), static_cast<float>(BADGE_H))});
	m_overlay_glyphs_src = glyphs.handle;

	const DataHandle glyphs_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
	graph.set_producer(glyphs_ref, graph.add(std::make_unique<nodes::StorageBufferNode>(glyphs, "glyphs", glyphs_ref)));

	// One node, like feng::add_text_layer, but cleared to an OPAQUE background so the badge can be a
	// plain blit (no blend pass) into the swapchain corner.
	const DataHandle badge = graph.add(std::make_unique<ValueData<gpu::ImageRef>>(gpu::ImageRef{}));
	auto			 node  = std::make_unique<nodes::GraphicsNode>("text.vert", "text.frag", rhi::Format::RGBA8_UNORM,
																   rhi::Format::UNDEFINED, 6, extent.handle, badge);
	node->add_storage_buffer(glyphs_ref)
		.add_sampled_image(atlas_src.handle, "atlas")
		.set_instances_from(glyphs_ref)
		.push_constant<TextPush>(push.handle, rhi::ShaderStage::VERTEX)
		.clear_color(m_overlay_bg)
		.blend(true);
	graph.set_producer(badge, graph.add(std::move(node)));
	m_overlay_image = badge;
	return true;
}

void WindowApp::set_overlay_text(const std::string& text)
{
	if (text.empty())
	{
		m_overlay_ready = false; // clear the badge (nothing blitted next frame)
		return;
	}
	if (m_overlay_failed || (m_overlay_font == nullptr && !init_overlay()))
	{
		return;
	}

	// Lay the text out centred in the badge, sized to BADGE_TEXT_PX, and re-render the badge once (it
	// only changes when the caller changes the text), caching the image to blit every frame.
	std::vector<feng::Glyph> glyphs;
	const feng::TextStyle	 style{.color  = glm::vec4{0.93F, 0.94F, 0.97F, 1.0F},
								   .scale  = BADGE_TEXT_PX / m_overlay_font->pixel_height(),
								   .halign = feng::HAlign::CENTER,
								   .valign = feng::VAlign::MIDDLE,
								   .gap	   = 0.0F};
	feng::append_text(glyphs, *m_overlay_font, text,
					  glm::vec2(static_cast<float>(BADGE_W) * 0.5F, static_cast<float>(BADGE_H) * 0.5F), style);

	m_overlay_graph->set(veng::graph::TypedHandle<std::vector<feng::Glyph>>{m_overlay_glyphs_src}, std::move(glyphs));
	m_overlay_ready = m_overlay_headless->run_once(*m_overlay_graph, std::array{m_overlay_image}).has_value();
}

void WindowApp::blit_overlay(VkCommandBuffer cmd_raw, VkImage swap_raw, veng::rhi::Extent2D extent)
{
	if (!m_overlay_ready)
	{
		return;
	}
	const auto* slot =
		dynamic_cast<veng::graph::ValueData<veng::gpu::ImageRef>*>(m_overlay_graph->get_data(m_overlay_image));
	if (slot == nullptr || extent.width < BADGE_W + (2U * static_cast<std::uint32_t>(BADGE_MARGIN)))
	{
		return; // no image yet, or the window is too narrow to seat the badge — skip rather than clip
	}

	const veng::gpu::ImageRef ref = slot->value();
	const vk::CommandBuffer	  cmd{cmd_raw};
	const vk::Image			  src = m_ctx->rhi().image(ref.texture);
	const vk::Image			  dst{swap_raw};

	// The badge was left in color-attachment layout by its graphics node; move it to transfer-src.
	m_overlay_pool->transition_image(ref.pool_id, cmd, vk::ImageLayout::eTransferSrcOptimal,
									 vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);

	// The figure's blit already wrote the whole swap image (transfer write); order our corner write after.
	const auto range =
		vk::ImageSubresourceRange().setAspectMask(vk::ImageAspectFlagBits::eColor).setLevelCount(1).setLayerCount(1);
	const auto wait = vk::ImageMemoryBarrier2()
						  .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
						  .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
						  .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
						  .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
						  .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
						  .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
						  .setImage(dst)
						  .setSubresourceRange(range);
	cmd.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(wait));

	const std::int32_t x1 = static_cast<std::int32_t>(extent.width) - BADGE_MARGIN;
	const std::int32_t x0 = x1 - static_cast<std::int32_t>(BADGE_W);
	const std::int32_t y0 = BADGE_MARGIN;
	const std::int32_t y1 = y0 + static_cast<std::int32_t>(BADGE_H);
	const std::array   src_offsets{
		vk::Offset3D{0, 0, 0}, vk::Offset3D{static_cast<std::int32_t>(BADGE_W), static_cast<std::int32_t>(BADGE_H), 1}};
	const std::array dst_offsets{vk::Offset3D{x0, y0, 0}, vk::Offset3D{x1, y1, 1}};
	const auto layers = vk::ImageSubresourceLayers().setAspectMask(vk::ImageAspectFlagBits::eColor).setLayerCount(1);
	const auto blit	  = vk::ImageBlit2()
							.setSrcOffsets(src_offsets)
							.setDstOffsets(dst_offsets)
							.setSrcSubresource(layers)
							.setDstSubresource(layers);
	cmd.blitImage2(vk::BlitImageInfo2()
					   .setSrcImage(src)
					   .setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal)
					   .setDstImage(dst)
					   .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal)
					   .setRegions(blit)
					   .setFilter(vk::Filter::eNearest));
}

void WindowApp::install_controls()
{
	// Capture the pointees, not `this`: the Window and Interactor live on the heap, so these pointers
	// survive a WindowApp move while `this` would dangle.
	Window*		window = m_window.get();
	Interactor* mapper = m_interactor.get();

	window->on_scroll(
		[window, mapper](double /*offset_x*/, double offset_y)
		{
			const auto cursor = window->cursor_pos();
			const auto size	  = window->window_size();
			if (size.width == 0 || size.height == 0)
			{
				return;
			}
			const glm::vec2 fraction{static_cast<float>(cursor.first) / static_cast<float>(size.width),
									 static_cast<float>(cursor.second) / static_cast<float>(size.height)};
			mapper->on_scroll(fraction, static_cast<float>(offset_y));
		});

	window->on_cursor_pos(
		[window, mapper, last_x = 0.0, last_y = 0.0](double pos_x, double pos_y) mutable
		{
			const auto size = window->window_size();
			if (window->mouse_held() && size.width != 0 && size.height != 0)
			{
				const glm::vec2 delta{static_cast<float>(pos_x - last_x) / static_cast<float>(size.width),
									  static_cast<float>(pos_y - last_y) / static_cast<float>(size.height)};
				mapper->on_drag(delta);
			}
			last_x = pos_x;
			last_y = pos_y;
		});
}

void WindowApp::rebuild_swapchain(veng::rhi::Extent2D extent)
{
	(void)m_ctx->device().waitIdle();
	if (!m_swap->rebuild(extent).has_value())
	{
		return; // a failed rebuild leaves the old swapchain in place; the next frame retries.
	}
}

bool WindowApp::draw_frame(std::size_t counter)
{
	const std::size_t slot	   = counter % FRAMES_IN_FLIGHT;
	auto			  acquired = m_swap->acquire(slot);
	if (!acquired.has_value())
	{
		return false; // hard device error.
	}
	if (!acquired->has_value())
	{
		rebuild_swapchain(m_window->framebuffer_extent()); // out of date.
		return true;
	}
	const veng::SwapchainManager::Frame frame = acquired->value();

	m_commands->reset_frame(slot); // safe: acquire() waited this slot's in-flight fence.
	auto cmd_result = m_commands->begin(veng::QueueKind::Graphics, slot);
	if (!cmd_result.has_value())
	{
		return false;
	}
	const vk::CommandBuffer	  cmd		 = cmd_result.value();
	const vk::Image			  swap_image = m_swap->image(frame.image_index);
	const veng::rhi::Extent2D extent	 = m_swap->extent(); // the acquired image's true size.

	VulkanTarget target{.command_buffer = static_cast<VkCommandBuffer>(cmd),
						.image			= static_cast<VkImage>(swap_image),
						.view			= VK_NULL_HANDLE, // render_into blits; the view is unused.
						.format			= static_cast<VkFormat>(veng::rhi::to_vk(m_swap->format())),
						.width			= extent.width,
						.height			= extent.height,
						.frame_index	= static_cast<std::uint32_t>(counter)};

	// Move the swapchain image to PRESENT. The common path is DRAWN: render_into blitted the scene in,
	// leaving the image in TRANSFER_DST. The else branch is defensive — run() only calls draw_frame
	// after needs_redraw(), so render_into should never idle here — and just makes the (untouched,
	// UNDEFINED) image legal to present rather than triggering a layout validation error.
	if (m_figure->render_into(target) == FrameStatus::DRAWN)
	{
		// Composite the fixed corner HUD onto the just-drawn frame (swap image is in TRANSFER_DST), then
		// move it to PRESENT.
		blit_overlay(static_cast<VkCommandBuffer>(cmd), static_cast<VkImage>(swap_image), extent);
		veng::CommandManager::image_barrier(cmd, swap_image, vk::ImageLayout::eTransferDstOptimal,
											vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eTransfer,
											vk::AccessFlagBits2::eTransferWrite,
											vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlagBits2::eNone);
	}
	else
	{
		veng::CommandManager::image_barrier(cmd, swap_image, vk::ImageLayout::eUndefined,
											vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eTopOfPipe,
											vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eBottomOfPipe,
											vk::AccessFlagBits2::eNone);
	}
	(void)cmd.end();

	const vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTransfer;
	const auto					 submit		= vk::SubmitInfo()
												  .setWaitSemaphores(frame.image_available)
												  .setWaitDstStageMask(wait_stage)
												  .setCommandBuffers(cmd)
												  .setSignalSemaphores(frame.render_finished);
	if (m_ctx->graphics_queue().submit(submit, frame.in_flight) != vk::Result::eSuccess)
	{
		return false;
	}

	auto presented =
		m_swap->present(m_ctx->graphics_queue(), frame.image_index, m_swap->render_finished_handle(frame.image_index));
	if (!presented.has_value())
	{
		return false;
	}
	if (presented.value()) // out of date / suboptimal.
	{
		rebuild_swapchain(m_window->framebuffer_extent());
	}
	return true;
}

void WindowApp::run(const std::function<void(double elapsed_seconds)>& tick)
{
	const auto	start	= std::chrono::steady_clock::now();
	std::size_t counter = 0;

	while (!m_window->should_close())
	{
		Window::poll();
		if (tick)
		{
			const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
			tick(elapsed.count());
		}

		const veng::rhi::Extent2D fb_extent = m_window->framebuffer_extent();
		if (fb_extent.width == 0 || fb_extent.height == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_SLEEP_MS)); // minimized window.
			continue;
		}
		if (fb_extent.width != m_swap->extent().width || fb_extent.height != m_swap->extent().height)
		{
			rebuild_swapchain(fb_extent);
		}

		// On-demand: resolve before acquiring. If nothing changed, skip the frame entirely and leave the
		// last presented image on screen — acquiring then presenting an un-redrawn swapchain image is the
		// multi-buffer trap (it would be in UNDEFINED layout). This mirrors veng's FrameExecutor idle.
		const veng::rhi::Extent2D swap_extent = m_swap->extent();
		if (!m_figure->needs_redraw(swap_extent.width, swap_extent.height))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_SLEEP_MS));
			continue;
		}

		if (!draw_frame(counter))
		{
			break;
		}
		++counter;
	}

	(void)m_ctx->device().waitIdle();
}
} // namespace geng

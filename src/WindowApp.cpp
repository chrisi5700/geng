// geng::WindowApp — the GLFW windowed front-end. It plays the host role (owns the window, device,
// swapchain, and presentation) and drives a Figure through Figure::render_into, exactly as a
// QVulkanWindow would. The per-frame contract: acquire a swapchain image, hand the figure a begun
// command buffer to record its scene + blit into, transition the image to PRESENT, submit, present.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <geng/WindowApp.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <thread>
#include <utility>
#include <veng/context/Context.hpp>
#include <veng/managers/CommandManager.hpp>
#include <veng/managers/QueueKind.hpp>
#include <veng/managers/SwapchainManager.hpp>
#include <veng/rhi/Convert.hpp>

namespace geng
{
namespace
{
/// N-buffering depth. Must match the Figure's own internal depth so its ResourcePool slot (chosen
/// as frame_index % N inside render_into) stays in step with the swapchain slot used here.
constexpr std::size_t FRAMES_IN_FLIGHT = 2;
constexpr int		  IDLE_SLEEP_MS	   = 10;
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

	// This app owns its device (it is the host). No extra shader paths: the host context never runs the
	// graph — the figure's adopted context, built below, carries geng's shaders.
	auto ctx_result =
		veng::Context::create("geng-window", app.m_window->required_extensions(),
							  [&app](VkInstance instance) { return app.m_window->create_surface(instance); }, {});
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

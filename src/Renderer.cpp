#include <array>
#include <chrono>
#include <functional>
#include <geng/Renderer.hpp>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <veng/context/Context.hpp>
#include <veng/gpu/ImageRef.hpp>
#include <veng/managers/CommandManager.hpp>
#include <veng/managers/FrameExecutor.hpp>
#include <veng/managers/SwapchainManager.hpp>
#include <veng/nodes/BlitNode.hpp>
#include <veng/nodes/PresentNode.hpp>
#include <veng/rendergraph/data/Data.hpp>
#include <veng/resources/ResourcePool.hpp>

namespace geng
{
namespace
{
constexpr std::size_t FRAMES_IN_FLIGHT = 2;
} // namespace

Renderer::Renderer(const char* title, int width, int height)
	: m_window(title, width, height)
{
	const std::array<std::string_view, 1> shader_paths{GENG_SHADER_DIR};

	auto ctx_result = veng::Context::create(
		"geng", m_window.required_extensions(),
		[this](VkInstance instance) { return m_window.create_surface(instance); }, shader_paths);
	if (!ctx_result.has_value())
	{
		throw std::runtime_error("geng::Renderer: failed to create the Vulkan context");
	}
	m_ctx = std::make_unique<veng::Context>(std::move(ctx_result.value()));

	auto swap_result = veng::SwapchainManager::create(*m_ctx, m_window.framebuffer_extent(), FRAMES_IN_FLIGHT);
	if (!swap_result.has_value())
	{
		throw std::runtime_error("geng::Renderer: failed to create the swapchain");
	}
	m_swap = std::make_unique<veng::SwapchainManager>(std::move(swap_result.value()));

	m_pool = std::make_unique<veng::ResourcePool>(m_ctx->device(), m_ctx->rhi(), m_ctx->allocator(), FRAMES_IN_FLIGHT);
	m_commands			 = std::make_unique<veng::CommandManager>(*m_ctx);
	m_scene_color_format = m_swap->format();

	using namespace veng::graph;

	m_screen		  = m_graph.add_source<veng::rhi::Extent2D>(m_swap->extent());
	m_swapchain_image = m_graph.add_source<veng::gpu::ImageRef>(veng::gpu::ImageRef{});
	m_scene_image	  = m_graph.add(std::make_unique<ValueData<veng::gpu::ImageRef>>(veng::gpu::ImageRef{}));
	m_presented_image = m_graph.add(std::make_unique<ValueData<veng::gpu::ImageRef>>(veng::gpu::ImageRef{}));
	m_frame_done	  = m_graph.add(std::make_unique<ValueData<int>>(0));
	m_sinks.push_back(m_frame_done);

	auto blit = std::make_unique<veng::nodes::BlitNode>(m_scene_image, m_swapchain_image, m_presented_image,
														veng::rhi::TextureUsage::PRESENT);
	m_graph.set_producer(m_presented_image, m_graph.add(std::move(blit)));

	auto present = std::make_unique<veng::nodes::PresentNode>(*m_swap, m_presented_image, m_frame_done);
	m_present	 = present.get();
	m_graph.set_producer(m_frame_done, m_graph.add(std::move(present)));

	m_executor = std::make_unique<veng::FrameExecutor>(*m_ctx, *m_swap, *m_pool, *m_commands, m_scheduler,
													   m_swapchain_image, FRAMES_IN_FLIGHT);
}

Renderer::~Renderer()
{
	if (m_ctx)
	{
		(void)m_ctx->device().waitIdle();
	}
}

veng::Context& Renderer::context() noexcept
{
	return *m_ctx;
}

void Renderer::rebuild_swapchain(veng::rhi::Extent2D extent)
{
	(void)m_ctx->device().waitIdle();
	if (!m_swap->rebuild(extent).has_value())
	{
		return;
	}
	const std::scoped_lock lock(m_graph_mutex);
	m_graph.set(m_screen, m_swap->extent());
}

void Renderer::run(const std::function<void()>& tick)
{
	while (!m_window.should_close())
	{
		Window::poll();
		if (tick)
		{
			tick();
		}

		const veng::rhi::Extent2D fb_extent = m_window.framebuffer_extent();
		if (fb_extent.width == 0 || fb_extent.height == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}
		if (fb_extent.width != m_swap->extent().width || fb_extent.height != m_swap->extent().height)
		{
			rebuild_swapchain(fb_extent);
		}

		auto outcome = m_executor->run_frame(m_graph, m_sinks, veng::FrameExecutor::Pacing::OnDemand, &m_graph_mutex);

		if (outcome.status == veng::FrameExecutor::Status::Idled)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
			continue;
		}
		if (outcome.status == veng::FrameExecutor::Status::OutOfDate)
		{
			rebuild_swapchain(m_window.framebuffer_extent());
			continue;
		}
		if (outcome.status == veng::FrameExecutor::Status::NodeFailed)
		{
			continue;
		}
		if (outcome.status == veng::FrameExecutor::Status::AcquireFailed)
		{
			break;
		}

		if (m_present->out_of_date())
		{
			rebuild_swapchain(m_window.framebuffer_extent());
		}
	}

	(void)m_ctx->device().waitIdle();
}
} // namespace geng

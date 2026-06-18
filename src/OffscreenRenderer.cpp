#include <geng/OffscreenRenderer.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <veng/context/Context.hpp>
#include <veng/managers/CommandManager.hpp>
#include <veng/managers/HeadlessExecutor.hpp>
#include <veng/nodes/ScreenshotNode.hpp>
#include <veng/rendergraph/data/Data.hpp>
#include <veng/resources/ResourcePool.hpp>

namespace geng
{
OffscreenRenderer::OffscreenRenderer(std::uint32_t width, std::uint32_t height)
	: m_extent{.width = width, .height = height}
{
	const std::array<std::string_view, 1> shader_paths{GENG_SHADER_DIR};

	auto ctx_result = veng::Context::create("geng", {}, {}, shader_paths);
	if (!ctx_result.has_value())
	{
		throw std::runtime_error("geng::OffscreenRenderer: failed to create the headless Vulkan context");
	}
	m_ctx = std::make_unique<veng::Context>(std::move(ctx_result.value()));

	m_pool	   = std::make_unique<veng::ResourcePool>(m_ctx->device(), m_ctx->rhi(), m_ctx->allocator(), 1);
	m_commands = std::make_unique<veng::CommandManager>(*m_ctx);
	m_executor = std::make_unique<veng::HeadlessExecutor>(*m_ctx, *m_pool, *m_commands, m_scheduler);

	m_screen	  = m_graph.add_source<veng::rhi::Extent2D>(m_extent);
	m_scene_image = m_graph.add(std::make_unique<veng::graph::ValueData<veng::gpu::ImageRef>>(veng::gpu::ImageRef{}));
}

OffscreenRenderer::~OffscreenRenderer()
{
	if (m_ctx)
	{
		(void)m_ctx->device().waitIdle();
	}
}

bool OffscreenRenderer::capture_png(const std::string& png_path)
{
	using namespace veng::graph;

	// A done-token sink fed by a ScreenshotNode that reads back m_scene_image and writes the PNG
	// itself (veng's StbWriteImpl). HeadlessExecutor::run_once records, submits, and blocks on the
	// fence — firing the screenshot's on_retired hook so the file is on disk before it returns.
	const DataHandle shot_done = m_graph.add(std::make_unique<ValueData<int>>(0));
	m_graph.set_producer(shot_done, m_graph.add(std::make_unique<veng::nodes::ScreenshotNode>(
										m_scene_image, shot_done, png_path, veng::nodes::ImageFormat::Png)));

	return m_executor->run_once(m_graph, std::array{shot_done}).has_value();
}
} // namespace geng

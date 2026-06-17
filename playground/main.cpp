// geng first slice: plot sin(x) as a thin-quad polyline through the veng render graph.
//
// The whole scene is three nodes:
//   * StorageBufferNode  — uploads the sampled (x, sin x) points into an SSBO
//   * a transform        — combines the data→clip matrix with the live framebuffer size into
//                          the line shader's push constants (so a resize recomputes reactively)
//   * GraphicsNode       — an instanced, mesh-free draw (6 verts × segment instances) that
//                          expands each segment into a screen-space-thick quad in the vertex
//                          shader (shaders/line.{vert,frag}.slang)
//
// The graph wiring lives here, in the playground, until the public Figure/Plot API takes shape.

#include <cmath>
#include <cstddef>
#include <exception>
#include <memory>
#include <numbers>
#include <print>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <veng/gpu/BufferRef.hpp>
#include <veng/nodes/GraphicsNode.hpp>
#include <veng/nodes/StorageBufferNode.hpp>
#include <veng/rendergraph/data/Data.hpp>
#include <veng/rhi/Enums.hpp>

#include <geng/Bounds2D.hpp>
#include <geng/Renderer.hpp>

namespace
{
/// Matches `PushData` in shaders/line.vert.slang (std430: mat4 @0, vec2 @64, float @72).
struct LineUniforms
{
	glm::mat4 view_proj;
	glm::vec2 extent;
	float	  half_width = 0.0F;

	friend bool operator==(const LineUniforms&, const LineUniforms&) noexcept = default;
};

constexpr float		  HALF_WIDTH_PX = 1.5F;
constexpr std::size_t SAMPLE_COUNT	= 256;

std::vector<glm::vec2> sample_sin(const geng::Bounds2D& bounds)
{
	std::vector<glm::vec2> points;
	points.reserve(SAMPLE_COUNT);
	for (std::size_t idx = 0; idx < SAMPLE_COUNT; ++idx)
	{
		const float frac  = static_cast<float>(idx) / static_cast<float>(SAMPLE_COUNT - 1);
		const float pos_x = bounds.min_x + (frac * (bounds.max_x - bounds.min_x));
		points.emplace_back(pos_x, std::sin(pos_x));
	}
	return points;
}
} // namespace

int main()
{
	using namespace veng;
	using namespace veng::graph;

	constexpr float		 X_LIMIT = 2.0F * std::numbers::pi_v<float>;
	const geng::Bounds2D bounds{.min_x = -X_LIMIT, .max_x = X_LIMIT, .min_y = -1.5F, .max_y = 1.5F};
	const std::vector<glm::vec2> points = sample_sin(bounds);

	try
	{
		geng::Renderer renderer("geng — sin(x)", 1280, 720);
		Graph&		   graph = renderer.graph();

		// Points → SSBO. The published ref's count also drives the instanced draw size.
		const auto		 points_src = graph.add_source<std::vector<glm::vec2>>(points);
		const DataHandle points_ref = graph.add(std::make_unique<ValueData<gpu::BufferRef>>(gpu::BufferRef{}));
		graph.set_producer(points_ref,
						   graph.add(std::make_unique<nodes::StorageBufferNode>(points_src, "points", points_ref)));

		// Push constants: the static data→clip matrix combined with the live framebuffer size,
		// so a window resize re-derives them (and, later, pan/zoom is just a new view_proj).
		const auto view_proj_src = graph.add_source<glm::mat4>(geng::ortho_view(bounds));
		const auto uniforms		 = graph.add_transform(
			[](const glm::mat4& view_proj, const rhi::Extent2D& extent)
			{
				return LineUniforms{.view_proj = view_proj,
									.extent		= glm::vec2(static_cast<float>(extent.width),
													static_cast<float>(extent.height)),
									.half_width = HALF_WIDTH_PX};
			},
			view_proj_src, renderer.screen());

		// Mesh-free instanced draw: 6 vertices per segment, segment count from the SSBO.
		auto curve = std::make_unique<nodes::GraphicsNode>("line.vert", "line.frag", renderer.scene_color_format(),
														   rhi::Format::UNDEFINED, 6, renderer.screen(),
														   renderer.scene_image());
		curve->add_storage_buffer(points_ref)
			.set_instances_from(points_ref)
			.push_constant<LineUniforms>(uniforms, rhi::ShaderStage::VERTEX)
			.clear_color({0.06F, 0.06F, 0.09F, 1.0F});
		graph.set_producer(renderer.scene_image(), graph.add(std::move(curve)));

		renderer.run();
	}
	catch (const std::exception& error)
	{
		std::println("geng demo failed: {}", error.what());
		return 1;
	}
	return 0;
}

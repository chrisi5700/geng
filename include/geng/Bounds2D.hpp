#ifndef GENG_BOUNDS2D_HPP
#define GENG_BOUNDS2D_HPP

#include <glm/mat4x4.hpp>

namespace geng
{
/// An axis-aligned data-space rectangle: the region of the plane a plot maps onto the screen.
struct Bounds2D
{
	float min_x;
	float max_x;
	float min_y;
	float max_y;
};

/// Build the data-space → Vulkan-clip-space transform that maps @p bounds onto the full clip
/// volume, with +Y pointing up on screen (Vulkan's clip Y points down, so the mapping is
/// flipped). Multiply a `vec4(x, y, 0, 1)` data position by the result to get clip space.
[[nodiscard]] glm::mat4 ortho_view(const Bounds2D& bounds);
} // namespace geng

#endif // GENG_BOUNDS2D_HPP
